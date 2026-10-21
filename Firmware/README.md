# Maintained FreeEEG128 firmware

Start with `FreeEEG128.cmd` or `bash FreeEEG128.sh` in the main folder.
[Windows options](../Flash/Windows/README.md) / [Linux options](../Flash/Linux/README.md).
The normal build creates both the first-install SWD factory image and subsequent
USB UF2 application updates.

| Folder | Purpose |
| --- | --- |
| [Alpha](Alpha/README.md) | Editable ADS131M08 acquisition, USB controls, 128-channel settings and host helpers |
| [UF2](UF2/README.md) | Resident USB loader, packaging, update recovery and flash layout |
| `tools/` | Shared platform launchers, automatic setup, OpenOCD configuration and layout checker |
| [Base](Base/README.md) | Original source archive, checked against its pinned SHA256 before building |
| [Legacy](Legacy/README.md) | Preserved original STM32CubeIDE workspace, including historical build outputs |
| `build/` | Ignored generated sources, images, manifests, test executables and release ZIPs |

Edit `Alpha/Inc` and `Alpha/Src` for application changes, or `UF2/Inc` and `UF2/Src`
for loader changes. `Alpha/prepare.py` reuses the original vendor code and peripheral
configuration, applies checked USB changes, and overlays maintained sources.
Every build regenerates `build/source`; edits there will be overwritten.

`python Firmware/UF2/build.py` creates the loader and relocated application.
`python Firmware/Alpha/build.py` is the standalone SWD-only alternative and
replaces an installed loader. The normal launchers use the UF2 build.

Successful builds are retained under `build/history/` with UTC date, CPU profile,
build ID and manifest. The latest outputs are under `build/uf2/`. Default 80 MHz
builds also refresh the tracked [ready-to-flash downloads](../Precompiled/README.md).
The included GitHub workflow checks every profile and both platform launchers
before updating these downloads and the rolling alpha release on `main`.
Downloaded tools stay in root `.tools/`; host Python dependencies stay in `.venv/`;
reports and settings backups stay in `reports/`. These generated folders are ignored.

[FreeEEG32 update audit](UPSTREAM.md) / [Validation record and commands](VALIDATION.md) / [Source and release layout checks](tools/README.md).
