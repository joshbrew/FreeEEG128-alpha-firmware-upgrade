"""FreeEEG128 board discovery, settings menu and verified USB updates."""
import argparse
from dataclasses import dataclass
import os
from pathlib import Path
import re
import struct
import sys
import time
import zlib

BOARD_ID = 'FreeEEG128-alpha-H743ZI-v1'
BUILD_PATTERN = rb'FreeEEG128-build:[0-9a-f]{32}\x00'


class DeviceError(RuntimeError):
    pass


@dataclass(frozen=True)
class Port:
    device: str
    serial: str = ''
    location: str = ''


@dataclass(frozen=True)
class Target:
    port: Port = None
    drive: Path = None

    def label(self):
        if self.port:
            return f'USB serial: {self.port.device} (serial {self.port.serial or "unknown"})'
        return f'UF2 loader: {self.drive}'


@dataclass(frozen=True)
class Image:
    data: bytes
    build: str


def windows_usb_product(serial_number):
    """Read the USB bus descriptor when usbser exposes only a generic COM name.

    CM_Get_DevNode_PropertyW / DEVPKEY_Device_BusReportedDeviceDesc:
    https://learn.microsoft.com/windows-hardware/drivers/install/devpkey-device-busreporteddevicedesc
    """
    if os.name != 'nt' or not serial_number:
        return ''
    import ctypes
    from ctypes import wintypes
    import uuid

    class GUID(ctypes.Structure):
        _fields_ = [('d1', wintypes.DWORD), ('d2', wintypes.WORD),
                    ('d3', wintypes.WORD), ('d4', ctypes.c_ubyte * 8)]

    class Key(ctypes.Structure):
        _fields_ = [('guid', GUID), ('pid', wintypes.DWORD)]

    cm = ctypes.WinDLL('cfgmgr32')
    locate = cm.CM_Locate_DevNodeW
    locate.argtypes = [ctypes.POINTER(wintypes.DWORD), wintypes.LPWSTR, wintypes.ULONG]
    locate.restype = wintypes.ULONG
    get = cm.CM_Get_DevNode_PropertyW
    get.argtypes = [wintypes.DWORD, ctypes.POINTER(Key), ctypes.POINTER(wintypes.ULONG),
                   ctypes.c_void_p, ctypes.POINTER(wintypes.ULONG), wintypes.ULONG]
    get.restype = wintypes.ULONG
    instance = wintypes.DWORD()
    instance_id = f'USB\\VID_0483&PID_5740\\{serial_number}'
    if locate(ctypes.byref(instance), instance_id, 0) != 0:
        return ''
    key = Key(GUID.from_buffer_copy(uuid.UUID('540b947e-8b40-45bc-a8a2-6a0b894cbda2').bytes_le), 4)
    value = ctypes.create_unicode_buffer(512)
    size = wintypes.ULONG(ctypes.sizeof(value))
    kind = wintypes.ULONG()
    if get(instance, ctypes.byref(key), ctypes.byref(kind), value, ctypes.byref(size), 0) == 0 and kind.value == 0x12:
        return value.value
    return ''


def serial_ports():
    from serial.tools import list_ports
    result = []
    for item in list_ports.comports():
        identity = ' '.join(str(getattr(item, key, '') or '')
                            for key in ('manufacturer', 'product', 'description'))
        # ST's VID/PID alone also identifies unrelated STM32 serial devices.
        if (item.vid, item.pid) != (0x0483, 0x5740):
            continue
        if 'freeeeg128' not in identity.lower():
            identity += ' ' + windows_usb_product(item.serial_number)
        if 'freeeeg128' in identity.lower():
            result.append(Port(item.device, item.serial_number or '', item.location or ''))
    return sorted(result, key=lambda p: p.device)


def mount_paths():
    if os.name == 'nt':
        import ctypes
        mask = ctypes.windll.kernel32.GetLogicalDrives()
        roots = [f'{chr(65 + i)}:/' for i in range(26) if mask & (1 << i)]
        drive_type = ctypes.windll.kernel32.GetDriveTypeW
        drive_type.argtypes = [ctypes.c_wchar_p]
        drive_type.restype = ctypes.c_uint
        # Skip network mappings, which can block discovery when offline.
        return [Path(root) for root in roots if drive_type(root) in (2, 3)]
    result = set()
    try:
        for line in Path('/proc/self/mountinfo').read_text().splitlines():
            mount = line.split()[4]
            result.add(Path(re.sub(r'\\([0-7]{3})', lambda m: chr(int(m[1], 8)), mount)))
    except OSError:
        if Path('/Volumes').is_dir():
            result.update(Path('/Volumes').iterdir())
    return sorted(result)


def is_uf2_drive(path):
    try:
        info = (Path(path) / 'INFO_UF2.TXT').read_text(errors='replace')
        return f'Board-ID: {BOARD_ID}' in info.splitlines()
    except OSError:
        return False


def uf2_drives():
    return [path for path in mount_paths() if is_uf2_drive(path)]


def choose(items, describe=str, input_fn=input):
    if not items:
        raise DeviceError('No FreeEEG128 found. Connect its USB data port, or press RESET twice for UF2 mode.')
    if len(items) == 1:
        return items[0]
    print('Several FreeEEG128 boards are connected:')
    for index, item in enumerate(items, 1):
        print(f'  {index}  {describe(item)}')
    if input_fn is input and not sys.stdin.isatty():
        raise DeviceError('Select one board with --port or --drive, or run the launcher interactively.')
    while True:
        value = input_fn('Board number: ').strip()
        if value.isdecimal() and 1 <= int(value) <= len(items):
            return items[int(value) - 1]
        print('Enter one of the listed numbers.')


def select_target(port=None, drive=None, include_loader=True):
    if port:
        known = next((item for item in serial_ports() if item.device.casefold() == port.casefold()), Port(port))
        return Target(port=known)
    if drive:
        path = Path(drive).resolve()
        if not is_uf2_drive(path):
            raise DeviceError(f'{path} is not a FreeEEG128 UF2 drive.')
        return Target(drive=path)
    targets = [Target(port=p) for p in serial_ports()]
    if include_loader:
        targets += [Target(drive=p) for p in uf2_drives()]
    elif not targets and uf2_drives():
        raise DeviceError('The board is in UF2 update mode. Press RESET once to start the installed application, '
                          'then retry. If no application is installed, copy its UF2 first.')
    elif not targets:
        raise DeviceError('No running FreeEEG128 found. Connect its USB data port and power the board. '
                          'A new board needs the factory image installed with ST-Link first.')
    return choose(targets, Target.label)


def open_port(port):
    import serial
    try:
        options = {'exclusive': True} if os.name != 'nt' else {}
        return serial.Serial(port.device, 115200, timeout=0.1, write_timeout=2, **options)
    except (OSError, serial.SerialException) as exc:
        raise DeviceError(f'Cannot open {port.device}. Close the EEG application using this port. '
                          'On Linux, also check serial-device permissions. ' + str(exc)) from exc


def transact(port, command, timeout=3):
    if any(c in command for c in '\r\n!') or not command.strip():
        raise ValueError('Provide each command without ! or newlines.')
    command = command.strip().upper()
    if command.split()[0] in ('SAVE', 'FACTORY') and timeout == 3:
        timeout = 10  # Explicit flash writes/sector erase can outlast an ordinary reply.
    port.write(('!' + command + '\n').encode('ascii'))
    port.flush()
    deadline = time.monotonic() + timeout
    data = bytearray()
    prefix = ('OK ' + command.split()[0]).encode('ascii')
    while time.monotonic() < deadline:
        data.extend(port.read(max(1, min(port.in_waiting, 2048))))
        match = re.search(re.escape(prefix) + rb'(?=[ \r\n])', data)
        for marker, start in ((prefix, match.start() if match else -1), (b'ERR ', data.find(b'ERR '))):
            if start < 0:
                continue
            end = data.find(b'\r\n', start)
            if end >= 0:
                reply = bytes(data[start:end]).decode('ascii', errors='replace')
                if marker == b'ERR ':
                    log_reply(port, command, reply)
                    raise DeviceError(reply)
                log_reply(port, command, reply)
                return reply
        if len(data) > 8192:
            del data[:-4096]
    log_reply(port, command, 'Timed out')
    raise TimeoutError('No control reply. Check the USB data connection, close other EEG software, '
                       'and check that the board has firmware with USB controls.')


def fields(reply):
    return dict(part.split('=', 1) for part in reply.split() if '=' in part)


def log_reply(port, command, reply):
    entries = getattr(port, '_freeeeg128_log', None)
    if entries is not None:
        entries.append({'command': command, 'reply': reply})
        del entries[:-256]


def friendly_error(error):
    text = str(error)
    if isinstance(error, FileExistsError):
        return 'That file already exists. Choose a new file name to preserve the earlier backup or report.'
    if 'journal full' in text:
        return 'Settings storage is full. Back up your settings, restore factory settings, then restore your backup.'
    if text.startswith(('ERR RATE', 'ERR GAIN', 'ERR PRESET', 'ERR LOAD', 'ERR FACTORY', 'ERR ADC')):
        return 'The ADC settings could not be verified. Streaming is paused. Run Check my board; check board power and reboot before retrying.'
    if text.startswith('ERR SAVE'):
        return 'Settings could not be saved. Keep the board powered and run Check my board. Changes may only be in RAM.'
    if text.startswith('ERR command'):
        return 'This firmware does not support that command. Check its version or update the application.'
    return text


def info_summary(reply):
    values = fields(reply)
    version = re.search(r'FreeEEG128-alpha\s+(\S+)', reply)
    result = f'Firmware: {version[1] if version else "unknown"}; CPU: {values.get("cpu_mhz", "?")} MHz'
    if values.get('update'):
        result += '; USB updates: ' + ('ready' if values['update'] == 'UF2' else 'loader installation required')
    if values.get('watchdog'):
        result += '; hang recovery: ' + ('on' if values['watchdog'] == 'IWDG1' else 'not active')
    return result


def identify(port):
    port._freeeeg128_log = []
    port.reset_input_buffer()
    previous = fields(transact(port, 'STOP')).get('was_streaming')
    port._freeeeg128_was_streaming = previous == '1' if previous in ('0', '1') else None
    port._freeeeg128_resume_allowed = True
    reply = transact(port, 'INFO')
    if not reply.startswith('OK INFO FreeEEG128-alpha '):
        raise DeviceError('The selected port did not identify as FreeEEG128.')
    return reply


def read_image(path):
    path = Path(path)
    if path.suffix.lower() != '.uf2' or not path.is_file():
        raise DeviceError(f'Application UF2 not found: {path}. Build firmware or select a precompiled .uf2.')
    if not 512 <= path.stat().st_size <= 3072 * 512 or path.stat().st_size % 512:
        raise DeviceError('Invalid UF2 file size.')
    data = path.read_bytes()
    count = len(data) // 512
    chunks = [None] * count
    record = None
    for offset in range(0, len(data), 512):
        block = data[offset:offset + 512]
        a, b, flags, address, size, number, total, family = struct.unpack_from('<8I', block)
        if (a, b, flags, size, total, family) != (0x0a324655, 0x9e5d5157, 0x2000, 256, count, 0x6db66082):
            raise DeviceError('UF2 header/family does not match this board.')
        if number >= count or address != 0x08020000 + number * 256 or chunks[number] is not None:
            raise DeviceError('UF2 has duplicate blocks or invalid application addresses.')
        if struct.unpack_from('<I', block, 508)[0] != 0x0ab16f30:
            raise DeviceError('UF2 block is incomplete.')
        if record is not None and block[288:320] != record:
            raise DeviceError('UF2 contains inconsistent image records.')
        record = block[288:320]
        chunks[number] = block[32:288]
    image = b''.join(chunks)
    magic, layout, board, length, crc, inv_length, inv_crc, rec_crc = struct.unpack('<8I', record)
    if ((magic, layout, board, length, inv_length, inv_crc) !=
            (0x32474546, 1, 0x38314546, len(image), len(image) ^ 0xffffffff, crc ^ 0xffffffff)
            or zlib.crc32(record[:28]) != rec_crc or zlib.crc32(image) != crc):
        raise DeviceError('UF2 board record or image checksum is invalid.')
    if len(image) < 0x420 or struct.unpack_from('<5I', image, 0x400) != (0x42474546, 1, 0x38314546, 0x08020000, 0x080e0000):
        raise DeviceError('UF2 application is for a different flash layout.')
    sp, entry = struct.unpack_from('<II', image)
    if not (0x20000000 < sp <= 0x20020000 and sp % 8 == 0 and entry & 1 and 0x08020000 <= entry < 0x08020000 + len(image)):
        raise DeviceError('UF2 application vectors are invalid.')
    tags = set(re.findall(BUILD_PATTERN, image))
    if len(tags) != 1:
        raise DeviceError('This image has no verifiable build ID. Rebuild it for automatic verification, '
                          'or copy it manually to the UF2 drive.')
    return Image(data, tags.pop()[:-1].decode('ascii'))


def wait_for_drive(before, explicit=None, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        candidates = ([Path(explicit).resolve()] if explicit else
                      [p for p in uf2_drives() if p not in before])
        candidates = [p for p in candidates if is_uf2_drive(p)]
        if candidates:
            return choose(candidates)
        time.sleep(0.25)
    raise DeviceError('The UF2 drive did not mount. Press RESET twice, check USB automount, '
                      'then retry with --drive PATH. First installation requires ST-Link.')


def reconnect_candidates(original, before):
    ports = serial_ports()
    if original and original.serial:
        return [p for p in ports if p.serial == original.serial]
    if original and original.location:
        return [p for p in ports if p.location == original.location]
    # Never verify an unrelated board that was already connected before the update.
    return [p for p in ports if p not in before]


def verify_restart(image, drive, original, before, timeout=90):
    deadline = time.monotonic() + timeout
    last_error = ''
    while time.monotonic() < deadline:
        if is_uf2_drive(drive):
            time.sleep(0.25)
            continue
        candidates = reconnect_candidates(original, before)
        if len(candidates) > 1:
            raise DeviceError('Several boards appeared after updating. Reconnect only the updated board '
                              'and check its INFO build ID; update success has not been verified.')
        if candidates:
            try:
                connection = open_port(candidates[0])
            except DeviceError as exc:
                last_error = str(exc)
                time.sleep(0.25)
                continue
            try:
                with connection as port:
                    info = identify(port)
                    if fields(info).get('build') != image.build:
                        raise DeviceError('The board restarted into a different firmware build. '
                                          'Update success has not been verified. ' + info)
                    # INFO pauses the stream. Return the freshly booted board to acquisition.
                    try:
                        transact(port, 'START')
                    except DeviceError as exc:
                        raise DeviceError(f'Firmware build verified, but acquisition could not start: {exc}') from exc
                    return info
            except (OSError, TimeoutError) as exc:
                last_error = str(exc)
        time.sleep(0.25)
    raise DeviceError('Firmware copied, but restart was not verified. Check STATUS.TXT on the '
                      'UF2 drive and reconnect the USB data cable. ' + last_error)


def update(image_path, port=None, drive=None):
    image = read_image(image_path)  # Reject bad files before sending any commands to hardware.
    target = select_target(port, drive)
    before_ports = serial_ports()
    before_drives = uf2_drives()
    print(f'Selected {target.label()}')
    print(f'Firmware build: {image.build.removeprefix("FreeEEG128-build:")}')
    if target.port:
        if drive and Path(drive).resolve() in before_drives:
            raise DeviceError('That UF2 drive is already connected to another loader. '
                              'Use --drive without --port to update it directly.')
        print('Entering update mode...')
        with open_port(target.port) as connection:
            info = identify(connection)
            if fields(info).get('update') != 'UF2':
                raise DeviceError('Install freeeeg128-factory.hex through ST-Link before using USB updates.')
            transact(connection, 'BOOTLOADER')
        print('Waiting for the FreeEEG128 drive...')
        target_drive = wait_for_drive(before_drives, drive)
    else:
        target_drive = target.drive
    if not is_uf2_drive(target_drive):
        raise DeviceError('The selected FreeEEG128 drive disconnected before copying.')
    print(f'Copying firmware to {target_drive}...')
    copy_error = None
    try:
        with (target_drive / 'firmware.uf2').open('wb') as output:
            output.write(image.data)
            output.flush()
            os.fsync(output.fileno())
    except OSError as exc:
        # Some hosts report disconnect during flush; only the running build can confirm success.
        copy_error = exc
        print(f'USB reported a copy/flush error: {exc}. Checking whether installation completed...')
    print('Waiting for verification, installation and restart...')
    try:
        info = verify_restart(image, target_drive, target.port, before_ports)
    except DeviceError as exc:
        if copy_error:
            raise DeviceError(f'Copy failed: {copy_error}. {exc}') from exc
        raise
    print('Firmware verified. USB EEG streaming resumed.')
    print(info_summary(info))


def show_settings(connection):
    values = fields(transact(connection, 'SETTINGS'))
    gains = values.get('gains', '').split(',')
    print(f'\nSample rate: {values.get("rate", "?")} Hz')
    print('Changes: ' + ('not saved' if values.get('dirty') == '1' else 'no unsaved changes'))
    print(f'Loaded from: {values.get("source", "unknown")}')
    for start in range(0, len(gains), 8):
        print(f'  Channels {start + 1:2}-{min(start + 8, len(gains)):2}: ' + '  '.join(gains[start:start + 8]))
    return values


def menu_value(prompt, choices, input_fn):
    value = input_fn(prompt).strip().upper()
    if value not in choices:
        print('Choose: ' + ', '.join(choices))
        return None
    return value


def configure(port=None, input_fn=input, advanced=False):
    target = select_target(port, include_loader=False)
    print(f'Selected {target.label()}')
    print('Settings pause acquisition. Choose Start streaming when finished; Save stores changes for power-up.')
    with open_port(target.port) as connection:
        info = identify(connection)
        print(info if advanced else info_summary(info))
        while True:
            values = show_settings(connection)
            print('\n1  Sample rate\n2  Channel gain\n3  Preset\n4  Save settings\n'
                  '5  Load saved settings\n6  Restore factory settings\n7  Status and diagnostics\n'
                  '8  Start streaming and exit\n9  Back up settings\n10 Restore settings from file\n'
                  'A  Show/hide advanced details\n0  Exit with streaming paused')
            action = input_fn('Select: ').strip()
            try:
                if action == '1':
                    rate = menu_value('Sample rate (250/500/1000/2000 Hz): ', ('250', '500', '1000', '2000'), input_fn)
                    if rate:
                        print(transact(connection, 'RATE ' + rate))
                elif action == '2':
                    channel = input_fn('Channel (1-128 or ALL): ').strip().upper()
                    if channel != 'ALL' and (not channel.isdecimal() or not 1 <= int(channel) <= 128):
                        print('Choose channel 1-128 or ALL.')
                        continue
                    gain = menu_value('Gain (1/2/4/8/16/32/64/128): ', ('1', '2', '4', '8', '16', '32', '64', '128'), input_fn)
                    if gain:
                        channels = range(1, 129) if channel == 'ALL' else [int(channel)]
                        for number in channels:
                            transact(connection, f'GAIN {number} {gain}')
                        print('Gain applied. Choose Save to keep it after power-up.')
                elif action == '3':
                    print('1  500 Hz, gain 8 (DEFAULT)\n2  2000 Hz, gain 8 (FAST)\n3  1000 Hz, gain 1 (LOWGAIN)')
                    choice = input_fn('Preset: ').strip().upper()
                    preset = {'1': 'DEFAULT', '2': 'FAST', '3': 'LOWGAIN'}.get(choice, choice)
                    if preset not in ('DEFAULT', 'FAST', 'LOWGAIN'):
                        print('Choose 1, 2 or 3.'); continue
                    if preset:
                        print(transact(connection, 'PRESET ' + preset))
                elif action == '4':
                    print(transact(connection, 'SAVE'))
                elif action == '5':
                    print(transact(connection, 'LOAD'))
                elif action == '6':
                    if input_fn('Erase saved settings and restore 500 Hz / gain 8? Type RESET: ').strip() == 'RESET':
                        print(transact(connection, 'FACTORY'))
                elif action == '7':
                    from board_tools import status_summary
                    status, diag = transact(connection, 'STATUS'), transact(connection, 'DIAG')
                    print(status_summary(status, diag))
                    if advanced: print(status); print(diag)
                elif action in ('8', '0'):
                    if values.get('dirty') == '1':
                        decision = input_fn('Unsaved changes: S save, K keep only in RAM, C continue editing [C]: ').strip().upper()
                        if decision == 'S': print(transact(connection, 'SAVE'))
                        elif decision != 'K': continue
                    if action == '8':
                        transact(connection, 'START')
                        print('USB EEG streaming resumed.')
                    else:
                        print('Streaming paused. Reopen Configure board and choose Start streaming to resume.')
                    return
                elif action == '9':
                    from board_tools import backup_connection
                    output = input_fn('Backup file path [automatic dated file]: ').strip()
                    backup_connection(connection, info, output or None)
                elif action == '10':
                    from board_tools import read_backup, restore_connection
                    path = input_fn('Settings file to restore: ').strip()
                    settings = read_backup(path)
                    restore_connection(connection, settings, input_fn=input_fn)
                elif action.upper() == 'A':
                    advanced = not advanced
                    print('Advanced details ' + ('shown.' if advanced else 'hidden.'))
                else:
                    print('Choose a listed action.')
            except DeviceError as exc:
                from board_tools import failure_report
                print(f'{friendly_error(exc)}\nStreaming remains paused.')
                failure_report(exc, connection, info, target.port)
                if advanced: print(str(exc))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=('update', 'configure', 'list', 'check', 'record', 'report', 'backup', 'restore'))
    parser.add_argument('--port', help='Override automatic USB serial selection')
    parser.add_argument('--drive', help='Override automatic UF2 drive selection')
    parser.add_argument('--image', help='Application UF2 to install')
    parser.add_argument('--output', help='Report or backup file path (default: dated file in reports/)')
    parser.add_argument('--file', help='Settings backup to restore')
    parser.add_argument('--seconds', type=float, default=10, help='Recording test duration, 2-60 seconds')
    parser.add_argument('--advanced', action='store_true', help='Show technical replies in addition to summaries')
    parser.add_argument('--yes', action='store_true', help='Apply a validated settings restore without the interactive preview prompt')
    parser.add_argument('--save', action='store_true', help='Also persist restored settings; requires --yes or an interactive choice')
    args = parser.parse_args()
    try:
        import serial
        if args.action == 'update':
            if not args.image:
                parser.error('update requires --image PATH')
            update(args.image, args.port, args.drive)
        elif args.action == 'configure':
            if args.drive:
                parser.error('configure uses USB serial; leave UF2 mode first')
            configure(args.port, advanced=args.advanced)
        elif args.action == 'list':
            targets = [Target(port=p) for p in serial_ports()] + [Target(drive=p) for p in uf2_drives()]
            for target in targets:
                print(target.label())
            if not targets:
                print('No FreeEEG128 connected.')
        else:
            from board_tools import run
            return run(args)
        return 0
    except ImportError:
        print('USB helper dependency missing. Run the FreeEEG128 launcher to install pyserial.', file=sys.stderr)
    except (DeviceError, OSError, TimeoutError, serial.SerialException) as exc:
        from board_tools import failure_report
        print(f'Error: {friendly_error(exc)}', file=sys.stderr)
        failure_report(exc)
    except (KeyboardInterrupt, EOFError):
        print('\nCancelled. Acquisition may be paused; reopen Configure board to start streaming.', file=sys.stderr)
    return 1


if __name__ == '__main__':
    # Shared helpers import this module when it is also the command-line entry point.
    sys.modules.setdefault('device', sys.modules[__name__])
    sys.exit(main())
