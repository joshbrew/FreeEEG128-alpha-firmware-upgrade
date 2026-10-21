import contextlib
import io
import json
from pathlib import Path
import sys
import tempfile
import types
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import board_tools as tools
import device
from test_device import Clock


def frame(sequence):
    return bytes((0xa0, sequence % 256)) + bytes(390) + b'\xc0'


class Connection:
    def __init__(self, hw):
        self.hw = hw
        self.buffer = bytearray()

    def __enter__(self): return self
    def __exit__(self, *args): pass
    def reset_input_buffer(self): self.buffer.clear()
    def flush(self): pass

    def pump(self):
        h = self.hw
        if not h.streaming or h.silent:
            return
        target = int((h.clock.value - h.started) * h.rate)
        while h.produced < target:
            data = frame(h.sequence)
            h.sequence = (h.sequence + 1) % 256
            h.produced += 1
            h.acquired += 1
            if h.drop_every and h.acquired % h.drop_every == 0:
                h.dropped += 1
            else:
                self.buffer.extend(data)

    @property
    def in_waiting(self):
        self.pump()
        return len(self.buffer)

    def read(self, count):
        self.hw.clock.sleep(0.005 if self.hw.streaming else 0.001)
        self.pump()
        data = self.buffer[:count]
        del self.buffer[:count]
        return data

    def write(self, data):
        command = data.decode().strip().removeprefix('!')
        h = self.hw
        h.commands.append(command)
        word = command.split()[0]
        if command == h.fail_command:
            reply = 'ERR GAIN SPI/readback; stream stopped'
        elif word == 'STOP':
            self.pump()
            previous = h.streaming
            h.streaming = False
            reply = 'OK STOP' + (f' was_streaming={int(previous)}' if h.prior_state_supported else '')
        elif word == 'START':
            h.streaming = True
            h.started, h.produced = h.clock.value, 0
            reply = 'OK START'
        elif word == 'INFO':
            reply = 'OK INFO FreeEEG128-alpha qos-5 cpu_mhz=80 update=UF2 watchdog=IWDG1 build=FreeEEG128-build:' + '0' * 32
        elif word == 'SETTINGS':
            rate, gains = (500, [8] * 128) if h.bad_readback else (h.rate, h.gains)
            reply = f'OK SETTINGS rate={rate} source=defaults dirty={h.dirty} gains=' + ','.join(map(str, gains))
        elif word == 'RATE':
            h.rate = int(command.split()[1]); h.dirty = 1
            reply = 'OK ' + command
        elif word == 'GAIN':
            _, channel, gain = command.split()
            h.gains[int(channel) - 1] = int(gain); h.dirty = 1
            reply = 'OK ' + command
        elif word == 'SAVE':
            h.dirty = 0; reply = 'OK SAVE'
        elif word == 'CHECK':
            reply = f'OK CHECK adc_ok_mask=0x{65535 & ~h.failed_adc:x} adc_failed_mask=0x{h.failed_adc:x} adc_fault={int(bool(h.failed_adc))}'
        elif word == 'STATUS':
            reply = f'OK STATUS rate={h.rate} acquired={h.acquired} missed=0 dma_errors=0 usb_dropped={h.dropped} rx_dropped=0 adc_fault={int(bool(h.failed_adc))} uptime_ms={int(h.clock.value * 1000)}'
        elif word == 'DIAG':
            reply = 'OK DIAG capture_millihz=500000 window_us=1000000 reset_flags=0x04000000'
        else:
            reply = 'OK ' + word
        self.buffer.extend(reply.encode() + b'\r\n')


class Hardware:
    def __init__(self, root):
        self.root, self.clock = root, Clock()
        self.rate, self.gains = 500, [8] * 128
        self.dirty = 0
        self.streaming, self.started, self.produced = True, 0, 0
        self.sequence = self.acquired = self.dropped = 0
        self.drop_every = self.failed_adc = 0
        self.silent = self.bad_readback = False
        self.prior_state_supported = True
        self.fail_command = None
        self.commands = []
        self.port = device.Port('COM5', 'board-A')
        self.connection = Connection(self)

    @contextlib.contextmanager
    def installed(self):
        with patch.object(tools, 'select_target', return_value=device.Target(port=self.port)), \
             patch.object(tools, 'open_port', return_value=self.connection), \
             patch.object(device, 'select_target', return_value=device.Target(port=self.port)), \
             patch.object(device, 'open_port', return_value=self.connection), \
             patch.object(tools, 'report_folder', return_value=self.root / 'reports'), \
             patch.object(tools, 'tool_versions', return_value={'python': 'test'}), \
             patch.object(device.time, 'monotonic', self.clock.now), patch.object(device.time, 'sleep', self.clock.sleep), \
             contextlib.redirect_stdout(io.StringIO()):
            yield


def args(action, **options):
    values = dict(action=action, port='COM5', advanced=False, output=None, file=None, yes=False, save=False, seconds=10)
    values.update(options)
    return types.SimpleNamespace(**values)


def backup(path, settings=None, **overrides):
    document = {'format': 'freeeeg128-settings', 'version': 1, 'board': device.BOARD_ID,
                'settings': settings or {'rate': 1000, 'gains': [4] * 128}}
    document.update(overrides)
    path.write_text(json.dumps(document))
    return path


class BoardToolsTests(unittest.TestCase):
    def test_frame_reader_handles_fragments_counter_wrap_and_gaps(self):
        reader = tools.FrameReader()
        data = b'noise' + b''.join(frame(n) for n in (254, 255, 256, 258, 258, 259))
        for start in range(0, len(data), 31): reader.feed(data[start:start + 31])
        self.assertEqual((reader.frames, reader.missing, reader.duplicates), (6, 1, 1))
        self.assertEqual(reader.discarded, 5)

    def test_frame_reader_recovers_from_malformed_frame(self):
        reader = tools.FrameReader()
        reader.feed(frame(0) + frame(1) + frame(2)[:-1] + b'X' + frame(3) + frame(4) + frame(5))
        self.assertEqual(reader.frames, 5)
        self.assertEqual(reader.malformed, 1)
        self.assertEqual(reader.missing, 1)

    def test_board_check_reports_adc_and_restores_previous_state(self):
        for running in (True, False):
            with self.subTest(running=running), tempfile.TemporaryDirectory() as folder:
                h = Hardware(Path(folder)); h.streaming = running
                with h.installed(): self.assertEqual(tools.run(args('check')), 0)
                self.assertEqual(h.streaming, running)
                report = json.loads(next((h.root / 'reports').glob('*.json')).read_text())
                self.assertTrue(report['board_check']['passed'])
                self.assertTrue(report['commands'])
                self.assertIn('CHECK', h.commands)
                self.assertFalse(any(cmd.startswith(('RATE', 'GAIN', 'SAVE')) for cmd in h.commands))

    def test_adc_failure_is_not_reported_as_pass_and_stays_paused(self):
        with tempfile.TemporaryDirectory() as folder:
            h = Hardware(Path(folder)); h.failed_adc = 4
            with h.installed(): self.assertEqual(tools.run(args('check')), 1)
            self.assertFalse(h.streaming)
            self.assertNotIn('START', h.commands)

    def test_recording_success_and_transport_failure(self):
        for dropping in (False, True):
            with self.subTest(dropping=dropping), tempfile.TemporaryDirectory() as folder:
                h = Hardware(Path(folder)); h.drop_every = 300 if dropping else 0
                with h.installed(): self.assertEqual(tools.run(args('record')), int(dropping))
                report = json.loads(next((h.root / 'reports').glob('*.json')).read_text())['recording']
                self.assertEqual(report['passed'], not dropping)
                self.assertAlmostEqual(report['measured_hz'], 500, delta=11)
                self.assertEqual(report['counter_changes']['usb_dropped'] > 0, dropping)
                self.assertTrue(h.streaming)

    def test_no_data_fails_and_original_paused_state_is_preserved(self):
        with tempfile.TemporaryDirectory() as folder:
            h = Hardware(Path(folder)); h.silent = True; h.streaming = False
            with h.installed(): self.assertEqual(tools.run(args('record', seconds=2)), 1)
            self.assertFalse(h.streaming)

    def test_recording_rejects_invalid_duration_before_opening_device(self):
        for duration in (0, 1, 61, float('nan'), float('inf')):
            with patch.object(tools, 'session') as session:
                with self.assertRaises(device.DeviceError): tools.run(args('record', seconds=duration))
                session.assert_not_called()

    def test_backup_round_trip_preserves_all_channel_gains(self):
        with tempfile.TemporaryDirectory() as folder:
            h = Hardware(Path(folder)); h.gains = [1, 2, 4, 8, 16, 32, 64, 128] * 16
            with h.installed(): self.assertEqual(tools.run(args('backup')), 0)
            path = next((h.root / 'reports').glob('*.json'))
            self.assertEqual(tools.read_backup(path), {'rate': 500, 'gains': h.gains})
            original = path.read_bytes()
            with h.installed(), self.assertRaises(FileExistsError): tools.run(args('backup', output=str(path)))
            self.assertEqual(path.read_bytes(), original)

    def test_invalid_backups_never_open_a_board(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'settings.json'
            variants = [({'rate': True, 'gains': [8] * 128}, {}), ({'rate': 500, 'gains': [8] * 31}, {}),
                        ({'rate': 256, 'gains': [8] * 128}, {}), ({'rate': 500, 'gains': [True] * 32}, {}),
                        (None, {'board': 'another-board'}), (None, {'version': True})]
            for settings, overrides in variants:
                backup(path, settings, **overrides)
                with patch.object(tools, 'session') as session:
                    with self.assertRaises(device.DeviceError): tools.run(args('restore', file=str(path), yes=True))
                    session.assert_not_called()
            path.write_text('{"version":1,"version":2}')
            with self.assertRaises(device.DeviceError): tools.read_backup(path)

    def test_restore_applies_and_verifies_then_saves_only_when_requested(self):
        for save in (False, True):
            with self.subTest(save=save), tempfile.TemporaryDirectory() as folder:
                h = Hardware(Path(folder)); path = backup(h.root / 'settings.json')
                with h.installed(): self.assertEqual(tools.run(args('restore', file=str(path), yes=True, save=save)), 0)
                self.assertEqual(h.rate, 1000); self.assertEqual(h.gains, [4] * 128)
                self.assertEqual('SAVE' in h.commands, save)
                self.assertTrue(h.streaming)

    def test_cancelled_restore_does_not_change_settings(self):
        with tempfile.TemporaryDirectory() as folder:
            h = Hardware(Path(folder))
            with h.installed():
                self.assertFalse(tools.restore_connection(h.connection, {'rate': 1000, 'gains': [4] * 128}, input_fn=lambda _: '0'))
            self.assertEqual(h.commands, ['SETTINGS'])

    def test_partial_restore_error_and_failed_readback_do_not_save_or_resume(self):
        for mode in ('write', 'readback'):
            with self.subTest(mode=mode), tempfile.TemporaryDirectory() as folder:
                h = Hardware(Path(folder)); path = backup(h.root / 'settings.json')
                h.fail_command = 'GAIN 2 4' if mode == 'write' else None
                h.bad_readback = mode == 'readback'
                with h.installed(), self.assertRaises(device.DeviceError): tools.run(args('restore', file=str(path), yes=True, save=True))
                self.assertNotIn('SAVE', h.commands)
                self.assertNotIn('START', h.commands)
                self.assertFalse(h.streaming)
                self.assertTrue(list((h.root / 'reports').glob('*error*.json')))

    def test_unsaved_configuration_exit_requires_a_choice(self):
        with tempfile.TemporaryDirectory() as folder:
            h = Hardware(Path(folder))
            responses = iter(['1', '1000', '8', 'C', '0', 'K'])
            with h.installed(): device.configure('COM5', input_fn=lambda _: next(responses))
            self.assertEqual(h.rate, 1000)
            self.assertNotIn('SAVE', h.commands)
            self.assertNotIn('START', h.commands)
            self.assertFalse(h.streaming)

    def test_old_stop_reply_leaves_state_unknown_and_paused(self):
        with tempfile.TemporaryDirectory() as folder:
            h = Hardware(Path(folder)); h.prior_state_supported = False
            with h.installed(): tools.run(args('backup'))
            self.assertFalse(h.streaming)
            self.assertNotIn('START', h.commands)

    def test_resume_failure_does_not_return_a_success_exit_code(self):
        with tempfile.TemporaryDirectory() as folder:
            h = Hardware(Path(folder)); h.fail_command = 'START'
            with h.installed(), self.assertRaisesRegex(device.DeviceError, 'streaming could not resume'):
                tools.run(args('check'))
            self.assertFalse(h.streaming)

    def test_reset_reason_and_counter_wrap(self):
        self.assertIn('watchdog recovery', tools.reset_reason(0x04000000))
        self.assertEqual(tools.reset_reason(0x00e00000), 'power-on')
        self.assertEqual(tools.counter_delta({'usb_dropped': '4294967295'}, {'usb_dropped': '2'}, 'usb_dropped'), 3)


if __name__ == '__main__': unittest.main()
