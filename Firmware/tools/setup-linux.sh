#!/usr/bin/env bash
set -euo pipefail
task_tools="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
task_root="$(dirname -- "$task_tools")"
if [[ -d "$task_root/Alpha" ]]; then task_root="$(dirname -- "$task_root")"; fi
task_mode="${1:-full}"
task_summary=()
task_had_python=0; task_had_openocd=0; task_had_gcc=0
command -v python3 >/dev/null 2>&1 && task_had_python=1
command -v openocd >/dev/null 2>&1 && task_had_openocd=1
command -v arm-none-eabi-gcc >/dev/null 2>&1 && task_had_gcc=1
printf '\n%s\n' "FreeEEG128 tool setup - $task_mode" 'Checking installed tools first. Missing tools are installed automatically.'
echo '  Python 3.10+ and venv: run the build, flash and control helpers.'
if [[ "$task_mode" != --serial-only && "$task_mode" != --flash-only ]]; then echo '  Arm GCC + newlib: compile edited STM32 Cortex-M7 firmware.'; fi
if [[ "$task_mode" != --serial-only ]]; then echo '  OpenOCD: program/verify the board through ST-Link SWD.'; fi
echo '  pyserial 3.5: USB commands, port discovery and software entry into UF2 mode.'
echo "  Python helper environment: $task_root/.venv"
task_packages=()
command -v python3 >/dev/null 2>&1 || task_packages+=(python3)
if [[ "$task_mode" != "--serial-only" ]]; then
    command -v openocd >/dev/null 2>&1 || task_packages+=(openocd)
fi
if [[ "$task_mode" != "--flash-only" && "$task_mode" != "--serial-only" ]]; then
    if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
        task_packages+=(gcc-arm-none-eabi libnewlib-arm-none-eabi)
    elif [[ "$(arm-none-eabi-gcc --print-file-name=nano.specs)" == nano.specs ]]; then
        task_packages+=(libnewlib-arm-none-eabi)
    fi
fi
if ! python3 -c 'import ensurepip' >/dev/null 2>&1; then task_packages+=(python3-venv); fi
if ((${#task_packages[@]})); then
    if ! command -v apt-get >/dev/null 2>&1; then
        echo 'Automatic Linux setup supports Debian/Ubuntu. Install Python 3.10+, python3-venv, Arm GNU Toolchain/newlib and OpenOCD with your package manager.' >&2
        exit 1
    fi
    task_sudo=(); if [[ "$EUID" != 0 ]]; then task_sudo=(sudo); fi
    echo '[DOWNLOAD/INSTALL] Missing system packages and their dependencies:'
    printf '  %s\n' "${task_packages[@]}"
    echo '  Source: configured APT repositories; apt shows URLs, package sizes and progress below.'
    echo '  Destination: system package locations managed by apt (for example /usr/bin and /usr/lib).'
    echo '[METADATA] Refreshing package indexes with apt-get update.'
    "${task_sudo[@]}" apt-get update
    echo '[INSTALL] Installing the listed packages. Download and installation progress follows.'
    "${task_sudo[@]}" apt-get install -y "${task_packages[@]}"
fi
python3 -c 'import sys; assert sys.version_info >= (3,10), "Python 3.10+ required"'
task_status=reused; if [[ "$task_had_python" == 0 ]]; then task_status=installed; fi
task_summary+=("Python $task_status: $(command -v python3)")
if [[ "$task_mode" != --serial-only && "$task_mode" != --flash-only ]]; then
    task_status=reused; if [[ "$task_had_gcc" == 0 ]]; then task_status=installed; fi
    task_summary+=("Arm compiler $task_status: $(command -v arm-none-eabi-gcc)")
else echo '[SKIP] Arm compiler: this action uses an existing firmware image or USB controls.'; fi
if [[ "$task_mode" != --serial-only ]]; then
    task_status=reused; if [[ "$task_had_openocd" == 0 ]]; then task_status=installed; fi
    task_summary+=("OpenOCD $task_status: $(command -v openocd)")
fi
if [[ ! -x "$task_root/.venv/bin/python" ]]; then
    echo "[CREATE] Python helper environment: $task_root/.venv (using system Python)."
    python3 -m venv "$task_root/.venv"
fi
if ! "$task_root/.venv/bin/python" -c 'import serial' >/dev/null 2>&1; then
    echo '[DOWNLOAD/INSTALL] pyserial==3.5'
    echo '  Why: USB serial commands, port discovery and software entry into the UF2 loader.'
    echo '  Source: pip package index (PyPI by default; pip displays its configured source below).'
    echo "  Destination: $task_root/.venv (project-local Python environment)."
    "$task_root/.venv/bin/python" -m pip --disable-pip-version-check install -r "$task_tools/requirements-host.txt"
    task_summary+=("pyserial 3.5 installed: $task_root/.venv")
else
    task_summary+=("pyserial reused: $task_root/.venv")
fi
mkdir -p "$task_root/.tools"
printf '%s\n' "FreeEEG128 setup: $task_mode" "${task_summary[@]}" > "$task_root/.tools/setup-summary.txt"
printf '\n%s\n' 'Setup complete:'
printf '  %s\n' "${task_summary[@]}"
echo "Summary saved to: $task_root/.tools/setup-summary.txt"
echo 'If probe/serial access is denied, use the udev instructions in Firmware/UF2/README.md and reconnect USB.'
