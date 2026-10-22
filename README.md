# FreeEEG128-alpha

Firmware and programming tools for the **FreeEEG128 alpha / STM32H743ZI**, with
**sixteen ADS131M08 converters and 128 channels**. This port brings over the
FreeEEG32 upgrade workflow with a separate ADS131M08 acquisition driver and board identity.

## Start here

**[Ready-to-flash downloads](Precompiled/README.md)**: complete ZIP, factory HEX,
application UF2, manifest and checksums at the default **80 MHz**. Menu **3** and
**5** use these on a fresh clone; a local build takes priority. Default source
builds refresh this folder. The included workflow tests all seven clock profiles
and Windows/Linux helpers before refreshing downloads and the rolling alpha
release after pushes to `main`. Publishing begins when these changes are pushed.

**Windows:** double-click **[FreeEEG128.cmd](FreeEEG128.cmd)**.

**Linux:** open a terminal in this folder and run `bash FreeEEG128.sh`.

| Menu | Action |
| --- | --- |
| 1 | Install/check tools |
| 2 | Build firmware from edited source |
| 3 | Flash existing compiled firmware + UF2 loader with ST-Link |
| 4 | Build edited source, then flash firmware + UF2 loader with ST-Link |
| 5 | Update firmware over USB |
| 6 | Configure sampling, gains, presets and saved settings |
| 7 | Check USB, firmware and all sixteen ADCs |
| 8 | Test recording for 10 seconds |
| 9 | Save a support report |
| 10 | Back up all 128 channel settings |
| 11 | Preview and restore a settings file |

Actions install missing tools as needed. Flashing a compiled image, USB updates
and configuration do not require the compiler. Automatic Linux setup supports
Debian/Ubuntu. [Windows commands](Flash/Windows/README.md) · [Linux commands](Flash/Linux/README.md).

## First installation

1. Connect ST-Link using the [wiring diagram](#swd-programming), and power the board normally.
2. Choose **4** to build the current source and flash it. Choose **3** to flash an
   ready-to-flash `freeeeg128-factory.hex` from `Precompiled/`, a local build or release ZIP.
3. Connect the board's **USB data port** for streaming and configuration.

The factory HEX combines this fork's resident UF2 loader and EEG application.
It is a first-install/recovery package; it does not refer to ST's ROM bootloader.
Option 3 does not incorporate source edits until you build again.

Edit application sources in `Firmware/Alpha/Inc` and `Firmware/Alpha/Src`.
Edit the loader in `Firmware/UF2`. Outputs are in `Firmware/build/uf2/`, including
the factory HEX, application UF2, manifest and precompiled release ZIP.

## USB updates

After the first ST-Link installation, choose **2** to build source changes and
**5** to install the new application over USB. Close other EEG software first.
The helper identifies the board, enters the loader, finds its drive, copies
`freeeeg128-application.uf2`, waits for restart and checks the running build ID.
Choose a board if several are connected. A board already in UF2 mode is also supported.

For a manual update, press **RESET twice within 600 ms**, then copy the application
UF2 onto the **FREEEEG128** drive. The drive disappears after a complete verified
upload. Allow several seconds for installation and restart. The **BOOT** button
selects ST's ROM loader; use RESET for UF2.

The loader checks both family and board/layout identity. FreeEEG32 application
images are rejected. Normal UF2 updates and the generated factory HEX preserve
the separate settings sector; a full-chip erase does not.
[Update and recovery details](Firmware/UF2/README.md).

## Sampling and gain

Choose **6 — Configure board**. Available settings:

- Sampling: **250, 500, 1000 or 2000 Hz**; default **500 Hz**.
- Gain: **1, 2, 4, 8, 16, 32, 64 or 128**, per channel or all 128 channels; default **8**.
- Presets: DEFAULT (500 Hz / gain 8), FAST (2000 Hz / gain 8), LOWGAIN (1000 Hz / gain 1).
- Explicit save/load, restore factory settings, status and diagnostics.

The board's 8.192 MHz oscillator and the ADS131M08's power-of-two oversampling
ratios give these rates. Do not select the FreeEEG32's 512/1024/2048 Hz settings:
those belong to a different converter. [TI ADS131M08 datasheet](https://www.ti.com/lit/ds/symlink/ads131m08.pdf).

Commands pause acquisition. Choose **Save settings** to retain changes after
power-up, then **Start streaming and exit**. Unsaved changes stay in RAM until
reboot. In a serial terminal, send `!HELP`, ending commands with a newline;
`!START` resumes streaming. [Command reference](Firmware/Alpha/README.md#usb-serial-controls).

## Board checks and settings files

Choose **7** to check the firmware, observed DRDY rate and MODE/CLOCK/gain
registers across all sixteen ADCs. Choose **8** to measure received 128-channel
frames, delivery rate and missing frames over 10 seconds. This checks data delivery,
not electrode contact or EEG signal quality.

Choose **9** for a support report, **10** to back up settings, or **11** to preview
and restore a settings file. Files are saved in `reports/`; support reports include
versions/settings/error counters without EEG samples. Restore validates all
128 gains and the board identity before changing anything. Saving restored
settings across power cycles is an explicit choice. A failed restore stays paused.
Checks and backups restore the previous acquisition state. [Helper details](Flash/HELPERS.md).

## SWD programming

![FreeEEG128 alpha photo and verified ST-Link wiring](SetupGuides/images/freeeeg128-swd.svg)

| Signal / source | FreeEEG128 connection |
| --- | --- |
| SWDIO | **J1 pin 4 — PA13** |
| SWCLK | **J1 pin 3 — PA14** |
| NRST / STM32 RST | **J1 pin 2 — NRST**, beside RESET |
| GND | **J2 pin 2 — GND**, beside BOOT |
| Separate board 5 V supply | **J2 pin 4 — 5 V power input**, above BOOT |
| VTref / VAPP input, if present | Measured **3.3 V target I/O supply** |

**J1 pin 1 (square pad) is PB2 / BOOT1, not ground.** J2 pin 4 is the
5 V input, not VTref. Power the board normally; leave probe power outputs
disconnected. Keep electrodes disconnected during programming.

The enlarged detail turns the supplied photo 45° to match the FreeEEG32 guide:
**5 V/GND above BOOT; SWD below RESET**, with the same pin positions. On the lower
SWD header, SWDIO is upper left, SWCLK upper right and NRST lower left; on the
upper power header, 5 V is upper left and GND lower left. Individual leaders point
to the metal pin bases. Pin numbers are checked against the versioned KiCAD PCB;
views from the opposite side are mirrored.
[Detailed wiring, clone dongles and recovery](SetupGuides/FreeEEG128_Programming_and_Updates.md).

## Stacking boards

Use one board's **8.192 MHz U7 clock** for the stack. No CPU-clock setting or
separate master/follower firmware is needed. Followers need their local oscillator
and sync links isolated; each board retains its own USB data connection.

**[Stacking instructions: wiring, configuration and start order](SetupGuides/FreeEEG128_Stacking.md)**

## Features added by this fork

| Feature | FreeEEG128 implementation |
| --- | --- |
| USB firmware updates | One ST-Link installation, then USB UF2 updates through PB14/PB15 |
| Ready-to-flash downloads and releases | Tracked 80 MHz ZIP/HEX/UF2, SHA256 checksums, fresh-clone defaults and checked automatic publishing |
| Build and flash launchers | Windows/Linux menus, automatic tool setup, precompiled-image support |
| Board detection and update checks | Serial/drive detection, multi-board selection and running-build verification |
| Multi-board stacking guide | Board-specific R112/R110 isolation, shared clock/sync/GND contacts, 500 Hz setup and separate USB streams; physical stack validation pending |
| Settings menu and serial commands | ADS131M08 rates, 128 per-channel gains, presets and explicit save/load |
| Board and recording checks | Sixteen-ADC register readback, measured DRDY/USB rates, frame-loss checks |
| Support reports and settings files | Sample-free support reports; validated 128-channel backup/restore |
| Hang recovery | Main-loop watchdog, reset reason and ADC fault diagnostics |
| CPU clock selection and recovery | 40/80/160/240/320/400/480 MHz profiles, default 80 MHz; external 8.192 MHz clock with HSI fallback, active-source reporting and an HSI-only recovery loader |
| Acquisition and transfer fixes | Four parallel SPI buses, four ADC rounds, complete 30-byte transfers, CRC checks and timeouts |
| USB buffering and batching | 16 complete 393-byte frames; at most five per transfer, with drop/error counters |
| CPU idle sleep | Optional interrupt-driven Sleep, off by default; acquisition/USB clocks retained |
| Update recovery | Board/layout checks, whole-image CRC, staged install and retry after interrupted flash writes |
| Repository organization | Maintained application/loader/tools separated from original IDE projects, hardware and setup guides; source and offline-release layouts checked during packaging |

Latest [FreeEEG32 update audit](Firmware/UPSTREAM.md): clock-source reporting and
startup recovery are adapted to this board's 8.192 MHz active oscillator. `!INFO`
reports `clock=HSE` normally or `clock=HSI` on fallback. ADC rates and alignment
still require the board oscillator; fallback preserves a software recovery path.

The 480 MHz profile requires revision V silicon and falls back to 400 MHz on
other revisions. Clock selection does not change sampling rate. Full 128-channel
output requires USB: the legacy UART speed cannot carry all offered rates.
The stream is **393 bytes per frame**, with 128 signed 24-bit samples; software
expecting FreeEEG32's 105-byte packets needs a 128-channel reader. The existing
`OpenVIBE/freeeeg32` integration is legacy and is not a 128-channel decoder.

**Hardware status:** this port has not been validated on a physical FreeEEG128.
Host simulations and cross-build results are recorded in
[VALIDATION.md](Firmware/VALIDATION.md). Verify SWD, USB enumeration/replug, both
UF2 entry methods, interrupted updates, rates, gain, channel order/alignment and
sustained throughput before relying on a recording. FreeEEG32 bench results do
not validate this board.

## Why ROM DFU cannot use the data connector

The data connector uses **PB14/PB15**. The STM32H743 ROM USB DFU loader uses
**PA11/PA12**, so the resident loader must first be installed through SWD.
[ST ROM bootloader reference](https://www.st.com/resource/en/application_note/an2606-stm32-microcontroller-system-memory-boot-mode-stmicroelectronics.pdf).

## Files and documentation

- [Firmware](Firmware/README.md): maintained Alpha application, UF2 loader, pinned base and legacy IDE sources.
- [Flash](Flash/README.md): Windows/Linux workflows and helper commands.
- [SetupGuides](SetupGuides/README.md): wiring, updates and original board photos.
- [AssemblyFiles](AssemblyFiles/README.md): KiCAD, Gerbers, BOM and datasheets.
- [Repository layout and moved-file map](SetupGuides/Repository_Layout.md).
- [OpenVIBE compatibility](OpenVIBE/README.md).
- `Firmware/build/`: generated outputs and dated build history; `.tools/` / `.venv/`: local tools.

[Upstream FreeEEG128](https://github.com/neuroidss/FreeEEG128-alpha)
