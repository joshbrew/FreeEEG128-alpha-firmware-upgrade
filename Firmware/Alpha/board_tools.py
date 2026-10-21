"""Board checks, USB recording checks, settings files and local support reports."""
from contextlib import contextmanager
from datetime import datetime, timezone
import importlib.metadata
import json
import math
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

from device import (BOARD_ID, DeviceError, fields, friendly_error, identify, info_summary,
                    open_port, select_target, transact)


def timestamp():
    return datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')


def report_folder():
    here = Path(__file__).resolve().parent
    root = here.parent.parent if here.name == 'Alpha' and here.parent.name == 'Firmware' else here
    return root / 'reports'


def write_json(data, output=None, kind='report'):
    path = Path(output).expanduser() if output else report_folder() / f'freeeeg128-{kind}-{timestamp()}.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    # Preserve a user's earlier backup/report, including when an explicit name is reused.
    with path.open('x', encoding='utf-8') as stream:
        json.dump(data, stream, indent=2, allow_nan=False)
        stream.write('\n')
    return path.resolve()


def failure_report(error, connection=None, info=None, port=None):
    if getattr(error, '_freeeeg128_reported', False):
        return
    try:
        path = write_json({'format': 'freeeeg128-error', 'version': 1, 'created_utc': timestamp(),
                           'error': str(error), 'guidance': friendly_error(error), 'info': info,
                           'port': port.device if port else None,
                           'commands': getattr(connection, '_freeeeg128_log', [])}, kind='error')
        error._freeeeg128_reported = True
        print(f'Details saved: {path}')
    except (OSError, ValueError):
        print('Could not save the error report. Check write permissions for the reports folder.')


def integer(values, key, default=0):
    try:
        return int(values.get(key, default), 0) if isinstance(values.get(key, default), str) else int(values.get(key, default))
    except (ValueError, TypeError):
        raise DeviceError(f'Invalid {key} in the board reply.')


def reset_reason(flags):
    for bit, label in ((26, 'watchdog recovery'), (28, 'window-watchdog recovery'),
                       (24, 'software restart'), (23, 'power-on'), (21, 'low-voltage reset'),
                       (22, 'RESET pin/button'), (30, 'low-power reset')):
        if flags & (1 << bit):
            return label
    return 'not reported'


def status_summary(status, diag):
    s, d = fields(status), fields(diag)
    lines = [f'Sample rate: {s.get("rate", "?")} Hz',
             'ADC settings: ' + ('fault detected; run Check my board' if integer(s, 'adc_fault') else 'no latched fault'),
             f'USB frames dropped since boot: {s.get("usb_dropped", "?")}',
             f'Missed sample events: {s.get("missed", "?")}; DMA errors: {s.get("dma_errors", "?")}',
             f'Last reset: {reset_reason(integer(d, "reset_flags"))}']
    if integer(d, 'window_us'):
        lines.append(f'Last active capture rate: {integer(d, "capture_millihz") / 1000:.2f} Hz')
    else:
        lines.append('Capture-rate measurement: no completed streaming window yet')
    adc = integer(s, 'last_adc')
    if adc:
        lines.append(f'Last ADC fault: ADC {adc}, register {s.get("last_reg", "?")}, operation {s.get("last_op", "?")}')
    return '\n'.join(lines)


@contextmanager
def session(port=None, advanced=False):
    target = select_target(port, include_loader=False)
    print(f'Selected {target.label()}')
    with open_port(target.port) as connection:
        info = identify(connection)
        print(info if advanced else info_summary(info))
        try:
            yield connection, info, target.port
        except (DeviceError, OSError, TimeoutError) as exc:
            failure_report(exc, connection, info, target.port)
            raise
        finally:
            primary_error = sys.exc_info()[0] is not None
            if not getattr(connection, '_freeeeg128_resume_allowed', True):
                print('Streaming paused. Check the board or reload saved settings before restarting.')
            elif getattr(connection, '_freeeeg128_was_streaming', None) is True:
                try:
                    transact(connection, 'START')
                    print('Previous streaming state restored: running.')
                except (DeviceError, OSError, TimeoutError) as exc:
                    print('Streaming could not resume: ' + friendly_error(exc))
                    failure_report(exc, connection, info, target.port)
                    if not primary_error:
                        raise DeviceError('The check finished, but streaming could not resume. ' + friendly_error(exc)) from exc
            else:
                print('Streaming paused. Use Configure board to start it.' if getattr(connection, '_freeeeg128_was_streaming', None) is None
                      else 'Previous streaming state restored: paused.')


def settings_from_reply(reply):
    values = fields(reply)
    try:
        result = {'rate': int(values['rate']), 'gains': [int(v) for v in values['gains'].split(',')]}
        validate_settings(result)
        return result
    except (KeyError, ValueError, TypeError, DeviceError) as exc:
        raise DeviceError('The board returned incomplete or invalid settings. Run Check my board and check the firmware version.') from exc


def validate_settings(settings):
    if not isinstance(settings, dict) or set(settings) != {'rate', 'gains'}:
        raise DeviceError('Settings must contain a sample rate and exactly 128 gains.')
    rate, gains = settings['rate'], settings['gains']
    if type(rate) is not int or rate not in (250, 500, 1000, 2000):
        raise DeviceError('Settings sample rate must be 250, 500, 1000 or 2000 Hz.')
    if not isinstance(gains, list) or len(gains) != 128 or any(type(g) is not int or g not in (1, 2, 4, 8, 16, 32, 64, 128) for g in gains):
        raise DeviceError('Settings need 128 channel gains, each a power of two from 1 through 128.')


def read_backup(path):
    source = Path(path).expanduser()
    if not source.is_file() or source.stat().st_size > 65536:
        raise DeviceError('Choose an existing FreeEEG128 settings JSON file, at most 64 KB.')

    def unique_keys(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError('duplicate JSON key')
            result[key] = value
        return result

    try:
        document = json.loads(source.read_text(encoding='utf-8-sig'), object_pairs_hook=unique_keys)
        if (not isinstance(document, dict) or document.get('format') != 'freeeeg128-settings' or
                type(document.get('version')) is not int or document['version'] != 1 or document.get('board') != BOARD_ID):
            raise ValueError('board or format mismatch')
        settings = document['settings']
        validate_settings(settings)
        return settings
    except (ValueError, KeyError, TypeError, UnicodeError, RecursionError) as exc:
        raise DeviceError('The file is not a supported settings backup for this board.') from exc


def backup_connection(connection, info, output=None):
    settings = settings_from_reply(transact(connection, 'SETTINGS'))
    path = write_json({'format': 'freeeeg128-settings', 'version': 1, 'board': BOARD_ID,
                       'created_utc': timestamp(), 'firmware': info, 'settings': settings}, output, 'settings')
    print(f'Settings backed up: {path}')
    return path


def restore_connection(connection, settings, yes=False, save=False, input_fn=input):
    validate_settings(settings)
    current = settings_from_reply(transact(connection, 'SETTINGS'))
    print(f'Sample rate: {current["rate"]} -> {settings["rate"]} Hz')
    changes = [(i + 1, old, new) for i, (old, new) in enumerate(zip(current['gains'], settings['gains'])) if old != new]
    print('Gain changes: ' + (', '.join(f'ch {i}: {old} -> {new}' for i, old, new in changes) or 'none'))
    if not yes:
        if input_fn is input and not sys.stdin.isatty():
            raise DeviceError('Review the settings interactively, or pass --yes to apply them (--save also persists them).')
        choice = input_fn('1 Apply for now, 2 Apply and save for power-up, 0 Cancel [0]: ').strip()
        if choice not in ('1', '2'):
            print('Restore cancelled. No settings changed.')
            return False
        save = choice == '2'
    # Do not resume a partially applied configuration if any command fails.
    connection._freeeeg128_resume_allowed = False
    if current['rate'] != settings['rate']:
        transact(connection, f'RATE {settings["rate"]}')
    for channel, _, gain in changes:
        transact(connection, f'GAIN {channel} {gain}')
    if settings_from_reply(transact(connection, 'SETTINGS')) != settings:
        raise DeviceError('Restored settings did not match readback. Streaming is paused; reload saved settings or reboot.')
    if save:
        transact(connection, 'SAVE')
    connection._freeeeg128_resume_allowed = True
    print('Settings restored and saved for power-up.' if save else 'Settings restored in RAM. Save them to keep them after power-up.')
    return True


def check_connection(connection, info, advanced=False):
    settings = transact(connection, 'SETTINGS')
    before = transact(connection, 'STATUS')
    check = transact(connection, 'CHECK')
    status = transact(connection, 'STATUS')
    diag = transact(connection, 'DIAG')
    values = fields(check)
    failed = integer(values, 'adc_failed_mask')
    known = 'adc_ok_mask' in values and 'adc_failed_mask' in values
    passed = known and integer(values, 'adc_ok_mask') == 65535 and failed == 0 and not integer(values, 'adc_fault') and not integer(fields(status), 'adc_fault')
    print('USB connection and firmware commands: passed')
    for adc in range(4):
        good = known and bool(integer(values, 'adc_ok_mask') & (1 << adc))
        print(f'ADC {adc + 1} settings readback: ' + ('passed' if good else 'failed; check board power/SPI connections'))
    print(status_summary(status, diag))
    print('Board configuration check passed.' if passed else 'Board check found a problem. Correct it and reboot before recording.')
    print('Use Test recording to check live acquisition and USB delivery.')
    if advanced:
        for reply in (info, settings, before, check, status, diag):
            print(reply)
    if not passed:
        connection._freeeeg128_resume_allowed = False
    return {'passed': passed, 'info': info, 'settings': settings, 'status_before': before,
            'adc_check': check, 'status': status, 'diagnostics': diag}


class FrameReader:
    """Reassemble the existing 393-byte EEG framing across arbitrary USB reads."""
    def __init__(self):
        self.buffer = bytearray()
        self.locked = False
        self.frames = self.missing = self.duplicates = self.malformed = self.discontinuities = self.discarded = 0
        self.last_sequence = None

    @staticmethod
    def frame_at(data, offset):
        return data[offset] == 0xa0 and data[offset + 392] == 0xc0

    def feed(self, data):
        self.buffer.extend(data)
        while len(self.buffer) >= 393:
            if not self.locked:
                start = self.buffer.find(b'\xa0')
                if start < 0:
                    self.discarded += len(self.buffer); self.buffer.clear(); break
                if start:
                    self.discarded += start; del self.buffer[:start]
                if len(self.buffer) < 786:
                    break
                if not (self.frame_at(self.buffer, 0) and self.frame_at(self.buffer, 393) and
                        (self.buffer[394] - self.buffer[1]) % 256 == 1):
                    self.discarded += 1; del self.buffer[0]; continue
                self.locked = True
            if not self.frame_at(self.buffer, 0):
                self.malformed += 1; self.locked = False
                self.discarded += 1; del self.buffer[0]; continue
            sequence = self.buffer[1]
            if self.last_sequence is not None:
                delta = (sequence - self.last_sequence) % 256
                if delta == 0: self.duplicates += 1
                elif delta > 128: self.discontinuities += 1
                elif delta > 1: self.missing += delta - 1
            self.last_sequence = sequence
            self.frames += 1
            del self.buffer[:393]


def counter_delta(before, after, key):
    return (integer(after, key) - integer(before, key)) & 0xffffffff


def record_connection(connection, seconds=10):
    if not math.isfinite(seconds) or not 2 <= seconds <= 60:
        raise DeviceError('Recording test duration must be between 2 and 60 seconds.')
    settings = settings_from_reply(transact(connection, 'SETTINGS'))
    adc = fields(transact(connection, 'CHECK'))
    if 'adc_ok_mask' not in adc or integer(adc, 'adc_ok_mask') != 65535 or integer(adc, 'adc_fault'):
        connection._freeeeg128_resume_allowed = False
        raise DeviceError('ADC settings did not pass. Run Check my board before recording.')
    baseline = fields(transact(connection, 'STATUS'))
    reader = FrameReader()
    print(f'Testing {settings["rate"]} Hz for {seconds:g} seconds...')
    try:
        transact(connection, 'START')
        warm_until = time.monotonic() + 0.25
        while time.monotonic() < warm_until:
            connection.read(max(1, min(connection.in_waiting, 65536)))
        connection.reset_input_buffer()
        start = time.monotonic()
        while time.monotonic() - start < seconds:
            reader.feed(connection.read(max(1, min(connection.in_waiting, 65536))))
        duration = time.monotonic() - start
    finally:
        # Drain queued EEG before asking for text counters, including on cancellation.
        try:
            transact(connection, 'STOP')
        except (OSError, TimeoutError, DeviceError):
            connection._freeeeg128_resume_allowed = False
            raise
    after_reply = transact(connection, 'STATUS')
    after = fields(after_reply)
    drops = {key: counter_delta(baseline, after, key) for key in ('usb_dropped', 'missed', 'dma_errors', 'rx_dropped')}
    measured = reader.frames / duration
    rate_ok = abs(measured - settings['rate']) <= settings['rate'] * 0.02
    reset = (integer(after, 'acquired') < integer(baseline, 'acquired') and
             integer(baseline, 'acquired') < 0xffffffff - 200000)
    passed = (reader.frames >= 2 and rate_ok and not reset and not integer(after, 'adc_fault') and
              not any(drops.values()) and not any((reader.missing, reader.duplicates, reader.malformed, reader.discontinuities)))
    print(f'Received {reader.frames} frames in {duration:.2f} s: {measured:.2f} Hz (requested {settings["rate"]} Hz)')
    print(f'Observed missing frames: {reader.missing}; malformed frames: {reader.malformed}; duplicates: {reader.duplicates}')
    print(f'USB drops: {drops["usb_dropped"]}; missed sample events: {drops["missed"]}; DMA errors: {drops["dma_errors"]}')
    print('Recording passed.' if passed else 'Recording check failed. See the saved report for counters and timing.')
    print('This checks data delivery, not electrode contact or EEG signal quality.')
    return {'passed': passed, 'duration_seconds': duration, 'requested_hz': settings['rate'], 'measured_hz': measured,
            'frames': reader.frames, 'observed_missing_frames': reader.missing, 'duplicates': reader.duplicates,
            'malformed_frames': reader.malformed, 'sequence_discontinuities': reader.discontinuities,
            'discarded_bytes': reader.discarded, 'device_reset': reset, 'counter_changes': drops,
            'status_before': baseline, 'status_after': after,
            'limits': '2% host rate tolerance; 8-bit sequence cannot identify losses of exact multiples of 256 frames; device counters are also checked.'}


def tool_versions():
    result = {'python': sys.version.split()[0], 'platform': platform.platform()}
    try:
        result['pyserial'] = importlib.metadata.version('pyserial')
    except importlib.metadata.PackageNotFoundError:
        result['pyserial'] = 'not found'
    from flash import find_gcc_dir, local_tool
    gcc_dir = find_gcc_dir()
    compiler = str(Path(gcc_dir) / ('arm-none-eabi-gcc.exe' if sys.platform == 'win32' else 'arm-none-eabi-gcc')) if gcc_dir else shutil.which('arm-none-eabi-gcc')
    for label, executable in (('arm_compiler', compiler), ('openocd', local_tool('openocd') or shutil.which('openocd'))):
        if not executable:
            result[label] = 'not installed (not required for USB controls)'
            continue
        try:
            check = subprocess.run([executable, '--version'], capture_output=True, text=True, timeout=3)
            lines = (check.stdout + check.stderr).splitlines()
            result[label] = {'path': executable, 'version': lines[0] if lines else 'unavailable'}
        except (OSError, subprocess.TimeoutExpired) as exc:
            result[label] = {'path': executable, 'error': str(exc)}
    return result


def run(args):
    settings = None
    if args.action == 'restore':
        source = args.file
        if not source:
            if not sys.stdin.isatty():
                raise DeviceError('Restore requires --file PATH to a settings backup.')
            source = input('Settings backup file: ').strip()
        settings = read_backup(source)  # File validation precedes board commands.
    if args.action == 'record' and (not math.isfinite(args.seconds) or not 2 <= args.seconds <= 60):
        raise DeviceError('Recording test duration must be between 2 and 60 seconds.')
    if args.action not in ('backup', 'restore'):
        print('Close other EEG software before using this helper.')
    with session(args.port, args.advanced) as (connection, info, port):
        if args.action == 'backup':
            backup_connection(connection, info, args.output)
            return 0
        if args.action == 'restore':
            restore_connection(connection, settings, args.yes, args.save)
            return 0
        document = {'format': 'freeeeg128-support', 'version': 1, 'created_utc': timestamp(),
                    'board': BOARD_ID, 'usb': {'port': port.device, 'serial': port.serial, 'location': port.location}, 'info': info}
        if args.action == 'record':
            document['recording'] = record_connection(connection, args.seconds)
            passed = document['recording']['passed']
        else:
            document['board_check'] = check_connection(connection, info, args.advanced)
            passed = document['board_check']['passed']
        document['tools'] = tool_versions()
        document['commands'] = list(getattr(connection, '_freeeeg128_log', []))
        path = write_json(document, args.output, args.action)
        print(f'Report saved: {path}')
        return 0 if passed else 1
