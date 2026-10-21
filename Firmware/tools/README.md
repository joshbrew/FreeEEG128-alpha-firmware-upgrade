# Shared host tooling

The root FreeEEG128 launchers and every action in `Flash/Windows` / `Flash/Linux`
use these helpers. The same folder is included as `tools/` in the release ZIP.

| File | Purpose |
| --- | --- |
| `launch.ps1` / `launch.sh` | Main menu and action routing in a checkout or extracted release |
| `setup-windows.ps1` / `setup-linux.sh` | Install/check only the dependencies needed for an action |
| `requirements-host.txt` | Pinned host serial dependency |
| `openocd-stlink.cfg` | SWD programming and recovery connection settings |
| `check_layout.py` | Check repository/release paths, documentation links, text encoding and required contents |

Run from the repository root:

```powershell
python Firmware/tools/check_layout.py
python Firmware/tools/check_layout.py --release Firmware/build/uf2/freeeeg128-uf2-release.zip
```

The checks are read-only and never open a board. An extracted release can check
itself with `python tools/check_layout.py --release .`. Source layout checks need
the checkout. Builds, tools and reports remain in their documented ignored folders.

[Windows commands](../../Flash/Windows/README.md) / [Linux commands](../../Flash/Linux/README.md).
