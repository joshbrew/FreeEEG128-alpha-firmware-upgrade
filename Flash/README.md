# Flash and update FreeEEG128

Choose your operating system:

- **[Windows commands and launchers](Windows/README.md)** — `.cmd` files; Python,
  compiler and OpenOCD setup is automatic when needed.
- **[Linux commands and launchers](Linux/README.md)** — Bash scripts; automatic
  package setup on Debian/Ubuntu.

| What you want to do | Windows launcher | Linux launcher |
| --- | --- | --- |
| Choose an action interactively | `Windows/Menu.cmd` | `Linux/menu.sh` |
| Compile edited source into UF2 + factory HEX | `Windows/Build.cmd` | `Linux/build.sh` |
| Compile edited source and flash firmware + UF2 loader through ST-Link | `Windows/Build-and-Flash.cmd` | `Linux/build-and-flash.sh` |
| Flash an existing HEX/BIN/ELF without compiling | `Windows/Flash-Precompiled.cmd` | `Linux/flash-precompiled.sh` |
| Update through the installed USB loader | `Windows/USB-Update.cmd` | `Linux/usb-update.sh` |
| Configure sample rate, gains and saved settings | `Windows/Configure.cmd` | `Linux/configure.sh` |
| Check USB/firmware and ADC settings | `Windows/Check-Board.cmd` | `Linux/check-board.sh` |
| Measure 10 seconds of USB recording | `Windows/Test-Recording.cmd` | `Linux/test-recording.sh` |
| Save a support report | `Windows/Support-Report.cmd` | `Linux/support-report.sh` |
| Back up settings | `Windows/Backup-Settings.cmd` | `Linux/backup-settings.sh` |
| Restore a settings file | `Windows/Restore-Settings.cmd` | `Linux/restore-settings.sh` |
| Install/check all tools ahead of time | `Windows/Install-Tools.cmd` | `Linux/install-tools.sh` |

Run commands from the repository root, or use an absolute launcher path. Relative
image paths are interpreted from your current directory. The root `FreeEEG128.cmd`
and `FreeEEG128.sh` open the main menu.

**Menu 3** flashes an existing compiled HEX without building; source edits are
not included until you build again. **Menu 4** compiles the current edited source
and flashes the new result. The default `freeeeg128-factory.hex` contains this
fork's EEG application and UF2 bootloader together. "Factory" means the combined
first-install/recovery package.

**First install:** connect ST-Link over SWD, power the target normally, and flash
`freeeeg128-factory.hex`. **Later updates:** press RESET twice within **600 ms**,
then copy `freeeeg128-application.uf2` onto the **FREEEEG128** drive. After installation,
the board restarts into normal USB serial EEG streaming. The BOOT button selects
ST's factory loader, which cannot use this board's USB wiring.

Source builds write `Firmware/build/uf2/`, including `freeeeg128-uf2-release.zip`.
The ZIP includes this `Flash/` folder and the tools needed for precompiled flashing;
source builds require the repository. The default is **80 MHz**, with sample
rate controlled independently. Physical USB/flashing/temperature validation is pending.

[ST-Link pinout and photos](../README.md#swd-programming) ·
[Complete update/recovery guide](../Firmware/UF2/README.md) ·
[USB commands, saved settings and diagnostics](../Firmware/Alpha/README.md#usb-serial-controls)
