"""Cross-platform build without make/Docker. Requires Arm GNU Toolchain on PATH."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from prepare import prepare, HERE, ARCHIVE_SHA256
from flash import find_gcc_dir


CPU_PROFILES = (40, 80, 160, 240, 320, 400, 480)


def archive_build(out, files, cpu_mhz, build_id, target):
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
    destination = HERE.parent / 'build/history' / f'{stamp}-{target}-{cpu_mhz}mhz-{build_id[:12]}'
    destination.mkdir(parents=True, exist_ok=False)
    for name in files:
        shutil.copy2(out / name, destination / name)
    print(f'Build saved: {destination}')
    return destination


def clock_manifest(cpu_mhz):
    if cpu_mhz not in CPU_PROFILES:
        raise ValueError('Unsupported CPU profile; choose ' + ', '.join(map(str, CPU_PROFILES)))
    ahb = {40: 40, 80: 40, 160: 40, 240: 60, 320: 40, 400: 50, 480: 60}[cpu_mhz]
    return {'cpu_mhz': cpu_mhz, 'ahb_mhz': ahb, 'apb_mhz': ahb // 2,
            'usb_mhz': 48, 'adc_mclk_hz': 8192000,
            'primary_pll_source': 'HSE-bypass', 'hse_hz': 8192000,
            'fallback_pll_source': 'HSI', 'hsi_nominal_hz': 64000000,
            'spi_hz': 8192000, 'fallback_spi_nominal_hz': 8000000,
            'revision_v_required': cpu_mhz == 480,
            'fallback_on_non_v': {'cpu_mhz': 400, 'ahb_mhz': 50, 'apb_mhz': 25} if cpu_mhz == 480 else None}


def build(gcc_dir=None, jobs=None, uf2=False, cpu_mhz=80):
    clocks = clock_manifest(cpu_mhz)
    gcc_dir = gcc_dir or find_gcc_dir()
    root = prepare()
    out = HERE.parent / 'build' / ('uf2/application' if uf2 else 'release')
    out.mkdir(parents=True, exist_ok=True)
    if uf2:
        shared = HERE.parent / 'UF2'
        for name in ('uf2_layout.h', 'boot_request.h'):
            shutil.copy2(shared / 'Inc' / name, root / 'Inc' / name)
        shutil.copy2(shared / 'Src/board_id.c', root / 'Src/board_id.c')
        linker = root / 'STM32H743ZITx_FLASH.ld'
        script = linker.read_text(encoding='utf-8')
        script = script.replace('ORIGIN = 0x8000000, LENGTH = 2048K', 'ORIGIN = 0x8020000, LENGTH = 768K')
        script = script.replace('  .text :', '  .board_id ORIGIN(FLASH) + 0x400 : { KEEP(*(.board_id)) } >FLASH\n\n  .text :')
        linker.write_text(script, encoding='utf-8')
        system = root / 'Src/system_stm32h7xx.c'
        system.write_text(system.read_text(encoding='utf-8').replace('#define VECT_TAB_OFFSET  0x00000000UL', '#define VECT_TAB_OFFSET  0x00020000UL'), encoding='utf-8')
    else:
        # Reserve bank 2 sectors 6/7; standalone code must not overlap persistent settings.
        linker = root / 'STM32H743ZITx_FLASH.ld'
        linker.write_text(linker.read_text(encoding='utf-8').replace(
            'ORIGIN = 0x8000000, LENGTH = 2048K', 'ORIGIN = 0x8000000, LENGTH = 1792K'), encoding='utf-8')
    suffix = '.exe' if os.name == 'nt' else ''
    def tool(name):
        path = str(Path(gcc_dir) / ('arm-none-eabi-' + name + suffix)) if gcc_dir else shutil.which('arm-none-eabi-' + name)
        if not path or not Path(path).is_file():
            raise RuntimeError('Install Arm GNU Toolchain, add bin to PATH, or pass --gcc-dir PATH')
        return path
    gcc = tool('gcc')
    cpu = ['-mcpu=cortex-m7', '-mthumb', '-mfpu=fpv5-d16', '-mfloat-abi=hard']
    includes = ['Inc', 'Drivers/STM32H7xx_HAL_Driver/Inc', 'Drivers/STM32H7xx_HAL_Driver/Inc/Legacy',
                'Middlewares/Third_Party/FatFs/src', 'Middlewares/ST/STM32_USB_Device_Library/Core/Inc',
                'Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc',
                'Drivers/CMSIS/Device/ST/STM32H7xx/Include', 'Drivers/CMSIS/Include']
    text = (root / 'Makefile').read_text()
    sources = re.search(r'C_SOURCES =\s*(.*?)# ASM sources', text, re.S).group(1).replace('\\', '').split()
    sources = [s for s in sources if not s.startswith(('Src/hal.c', 'Src/ads131m0x.c', 'Src/bsp_driver_sd.c', 'Src/sd_diskio.c', 'Src/fatfs', 'Middlewares/Third_Party/'))]
    sources += ['Src/freeeeg128.c', 'Src/ads131_board.c', 'Src/clock_start.c', 'Src/control_protocol.c', 'Src/sysmem.c', 'Src/settings.c',
                'Src/settings_stm32h743.c', 'Src/runtime_stm32h743.c', 'Src/watchdog.c', 'Src/settings_probe.s', 'startup_stm32h743xx.s']
    if uf2:
        sources += ['Src/board_id.c']
    flags = cpu + ['-DUSE_HAL_DRIVER', '-DSTM32H743xx', '-Og', '-g', '-Wall', '-std=gnu11',
                   '-ffunction-sections', '-fdata-sections', '-fcommon',
                   '-Wno-error=implicit-function-declaration', '-Wno-error=incompatible-pointer-types',
                   '-Wno-error=int-conversion'] + ['-I' + p for p in includes]
    flags += [f'-DFREEEEG128_CPU_MHZ={cpu_mhz}', '-DHSE_VALUE=8192000u']
    if uf2:
        flags += ['-DFREEEEG128_UF2_APPLICATION']
    def run(args):
        result = subprocess.run(args, cwd=root, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        if result.returncode:
            raise RuntimeError(result.stdout)
        return result.stdout
    compiler = run([gcc, '--version']).splitlines()[0]
    fingerprint = hashlib.sha256()
    fingerprint.update(json.dumps([compiler, flags, 'maintained-C:-O2', 'link:nano,gc-sections'], sort_keys=True).encode())
    for source in sorted(root.rglob('*')):
        if source.is_file() and source.suffix.lower() in ('.c', '.h', '.s', '.ld'):
            fingerprint.update(source.relative_to(root).as_posix().encode() + b'\0')
            fingerprint.update(source.read_bytes() + b'\0')
    build_id = fingerprint.hexdigest()[:32]
    flags += ['-DFREEEEG128_BUILD_ID="' + build_id + '"']
    def compile_one(source):
        obj = out / (Path(source).stem + '.o')
        # Optimize the maintained acquisition/control path; retain legacy debug flags elsewhere.
        optimize = ['-O2'] if (HERE / source).exists() and source.endswith('.c') else []
        return obj, run([gcc, *flags, *optimize, '-c', source, '-o', str(obj)])
    with ThreadPoolExecutor(max_workers=jobs or min(os.cpu_count() or 1, 8)) as pool:
        results = list(pool.map(compile_one, sources))
    (out / 'warnings.log').write_text(''.join(output for _, output in results))
    elf = out / 'freeeeg128-alpha.elf'
    print(run([gcc, *cpu, *[str(obj) for obj, _ in results], '-specs=nano.specs',
               '-TSTM32H743ZITx_FLASH.ld', '-LDrivers/CMSIS/Lib/GCC', 
               '-lc', '-lm', '-lnosys', '-Wl,--gc-sections,-Map=' + str(out / 'freeeeg128-alpha.map'), '-o', str(elf)]))
    for fmt, ext in [('ihex', 'hex'), ('binary', 'bin')]:
        run([tool('objcopy'), '-O', fmt, str(elf), str(elf.with_suffix('.' + ext))])
    if not 8 <= elf.with_suffix('.bin').stat().st_size <= 2 * 1024 * 1024:
        raise RuntimeError('Unexpected binary size: check flash load addresses and NOLOAD sections')
    print(run([tool('size'), str(elf)]))
    manifest = {'board': 'FreeEEG128-alpha STM32H743ZI', 'version': 'qos-5', 'build_id': build_id, 'flash_address': '0x08020000' if uf2 else '0x08000000',
                'base_archive_sha256': ARCHIVE_SHA256, 'compiler': compiler,
                'hardware_validated': False, 'clocks': clocks, 'settings_address': '0x081c0000',
                'idle_mode': 'spin (optional WFI disabled by default)', 'maintained_source_optimization': '-O2', 'files': {}}
    for ext in ('elf', 'hex', 'bin'):
        path = elf.with_suffix('.' + ext)
        manifest['files'][path.name] = {'bytes': path.stat().st_size, 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Built firmware: {out} (requested CPU {cpu_mhz} MHz, AHB {clocks["ahb_mhz"]} MHz, USB 48 MHz)')
    if not uf2:
        archive_build(out, ['freeeeg128-alpha.hex', 'freeeeg128-alpha.bin', 'freeeeg128-alpha.elf', 'manifest.json'], cpu_mhz, build_id, 'standalone')
    return out


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gcc-dir', help='Directory containing arm-none-eabi-gcc')
    parser.add_argument('--jobs', type=int)
    parser.add_argument('--cpu-mhz', type=int, choices=CPU_PROFILES, default=80,
                        help='Application CPU clock (default: 80); USB/ADC clocks are unchanged')
    args = parser.parse_args()
    build(args.gcc_dir, args.jobs, cpu_mhz=args.cpu_mhz)
