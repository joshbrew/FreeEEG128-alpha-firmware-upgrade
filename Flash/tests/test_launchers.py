"""Test platform wrappers with a recording backend; never install tools or flash."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
BASH = os.environ.get('FREEEEG128_TEST_BASH') or shutil.which('bash')
ACTIONS = {'Build': 'build', 'Build-and-Flash': 'build-flash',
           'Flash-Precompiled': 'flash', 'USB-Update': 'usb-update',
           'Install-Tools': 'setup', 'Configure': 'configure', 'Menu': None,
           'Check-Board': 'check', 'Test-Recording': 'record', 'Support-Report': 'report',
           'Backup-Settings': 'backup', 'Restore-Settings': 'restore'}


class Launchers(unittest.TestCase):
    def prepare(self, folder):
        root = Path(folder)
        shutil.copytree(ROOT / 'Flash', root / 'Flash',
                        ignore=shutil.ignore_patterns('tests', '__pycache__'))
        (root / 'record.py').write_text(
            'import json, os, pathlib, sys\n'
            'pathlib.Path(os.environ["FREEEEG128_TEST_LOG"]).write_text(json.dumps(sys.argv[1:]))\n'
            'raise SystemExit(int(os.environ.get("FREEEEG128_TEST_EXIT", "0")))\n')
        python = Path(sys.executable).as_posix()
        (root / 'FreeEEG128.cmd').write_bytes(
            f'@echo off\r\n"{python}" "%~dp0record.py" %*\r\nexit /b %ERRORLEVEL%\r\n'.encode())
        (root / 'FreeEEG128.sh').write_text(
            '#!/usr/bin/env bash\n'
            'task_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"\n'
            f'exec "{python}" "$task_root/record.py" "$@"\n', newline='\n')
        env = dict(os.environ, FREEEEG128_TEST_LOG=str(root / 'arguments.json'))
        return root, env

    @unittest.skipUnless(os.name == 'nt', 'Windows CMD runtime required')
    def test_windows_actions_forward_quoted_arguments_and_exit_status(self):
        with tempfile.TemporaryDirectory(prefix='FreeEEG128 launcher test ') as folder:
            root, env = self.prepare(folder)
            arguments = ['-Image', 'firmware $[name] & spaces.hex', '-DryRun']
            for name, action in ACTIONS.items():
                wrapper = root / 'Flash/Windows' / (name + '.cmd')
                # CMD parses its /c argument itself, rather than using CRT argv quoting.
                command = f'"{os.environ.get("COMSPEC", "cmd.exe")}" /d /s /c ""{wrapper}" ' + subprocess.list2cmdline(arguments) + '"'
                result = subprocess.run(command,
                                        cwd=root.parent, env=env, input='\n', text=True, capture_output=True, timeout=15)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(json.loads((root / 'arguments.json').read_text()),
                                 ([action] if action else []) + arguments)
            env['FREEEEG128_TEST_EXIT'] = '17'
            result = subprocess.run(command,
                                    cwd=root.parent, env=env, input='\n', text=True, capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 17)

    @unittest.skipUnless(BASH and (os.name == 'posix' or os.environ.get('FREEEEG128_TEST_BASH')),
                         'Bash runtime required')
    def test_linux_actions_forward_quoted_arguments_and_exit_status(self):
        with tempfile.TemporaryDirectory(prefix='FreeEEG128 launcher test ') as folder:
            root, env = self.prepare(folder)
            arguments = ['firmware $[name] & spaces.hex', '--dry-run']
            for name, action in ACTIONS.items():
                wrapper = root / 'Flash/Linux' / (name.lower() + '.sh')
                result = subprocess.run([BASH, str(wrapper), *arguments], cwd=root.parent,
                                        env=env, text=True, capture_output=True, timeout=15)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(json.loads((root / 'arguments.json').read_text()),
                                 ([action] if action else []) + arguments)
            env['FREEEEG128_TEST_EXIT'] = '17'
            result = subprocess.run([BASH, str(wrapper), *arguments], cwd=root.parent,
                                    env=env, text=True, capture_output=True, timeout=15)
            self.assertEqual(result.returncode, 17)


if __name__ == '__main__':
    unittest.main()
