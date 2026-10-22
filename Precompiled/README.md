# Ready-to-flash firmware (80 MHz)

- **[Complete release ZIP](freeeeg128-uf2-release.zip)**: download and extract it, then run `FreeEEG128.cmd` on Windows or `bash FreeEEG128.sh` on Linux.
- **[Factory HEX](freeeeg128-factory.hex)**: EEG firmware + UF2 bootloader for first installation/recovery with ST-Link (menu **3**).
- **[Application UF2](freeeeg128-application.uf2)**: USB update for a board with the loader installed (menu **5**, or double-reset and copy to FREEEEG128).
- **[Build manifest](manifest.json)** and **[checksums](SHA256SUMS.txt)** identify the compiled firmware.

These files are tracked in Git. A local default 80 MHz build refreshes them, so
include the updated `Precompiled/` files when committing a firmware change.
Once these changes are pushed to `main`, GitHub Actions tests all seven clock profiles and the
Windows/Linux helpers, then commits the verified default 80 MHz downloads here
and updates the [rolling alpha release](https://github.com/joshbrew/FreeEEG128-alpha-firmware-upgrade/releases/tag/latest-alpha).
Run `git pull --ff-only` before your next push to pick up that generated commit.
On a fresh clone, menu **3** automatically uses this folder's factory HEX; menu
**5** uses its UF2. If a local build exists, those actions prefer that build.
Other clock profiles remain available in the workflow's build artifacts.

If checks fail, the previous automatic downloads remain available. Updating this
folder manually does not run the checks. Images remain alpha until physical-board
validation is complete. See the [main guide](../README.md) for wiring and setup.
