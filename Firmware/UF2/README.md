# FreeEEG128 UF2 loader and build tools

Install **freeeeg128-factory.hex** once through ST-Link. Then update the application
through the existing PB14/PB15 USB data connector. This loader is ported from the
FreeEEG32 upgrade, with a separate FreeEEG128 board identity, correct 8.192 MHz HSE
clock configuration with HSI fallback/source reporting, an HSI-only recovery
clock, and **PC0** USB isolator control.

## Build and install

From the repository root:

```powershell
python Firmware/UF2/build.py
python Firmware/UF2/build.py --cpu-mhz 160
python Firmware/UF2/build.py --gcc-dir "C:/path/to/toolchain/bin"
```

The root FreeEEG128 launcher provides automatic setup and the same commands:
menu **2** builds; **3** flashes a compiled image; **4** builds and flashes;
**5** updates over USB. [Windows](../../Flash/Windows/README.md) /
[Linux](../../Flash/Linux/README.md) command options.

| Output in Firmware/build/uf2 | Purpose |
| --- | --- |
| `freeeeg128-factory.hex` | Combined resident loader + application; first install/recovery with ST-Link |
| `freeeeg128-application.uf2` | Normal USB application update |
| `freeeeg128-application.hex` | Relocated application and commit record for SWD |
| `freeeeg128-bootloader.hex` | Loader only; does not install the application |
| `freeeeg128-uf2-release.zip` | Precompiled package, launchers, helpers, wiring and manifest |
| `manifest.json` | Compiler, clocks, build identity, hashes and validation status |

Factory means this fork's combined first-install/recovery package, separate from
ST ROM boot. Source edits require rebuilding. Normal updates preserve the loader
and saved settings; full-chip erase removes them. If upgrading an earlier loader
to the new HSI-only recovery clock, reinstall the factory HEX through ST-Link;
copying an application UF2 alone does not update the resident loader.

## Windows: automatic setup and build/flash

Use the **[Windows command guide and named launchers](../../Flash/Windows/README.md)**
for source builds, precompiled flashing, USB updates and dry-run commands.

The launcher detects Python 3.10+ and Arm GCC. Missing Python/compiler packages
are installed through Windows **winget**. Missing OpenOCD is downloaded as a
portable xPack 0.12.0-7 archive into `.tools/` and checked against the publisher's
pinned SHA256 before extraction. It does not replace USB drivers automatically.
If a probe is not recognized, install ST's
[ST-Link USB driver](https://www.st.com/en/development-tools/stsw-link009.html)
or the drivers bundled with CubeProgrammer. Winget itself comes with Windows
[App Installer](https://learn.microsoft.com/windows/package-manager/winget/).

Serial-update helpers use a local `.venv` with `pyserial==3.5`. Tool setup does
not flash hardware. `flash` / `build-flash` program through SWD; `usb-update`
installs firmware over USB. A plain `build` compiles
without touching the board. `flash.cmd` also supports dropping a HEX/BIN/ELF onto
it and automatically invokes the flash tooling setup.

## Linux: automatic setup and build/flash

Use the **[Linux command guide and named launchers](../../Flash/Linux/README.md)**
for source builds, precompiled flashing, USB updates and dry-run commands.

Missing packages are installed through apt with sudo: Python/venv, OpenOCD, and
(for source builds only) `gcc-arm-none-eabi` plus newlib. The host serial package
is installed in `.venv`, not into system Python. Other Linux distributions can
install those tools through their own package manager and use the Python commands
below. Their automatic package installation is not implemented.

OpenOCD's packaged udev rules normally provide probe access. If permission is
denied, install the packaged `60-openocd.rules` into `/etc/udev/rules.d/`, reload
udev rules, and reconnect the probe. For serial access, add your account to the
distribution's serial-access group (usually `dialout`) and log in again. Do not run
an entire source build as root just to access the probe.

## Using only a precompiled release

Extract `freeeeg128-uf2-release.zip` into its own folder and keep `Flash/`, `tools/`
and the Python helpers together. Open `FreeEEG128.cmd` or run `bash FreeEEG128.sh`
from that folder. Flashing, USB updates, configuration and board checks use the
bundled helpers. Source-build actions require the repository checkout.

Every UF2 build produces a ZIP. Default 80 MHz builds also refresh the tracked
`Precompiled/` downloads. On a fresh clone, menu 3 uses its factory HEX and menu 5
uses its application UF2; a local build takes priority. Start with the ZIP's README,
CONTROL and WIRING guides; validation and vendor licenses are included offline.

The included GitHub workflow builds all seven profiles and tests Windows/Linux
helpers. After successful pushes to `main`, it refreshes the tracked downloads and
[rolling alpha release](https://github.com/joshbrew/FreeEEG128-alpha-firmware-upgrade/releases/tag/latest-alpha).
Publishing begins when the workflow is pushed. Other clock profiles remain workflow
artifacts; generated intermediates remain ignored. Pull the automatic download
commit before your next push.

For multiple boards, follow the [stacking guide](../../SetupGuides/FreeEEG128_Stacking.md)
for clock/sync isolation, the three shared contacts, and the startup order.

## Wiring and entry

[Target SWD connections](../../README.md#swd-programming): SWDIO → **J1.4 / PA13**;
SWCLK → **J1.3 / PA14**; NRST → **J1.2**; GND → **J2.2**. **J1.1 is PB2, not ground.**
Power normally. Probe VTref, if present, needs measured target 3.3 V I/O supply;
the 5 V input is not VTref. Disconnect electrodes during programming.
[Full wiring and probe guide](../../SetupGuides/FreeEEG128_Programming_and_Updates.md).

After installation, send `!BOOTLOADER`, choose menu 5, or press **RESET twice within
600 ms**. Copy the application UF2 to the **FREEEEG128** drive. The helper also
accepts a board already in loader mode. BOOT enters ST ROM boot, which uses
PA11/PA12 for USB DFU and cannot use this board's PB14/PB15 connector.

The synthetic update disk exposes `INFO_UF2.TXT` and `STATUS.TXT`.
Board-ID is **FreeEEG128-alpha-H743ZI-v1**. Board word **0x38314546** differs from
FreeEEG32's 0x32334546. Host tools and loader reject the other board's images even
though both use the registered STM32H7 UF2 family. After a complete verified upload
and 1.5 seconds without storage writes, the device disconnects and resets to
install. Files are not persistent storage. CRC detects corruption; images are
not signed and the loader is not secure boot.

A missing/invalid app stays in the loader. An interrupted upload can be retried by
copying the complete UF2. An interrupted install retries the intact verified
bank-2 stage after reset. SWD can reinstall the factory image for loader recovery.

## Flash layout and recovery

| Address range | Use |
| --- | --- |
| `0x08000000–0x0801FFFF` | 128 KB resident loader, bank 1 sector 0 |
| `0x08020000–0x080DFFFF` | Up to 768 KB application, bank 1 sectors 1–6 |
| `0x080E0000` | App commit record, sector 7 |
| `0x08100000–0x081BFFFF` | Update staging, bank 2 sectors 0–5 |
| `0x081C0000–0x081DFFFF` | Application settings journal; preserved by UF2 and factory images |
| `0x081E0000` | Staged commit record, bank 2 sector 7 |

Every 256-byte payload block has bounds, family, board/layout, block-number/count
and duplicate checks. A received-block bitmap accepts out-of-order writes. The
whole image, board header and vectors must pass before the staged commit record
is written. The application record is invalidated before installation and written
last after readback. No update erase/write can target loader sector 0. This is
software address protection; the installer does not change hardware option bytes.

Flash programming uses aligned 32-byte H743 flash words. Loader reads probe for
double-bit ECC BusFaults left by interrupted writes. The loader has caches disabled,
polls the ST USB stack from main, and performs flash operations outside USB ISR
context. Bank-swap mode and chips without 2 MB flash are rejected for writes.
CRC detects corruption; this is not signed firmware or secure boot.

## CPU clock profiles

Application choices: **40/80/160/240/320/400/480 MHz**, default **80**. The loader
always uses **80 MHz from HSI**, independently of the external oscillator.
The application normally uses **8.192 MHz HSE in bypass mode** with fractional PLL1
settings for exact nominal CPU MHz. If HSE startup fails, it retries with HSI.
Both HAL and CMSIS compile with the same 8.192 MHz HSE constant. PLL2 switches to
safe fallback divisors: SPI is normally 8.192 MHz, nominal 8 MHz on HSI. USB stays
on HSI48+CRS and USART1 on HSI. `!INFO` reports the active PLL input through
`clock=HSE` / `clock=HSI` / `clock=CSI`. A missing ADC oscillator still prevents
valid acquisition; fallback is for recovery and has HSI timing tolerance. AHB stays above
USB's 30 MHz minimum. 480 MHz requires revision V and falls back to 400 MHz on
other revisions. Sampling remains 250/500/1000/2000 Hz regardless of CPU choice.
Query `!INFO` for actual/requested MHz and revision.

| CPU requested | AHB | APBs | Voltage scale | Notes |
| --- | --- | --- | --- | --- |
| 40 MHz | 40 MHz | 20 MHz | VOS3 | Lower CPU clock |
| 80 MHz | 40 MHz | 20 MHz | VOS3 | Default |
| 160 MHz | 40 MHz | 20 MHz | VOS3 | |
| 240 MHz | 60 MHz | 30 MHz | VOS2 | One flash wait state |
| 320 MHz | 40 MHz | 20 MHz | VOS1 | |
| 400 MHz | 50 MHz | 25 MHz | VOS1 | Maximum offered for non-V revisions |
| 480 MHz | 60 MHz | 30 MHz | VOS0 | Rev V only; other revisions automatically use 400 MHz |

## Verification and hardware checklist

Run `python Firmware/Alpha/tests/run_native.py` for actual updater simulations,
including all 51 upload and 51 install power-cut points in the fixture, ECC faults,
reordered/duplicate/bad/missing blocks and FAT12 reads. Run
`python -m unittest discover -s Firmware/UF2/tests -v` for packaging checks.
Host simulations cannot verify electrical timing, USB enumeration or real flash
power-loss behavior. [Validation record](../VALIDATION.md).

[Application controls and stream format](../Alpha/README.md#usb-serial-controls) ·
[UF2 format](https://github.com/microsoft/uf2) ·
[ST ROM boot reference](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf).
