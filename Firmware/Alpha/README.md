# FreeEEG128 application firmware

The maintained **STM32H743ZI / sixteen ADS131M08 / 128-channel** application.
Use the root FreeEEG128 menu for normal builds, flashing and configuration.
The default is 80 MHz CPU, 500 Hz sampling and gain 8 on every channel.

## Build

Requires Python 3.10+ and Arm GNU Toolchain; the root launcher installs tools as
needed. From the repository root:

```powershell
python Firmware/UF2/build.py
python Firmware/UF2/build.py --cpu-mhz 160
python Firmware/UF2/build.py --gcc-dir "C:/path/to/toolchain/bin"
```

These build the resident loader, application at `0x08020000`, combined factory HEX
and USB update UF2. Outputs: `Firmware/build/uf2/`. Successful builds are also saved
in dated `Firmware/build/history/` folders, with the clock profile and build ID.

`python Firmware/Alpha/build.py` instead makes a **standalone SWD application at
0x08000000** in `Firmware/build/release/`. That image replaces any resident loader
and cannot request UF2 mode. Use the combined UF2 build for the normal update workflow.

`prepare.py` verifies the pinned FreeEEG128 archive SHA256, materializes vendor
sources, builds a reduced main from its generated clock/peripheral setup, applies
USB fixes and overlays `Inc/` / `Src/`. Edit those overlays, never generated
`Firmware/build/source`. Builds require no make, Docker or network downloads.
Vendor notices remain in the pinned archive and generated files.

The deterministic build ID fingerprints prepared source, compiler and flags;
`!INFO` reports the same ID used by USB update verification. Manifests include
image sizes/hashes, compiler, clock profile and hardware-validation status.

## ADS131M08 acquisition

`Src/ads131_board.c` handles actual ADS131M08 frames and registers; it does not use
AD7771 register addresses or the old SAI path. SPI2/3/4/5 run in mode 1 at 8.192 MHz
from the separate PLL2 clock (nominal 8 MHz on HSI fallback). Four ADCs are read concurrently, in four rounds,
with GPIO chip selects held throughout each 30-byte transfer. The original
`devices[0..15]` order is retained, including SPI2/SPI3's nonsequential select wiring.
Each converter supplies eight consecutive channels.

Every transfer contains response + eight 24-bit samples + output CRC. Register
read responses are taken from the next frame; writes are read back. Output CRC-16
CCITT is checked. Configuration readback errors prevent START/SAVE. SPI errors,
CRC failures and 10 ms DMA timeouts discard the incomplete acquisition. A new
DRDY during an unfinished acquisition also discards it to avoid combining samples
from adjacent conversion periods. Confirm alignment electrically on hardware.

All ADCs share the external 8.192 MHz clock and SYNC/RESET signal. HR mode and
OSR 16384/8192/4096/2048 produce 250/500/1000/2000 Hz. A short SYNC pulse aligns
filters after rate changes and START. CHECK actively drains ADC1 during rate measurement, because unread FIFO data
can otherwise halve DRDY indications.
The driver drains FIFO data during filter settling and only then queues samples. The external reference setting follows the existing
REF2025 board. [TI datasheet](https://www.ti.com/lit/ds/symlink/ads131m08.pdf).

## USB serial controls

VID/PID `0483:5740`, UID-based serial and product **FreeEEG128 EEG + Control**.
OTG_HS operates at full speed through its embedded PHY on PB14/PB15; PC0 controls
the USB isolator. HSI48 is disciplined from USB1 SOF through CRS. Baud settings on
the virtual COM port do not change ADC rate.

Close EEG software using the port. Use menu **6**, or:

```powershell
python Firmware/Alpha/control.py --list
python Firmware/Alpha/control.py --port COM5 "RATE 1000" "GAIN 128 16" SETTINGS --resume
```

Serial commands start with `!` and end in CR, LF or CRLF. Every prefixed command
pauses acquisition, including invalid commands. Explicit `!START` resumes it.
Unprefixed traffic is ignored. USB interrupts enqueue bytes; parsing, ADC access
and replies run in main. Bounded queues reject truncated commands after overflow.

| Command | Effect |
| --- | --- |
| `!HELP`, `!HELP SETTINGS`, `!HELP PRESETS`, `!HELP DIAG` | Usage and units |
| `!INFO` | Firmware/build identity, CPU/AHB MHz, silicon revision, update mode and active PLL input (`clock=HSE`, `HSI` or `CSI`) |
| `!STATUS` | Rate, complete captures, missed DRDY, DMA/CRC errors, USB/UART drops, ADC fault and readback masks |
| `!CHECK` | Measure ADC1 DRDY for 500 ms; read MODE/CLOCK/gain registers on all sixteen ADCs; passing mask `0xffff` |
| `!DIAG` | Measured streaming-window DRDY/capture rates, maximum capture/latency/work times, wait fraction, reset flags |
| `!SETTINGS` | Rate and all 128 gains, saved/default source and dirty flag |
| `!RATE 250`, `!RATE 500`, `!RATE 1000`, `!RATE 2000` | Set all ADCs' OSR and synchronize |
| `!GAIN 128 16` | Set channel 128 gain to 16; channels 1–128, gains 1/2/4/8/16/32/64/128 |
| `!PRESET DEFAULT` | 500 Hz / gain 8, unsaved |
| `!PRESET FAST` | 2000 Hz / gain 8, unsaved |
| `!PRESET LOWGAIN` | 1000 Hz / gain 1, unsaved |
| `!SAVE` | Persist verified rate and every channel gain |
| `!LOAD` | Apply saved settings, or defaults if no valid record exists |
| `!FACTORY` | Apply defaults, erase the journal and save defaults |
| `!STOP`, `!START` | Pause/resume; STOP reports previous streaming state |
| `!REBOOT` | Reset and reload saved settings |
| `!BOOTLOADER` | Acknowledge, then reset into UF2; standalone firmware returns an installation-required error |
| `!SPIREAD 16 4` | Read one 16-bit register, ADC 1–16 / register 0–62, decimal |

Raw SPI writes are rejected: they could change ADC word length or clocks behind
the acquisition engine. Use RATE/GAIN for verified changes. CHECK measures ADC1's
DRDY, not sixteen separate DRDY signals; verify inter-converter alignment on the bench.

## Persistent settings

Settings are explicitly saved in bank 2 sector 6, `0x081c0000–0x081dffff`, outside
UF2 staging. Each 96-byte journal record contains 128 gain codes, rate, sequence,
CRC and inverse. Three aligned 32-byte flash words are written once each, with
commit data last. Torn/ECC records are consumed and skipped; previous valid
records survive. There are 1365 record slots. An identical save causes no wear.
A full journal returns an error instead of erasing settings. Back up, use FACTORY
to recycle the journal, then restore the backup. UF2 and sparse factory images
preserve settings; full-chip erase does not.

## Packet format and throughput

Each **393-byte** frame: `A0`, 8-bit sequence, **128 big-endian signed 24-bit
samples**, six reserved zero bytes, `C0`. ADC index first, then channel 0–7.
The 16-frame USB queue batches at most five frames (1965 bytes) into the owned
2048-byte CDC buffer. USB busy does not stall acquisition; full queues count drops.
Packet framing is independent of USB transfer boundaries. Host menu 8 checks
framing, sequence continuity, delivery rate and firmware error-counter changes.

Use USB for complete streams. At 2000 Hz, payload is 786000 bytes/s before USB
overhead; sustained delivery needs hardware testing. The original UART is retained
as a best-effort mirror with drop counters and cannot carry all 128-channel rates.
Existing FreeEEG32 readers expect 105 bytes and need adaptation; the retained
OpenVIBE driver has not been upgraded to decode this 128-channel protocol.

## Clock profiles and hang recovery

40/80/160/240/320/400/480 MHz CPU profiles use exact fractional PLL1 settings for
the board's 8.192 MHz HSE input in bypass mode, supplied by the active U7 oscillator.
HAL and CMSIS receive the same explicit `HSE_VALUE=8192000` build setting. This is
a different circuit from FreeEEG32's passive 16 MHz Y2 crystal.

If HSE startup fails, PLL1 retries from nominal 64 MHz HSI with matching CPU/bus
profiles. PLL2 switches divisors too, keeping ADC SPI at nominal 8 MHz instead of
overclocking it. HSI48+CRS still supplies USB, and USART1 retains HSI. `!INFO` and
support-report INFO replies expose the source actually selected. HSI-derived timer
measurements inherit the internal oscillator's tolerance; do not treat fallback
as a replacement for the ADC's external clock. If that clock is absent, ADC checks
must fail and recording cannot be relied on. The USB loader uses HSI at 80 MHz
without waiting for HSE, so software recovery remains available.

AHB stays at 40–60 MHz; sampling does not follow CPU or SPI selection.
480 MHz requires revision V; older silicon falls back to 400 MHz. The loader always
uses HSI at 80 MHz. Instruction caching is enabled, data caching remains disabled for DMA
coherency. Optional ordinary Sleep uses `FREEEEG128_IDLE_WFI=1`; default is spin,
following the FreeEEG32 fix for unreliable WFI wakeup. No Stop mode is used.

The main-loop watchdog is fed after work returns, nominal 32.768 s with LSI
tolerance. It does not start in the loader and does not modify option bytes.
`!DIAG` reports reset flags. Wait fraction describes time in the idle wait, not
measured CPU utilization. Physical temperatures remain unmeasured.

## Verification

```powershell
python -m unittest discover -s Firmware/Alpha/tests -v
python -m unittest discover -s Firmware/UF2/tests -v
python -m unittest discover -s Flash/tests -v
python Firmware/Alpha/tests/run_native.py
```

The native runner uses MSVC on Windows or GCC/Clang on Linux. Tests simulate
register timing/CRC, all ADC selects/gains, acquisition/USB/control boundaries,
settings interruption recovery, watchdog, clock limits/startup/fallback and actual UF2 updater
power cuts. [Validation and hardware checklist](../VALIDATION.md).
