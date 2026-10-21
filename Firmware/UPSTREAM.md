# FreeEEG32 update audit for FreeEEG128

Audited FreeEEG32 source (remote HEAD verified) through commit
[`347dc08`](https://github.com/joshbrew/FreeEEG32-beta-firmware-upgrade/commit/347dc08)
on 2026-10-21. The source checkout was clean during comparison. This records what
was adapted to the 128-channel board; it is not a FreeEEG128 bench result.

| FreeEEG32 update | FreeEEG128 treatment |
| --- | --- |
| `2597af7` / `347dc08`: tracked downloads and rolling release | Add verified 80 MHz factory HEX, application UF2, ZIP, manifest and SHA256SUMS; builds refresh the tracked folder, other profiles leave it intact |
| `2597af7`: fresh-clone flashing and USB updates | Windows/Linux launchers and flash helper prefer local builds, then tracked FreeEEG128 downloads; extracted ZIP defaults stay self-contained |
| `2597af7`: checked automatic publishing | Add Windows/Linux checks, all seven Arm profiles and native simulations, artifacts, plus guarded publishing on this repository's `main` branch to `latest-alpha`; workflow has not yet run on GitHub |
| `2597af7`: cross-platform tests and binary preservation | Port absolute-path test fixes and byte-preserving Git attributes for published images/checksums |
| `d971dc4`: external H7 clock and HSI startup fallback | The 128 board already used an active 8.192 MHz HSE source. Preserve bypass mode and its fractional PLL values; add a tested HSI retry with matching CPU/bus profiles |
| `d971dc4`: correct HSE frequency assumption | Keep 8.192 MHz, verified against this board's U7 BOM/peripheral configuration. Pass the same explicit value to HAL and CMSIS, whose unoverridden defaults previously differed |
| `d971dc4`: report actual clock input in INFO | Add `runtime_clock_source()` and `clock=HSE`, `HSI` or `CSI` to INFO in both standalone and UF2 applications; support reports retain these replies |
| Recovery clock independence | The FreeEEG128 loader explicitly uses nominal 80 MHz HSI without waiting for HSE. Its USB clock remains HSI48+CRS |
| Shared PLL source / peripheral preservation | Adapt PLL2 M/N on HSI fallback too. Normal SPI is 8.192 MHz; fallback SPI is nominal 8 MHz. USART1 stays on HSI and USB on HSI48+CRS |
| `2b2b9db`: correct FreeEEG32 bench recording label | Applies to FreeEEG32's 512 Hz bench result. FreeEEG128 retains its actual ADS131M08 250/500/1000/2000 Hz settings and unvalidated hardware status |
| Prior USB, WFI, watchdog, settings, queueing and host-tool fixes | Already included in this port. Shared setup/launch scripts and UF2 update/flash logic match after board naming; converter-specific acquisition/settings remain adapted to ADS131M08 |

FreeEEG32's passive 16 MHz Y2 crystal setup, AD7771 SINC3/SRC register writes,
SAI transfers, four-ADC masks, 32-channel packet length and raw AD7771 SPI writes
are not applicable to this PCB. Copying them would change the hardware assumptions.
The 128-channel equivalents remain in the ADS131M08 driver and host helpers.

## Installing this update

A normal application UF2 update installs the new application clock fallback and
INFO reporting while retaining the already installed loader. To install the new
HSI-only recovery loader too, flash the newly built `freeeeg128-factory.hex` through
ST-Link. Normal sparse factory programming preserves the settings sector; a full
chip erase does not. No hardware has been flashed automatically during this work.

## Validation

Native tests exercise the actual clock-start code with successful external-clock
startup, external-clock failure, HSI failure, source-register reporting, safe PLL2
rates and revision-V/older-silicon behavior. All seven requested CPU profiles are
checked in application and loader compilation modes. INFO tests check both HSE and
HSI replies. Arm cross-builds check the vendor HAL types and link both applications
and the loader. Physical oscillator failure, accuracy and USB recovery still need
bench validation.

[Validation commands](VALIDATION.md) / [Clock and recovery behavior](Alpha/README.md#clock-profiles-and-hang-recovery).
