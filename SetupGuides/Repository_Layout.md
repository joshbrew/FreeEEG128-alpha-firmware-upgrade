# FreeEEG128 repository layout

The repository uses the same entry-point and folder organization as the FreeEEG32
upgrade. FreeEEG128's maintained application is named **Alpha**, matching this
board revision. Its converter driver, channel count, clock setup and image identity
remain specific to FreeEEG128.

```text
FreeEEG128.cmd / FreeEEG128.sh     Main menu
README.md                        Start here; feature table and wiring
Precompiled/                     Tracked default 80 MHz images, ZIP and checksums
.github/workflows/firmware.yml    Cross-platform tests, seven builds and publishing
Flash/
  Windows/                       Named Windows actions and guide
  Linux/                         Matching Bash actions and guide
  HELPERS.md                     Checks, reports and settings files
Firmware/
  Alpha/Inc, Src                 Editable application and ADS131M08 driver
  Alpha/tests                   Host and native acquisition/control tests
  UF2/Inc, Src, vendor            Editable loader and pinned USB MSC middleware
  UF2/release.py                 Offline package builder and link relocation
  UF2/precompiled.py             Verify and refresh tracked default downloads
  UF2/tests                     UF2 packaging/update tests
  Base/                         Pinned original archive and vendor licenses
  Legacy/STM32CubeIDE/           Preserved historical IDE workspace
  tools/                        Shared setup, action routing and layout checker
  VALIDATION.md                 Tested behavior and outstanding bench checks
  build/                        Generated source, images, tests and build history
AssemblyFiles/
  KiCAD/                        Original project and fabrication/assembly outputs
  datasheets/                   Original component datasheets
SetupGuides/
  FreeEEG128_Stacking.md         Shared-clock/sync wiring and multi-board setup
  images/                       Original photos, screenshots and new wiring diagram
OpenVIBE/                        Historical integration and compatibility note
```

## Original files and their destinations

| Original root location | Current location |
| --- | --- |
| `STM32CubeIDE/` | `Firmware/Legacy/STM32CubeIDE/` |
| `STM32H743ZI_ads131_80MHz_makefile.tar.gz` | `Firmware/Base/STM32H743ZI_ads131_80MHz_makefile.tar.gz` |
| `KiCAD/` | `AssemblyFiles/KiCAD/` |
| `datasheets/` | `AssemblyFiles/datasheets/` |
| Board photographs and historical screenshots | `SetupGuides/images/` |
| `OpenVIBE/` | Retained in place, with a compatibility README |

All 638 relocated originals retain their original Git content. Historical IDE
metadata and binaries stay in the legacy tree. Original schematic/PCB references
stay together. New application sources and generated releases have separate homes.

## Editable files and generated outputs

Edit the application in `Firmware/Alpha/Inc` and `Src`, and the loader in
`Firmware/UF2/Inc` and `Src`. Every build recreates `Firmware/build/source` from
the pinned archive; edits there are overwritten. The standalone build is an
explicit SWD-only alternative; the main menu builds the UF2 workflow.

`Firmware/build/`, `.tools/`, `.venv/` and `reports/` are ignored. They hold generated
images/tests/history, downloaded tools, host dependencies, and reports/backups.
The default 80 MHz release is also copied into tracked `Precompiled/` for fresh
clones. Its ZIP/HEX/UF2/manifest are verified before replacement; SHA256SUMS covers
the downloads. Other profiles do not replace the default. After successful `main`
pushes, the included workflow commits downloads and updates a rolling alpha release.
Pull that generated commit before pushing again. [Download guide](../Precompiled/README.md).

## Precompiled release layout

Extract the release ZIP into its own folder. Its root contains the same main menu,
firmware images/manifest, Python helpers, README, CONTROL, WIRING, STACKING and VALIDATION.
`Flash/` holds the same named actions, `tools/` holds their shared setup/routing,
and `licenses/` holds vendor notices. The wiring images are included for offline use.

The extracted package supports flashing, USB updates, configuration and checks.
Building edited source requires the checkout. Documentation links are rewritten
for the packaged paths and checked when the ZIP is created. Required images must
match the manifest before packaging.

## Check the organization

```powershell
python Firmware/tools/check_layout.py
python Firmware/tools/check_layout.py --release Firmware/build/uf2/freeeeg128-uf2-release.zip
```

An extracted release can check its own contents with
`python tools/check_layout.py --release .`. These checks are read-only and do not
access the board. [Shared tooling](../Firmware/tools/README.md).

[Start here](../README.md) / [Validation](../Firmware/VALIDATION.md) /
[Assembly files](../AssemblyFiles/README.md).
