# Pinned FreeEEG128 build input

`STM32H743ZI_ads131_80MHz_makefile.tar.gz` is the original FreeEEG128 source
archive, preserved without modification. SHA256:

```
b288351888cc8145a18c6b43ed905d9a9197daf5e3e17843cde82b197189634a
```

`Alpha/prepare.py` verifies the archive, normalizes its legacy line endings,
reuses its STM32 HAL/CMSIS/USB stack and generated peripheral configuration,
replaces the experimental main loop with the maintained application, and applies
the USB ownership/enumeration fixes carried over from FreeEEG32.
The new ADS131M08 driver is in `Alpha/Src/ads131_board.c`; the AD7771 driver is not used.
Vendor notices remain in the source archive and generated source files.

The original [STM32 HAL license](licenses/ST-HAL.md) and [CMSIS license](licenses/CMSIS.txt)
are also stored here so release bundles can be repackaged without generated build
sources. Their bytes match the corresponding files in the pinned archive.
