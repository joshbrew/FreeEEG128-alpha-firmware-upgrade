"""FreeEEG128 USB controls. Install pyserial with: python -m pip install pyserial."""
import argparse
from device import DeviceError, identify, select_target, open_port, transact


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', help='COM port or /dev/ttyACM device')
    parser.add_argument('--list', action='store_true', help='List USB serial devices')
    parser.add_argument('--resume', action='store_true', help='Resume acquisition after the commands')
    parser.add_argument('commands', nargs='*', help='Quoted commands, for example "RATE 1000" "GAIN 1 4" STATUS')
    args = parser.parse_args()
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.error('Install pyserial: python -m pip install pyserial')
    if args.list:
        for port in list_ports.comports():
            print(f'{port.device}: {port.description} serial={port.serial_number}')
        return
    commands = args.commands or ['INFO', 'STATUS']
    for command in commands:
        if any(c in command for c in '\r\n!') or not command.strip():
            parser.error('Provide each command as one quoted argument without ! or newlines')
    try:
        target = select_target(args.port, include_loader=False)
        with open_port(target.port) as port:
            print(identify(port))
            for command in commands:
                print(transact(port, command.strip().upper()))
            if args.resume:
                print(transact(port, 'START'))
    except (DeviceError, OSError, TimeoutError, serial.SerialException) as exc:
        parser.exit(1, f'Error: {exc}\n')
    except (KeyboardInterrupt, EOFError):
        parser.exit(1, '\nCancelled. Streaming may be paused.\n')


if __name__ == '__main__':
    main()
