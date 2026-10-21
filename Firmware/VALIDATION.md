# FreeEEG128 upgrade validation

Recorded 2026-10-21. **No physical FreeEEG128 has been flashed or bench-tested by
this work.** Simulation results do not establish electrical timing, signal quality,
ADC alignment, USB throughput or safe recovery from real power loss.

## Automated checks

| Check | Result |
| --- | --- |
| Application host helpers | 46 Python tests pass: discovery, identity checks, USB update verification, controls, 128-channel settings files, recording and build history |
| UF2 packaging | 11 Python tests pass, including FreeEEG32 rejection, verified download/checksum refresh, corruption/duplicate rejection and default-profile enforcement |
| Platform launchers and repository/release layout | 13 tests pass: six organization/package-integrity regressions plus Windows and Bash wrappers, quoted arguments/exit codes, shared helpers in repository and extracted-release layouts. Bash tested through Git Bash on Windows; native Linux setup remains untested |
| ADS131M08 driver simulation | 16 select mappings, delayed register responses, CRC vector/corruption, all four rates, all 128 gains, four parallel DMA rounds and transport failure paths pass |
| Application simulation | 128-channel command bounds, replies exceeding 512 bytes, ADC checks, 393-byte packets, 1965-byte transfer bound, queue drops, settling reads and fault gating pass |
| Settings journal simulation | All 128 gains, unchanged-save wear avoidance, each of three torn-word cuts, CRC fallback, journal-full behavior and factory reset pass |
| Watchdog simulation | Main-loop cadence, long work, tick wrap and initialization failure pass |
| Clock simulation | Native suite now has 26 executions; all seven profiles pass for revision V/older silicon: exact fractional HSE calculations, nominal HSI CPU/SPI rates, external-clock failure, HSI failure, active-source reporting and application/loader modes |
| UF2 updater simulation | Actual updater passes reordered/duplicate/missing/bad blocks, CRC rejection, FAT12 reads, 51 upload and 51 install interruption points, ECC faults and preservation of loader/settings |
| Arm GNU 14.2.1 build | Loader + relocated application builds checked at all seven profiles (40/80/160/240/320/400/480 MHz). Standalone application build checked separately |
| Release contents | UF2 re-read by the USB updater's validator; image hashes, sparse HEX addresses, launchers, controls, wiring and licenses checked |
| Wiring diagram | Supplied photo matches the repository original; J1/J2 pin numbers checked against the PCB. SVG rendered to PNG and visually reviewed; all 638 relocated originals match their original Git blobs |

Run from the repository root:

```powershell
python -m unittest discover -s Firmware/Alpha/tests -v
python -m unittest discover -s Firmware/UF2/tests -v
python -m unittest discover -s Flash/tests -v
python Firmware/Alpha/tests/run_native.py
python Firmware/UF2/build.py
python Firmware/Alpha/build.py
```

The organization audit also checks every maintained Markdown file/section link,
required source/release contents, UTF-8 text, image hashes, missing helpers and
both ZIP/extracted layouts. Release creation runs these checks automatically.
To repeat the layout checks without compiling or accessing hardware:

```powershell
python Firmware/tools/check_layout.py
python Firmware/tools/check_layout.py --release Firmware/build/uf2/freeeeg128-uf2-release.zip
```

The native runner uses MSVC Build Tools on Windows or a host C compiler on Linux.
The Arm linker retains inherited newlib syscall/RWX segment warnings and the
loader build reports unused-parameter warnings in the pinned ST HAL. Those logs
are in `Firmware/build/uf2/` and `application/`; no warning is a hardware result.

## Required hardware checks

- Verify J1/J2 orientation and target 3.3 V reference; install and read back the factory image over ST-Link.
- Enumerate CDC and MSC repeatedly on Windows and Linux, including cable replug and USB suspend/resume.
- Test menu-driven and double-RESET UF2 entry, image/build verification and subsequent CDC streaming.
- Exercise incomplete UF2 copies and interrupted stage/install writes on a recoverable bench setup.
- Check measured 250/500/1000/2000 Hz, all sixteen converter identities, gain registers, analog response and channel order/alignment.
- Run sustained 128-channel recordings at each rate, including 786000 bytes/s at 2000 Hz; inspect missed/DMA/USB counters.
- Confirm START after a pause, common SYNC, settling reads and FIFO behavior with a logic analyzer.
- Check settings SAVE, reboot/LOAD, backup/restore, factory reset and interrupted SAVE on hardware.
- Confirm watchdog recovery, reset reasons, all claimed clock profiles/revisions and physical temperatures.

The legacy OpenVIBE FreeEEG32 driver is preserved but has not been converted into
a 128-channel reader. The new host recording check understands the 393-byte format.

Latest clock updates are traced in the [FreeEEG32 audit](UPSTREAM.md). Hardware
checks must also cover HSE failure, HSI fallback, the HSI-only loader and matching
INFO source reporting; FreeEEG32 crystal bench results do not validate U7 here.

## Download and publishing parity

Verified FreeEEG32 remote HEAD `347dc08` before this port. Default 80 MHz builds
refresh the tracked ZIP/HEX/UF2/manifest and SHA256SUMS only after package checks;
other profiles leave those downloads intact. Fresh-clone defaults were tested with
Windows PowerShell and Git Bash. The added GitHub workflow targets this repository
and its `main` branch, gates publishing on all build/helper jobs, and avoids force
pushing generated commits. GitHub Actions and release publishing have not been
executed from this working tree; they begin after these changes are pushed.
