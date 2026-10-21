"""Run the actual firmware logic on a host compiler; never access a board."""
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / 'Firmware/build/native-tests'
OUT.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
cc = shutil.which('cc') or shutil.which('gcc')
msvc = os.name == 'nt'
if msvc:
    cc = shutil.which('cl')
    if not cc:
        installer = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe'
        install = subprocess.check_output([str(installer), '-latest', '-products', '*', '-requires',
                                          'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        setup = Path(install) / 'VC/Auxiliary/Build/vcvarsall.bat'
        result = subprocess.check_output(f'call "{setup}" x64 >nul && set', shell=True, text=True)
        env.update(line.split('=', 1) for line in result.splitlines() if '=' in line and not line.startswith('='))
        cc = shutil.which('cl', path=env.get('Path', env.get('PATH')))
if not cc:
    raise SystemExit('Install a host C compiler: GCC/Clang or MSVC Build Tools.')

def run(name, sources, includes, defines=()):
    exe = OUT / (name + ('.exe' if msvc else ''))
    if msvc:
        args = [cc, '/nologo', '/std:c11', '/W3', '/utf-8', '/D_CRT_SECURE_NO_WARNINGS',
                *['/I' + str(ROOT / p) for p in includes], *['/D' + d for d in defines],
                *[str(ROOT / p) for p in sources], '/Fe:' + str(exe)]
    else:
        args = [cc, '-std=c11', '-Wall', '-Wextra', *['-I' + str(ROOT / p) for p in includes],
                *['-D' + d for d in defines], *[str(ROOT / p) for p in sources], '-o', str(exe)]
    result = subprocess.run(args, cwd=OUT, env=env, text=True, capture_output=True)
    if result.returncode:
        raise SystemExit(result.stdout + result.stderr)
    subprocess.run([str(exe)], cwd=OUT, env=env, check=True)

A = 'Firmware/Alpha'
U = 'Firmware/UF2'
run('ads131', [f'{A}/tests/test_ads131.c'], [f'{A}/tests/mocks', f'{A}/Inc'])
run('firmware', [f'{A}/tests/test_firmware.c', f'{A}/Src/control_protocol.c'], [f'{A}/tests/mocks', f'{A}/Inc'])
run('settings', [f'{A}/tests/test_settings.c', f'{A}/Src/settings.c'], [f'{A}/Inc'])
run('watchdog', [f'{A}/tests/test_watchdog.c', f'{A}/Src/watchdog.c'], [f'{A}/Inc'])
for mhz in (40, 80, 160, 240, 320, 400, 480):
    run(f'clock-{mhz}', [f'{A}/tests/test_clock.c'], [f'{A}/Inc'], [f'FREEEEG128_CPU_MHZ={mhz}'])
for mhz in (40, 80, 160, 240, 320, 400, 480):
    for boot in (False, True):
        defines = [f'FREEEEG128_CPU_MHZ={mhz}'] + (['FREEEEG128_UF2_BOOTLOADER'] if boot else [])
        run(f'clock-start-{mhz}-{"boot" if boot else "app"}',
            [f'{A}/tests/test_clock_start.c', f'{A}/Src/clock_start.c'],
            [f'{A}/tests/clock_mocks', f'{A}/Inc'], defines)
run('uf2', [f'{U}/tests/test_update.c', f'{U}/Src/uf2_update.c', f'{U}/Src/uf2_disk.c'], [f'{U}/Inc'])
