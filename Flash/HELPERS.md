# Board checks, settings files and support reports

Open `FreeEEG128.cmd` or run `bash FreeEEG128.sh` in the main folder. Close EEG
software using the board before running a helper. These actions need only Python
and the USB helper; missing dependencies are installed automatically.

| Menu | Use |
| --- | --- |
| 6 | Configure rate/gain, save/load settings, start/stop streaming |
| 7 | Check USB, firmware identity and each ADC's settings |
| 8 | Test 10 seconds of USB recording |
| 9 | Save a support report |
| 10 | Back up the current rate and all 128 gains |
| 11 | Preview and restore a settings file |

## What the checks measure

The board check reads back rate/gain registers on each ADC without changing
settings. It identifies failed ADCs and records the last failed register and
operation. An ADC configuration fault remains latched until reboot and prevents
acquisition from resuming.

The recording test checks the 393-byte, 128-channel frames, sequence numbers,
received rate and device counters. It allows 250 ms of startup settling. Passing
requires a rate within 2% of the request, at least two frames, and no observed
sequence loss, duplicates, malformed frames, USB drops, missed sample events,
DMA errors, RX drops or ADC faults. UART drops are not a USB-test failure.

The eight-bit sequence counter cannot identify losses of exactly 256 frames;
device counters and measured rate provide additional checks. This test measures
data delivery, not ADC clock accuracy, channel alignment, electrode contact or
EEG signal quality. Recording duration can be 2–60 seconds.

Checks, recording tests, reports and backups restore the previous streaming
state. A recording test briefly starts acquisition if it was paused. Firmware
without previous-state reporting stays paused afterward; use Configure board
to start it. A failed or partial settings restore also stays paused.

## Settings and reports

Backups include current settings, including unsaved changes. Restore validates
the board/format version, rate and 128 gains before opening the board. Its preview
offers **Apply for now**, **Apply and save for power-up**, or **Cancel**. Settings
are read back before saving. Configuration warns about unsaved changes before
leaving and lists presets by their rate and gain values.

Dated JSON files go in `reports/`. Support reports contain firmware/build ID,
clock/revision, USB identity, settings, ADC checks, reset flags, error counters,
tool versions and command replies. Error reports retain the underlying error and
available replies. Reports contain no EEG samples and are not sent automatically.
Existing files are not overwritten. Choose **A** in configuration, or pass
`-Advanced` / `--advanced`, to show raw diagnostic replies.

## Windows commands

Run from the main folder; add `-Port COM5` to select a board.

```powershell
.\Flash\Windows\Check-Board.cmd
.\Flash\Windows\Test-Recording.cmd -Seconds 10
.\Flash\Windows\Support-Report.cmd -Output "C:\Reports\board.json"
.\Flash\Windows\Backup-Settings.cmd -Output "C:\Reports\settings.json"
.\Flash\Windows\Restore-Settings.cmd -SettingsFile "C:\Reports\settings.json"
# Apply without an interactive prompt, and save for power-up:
.\Flash\Windows\Restore-Settings.cmd -SettingsFile "C:\Reports\settings.json" -Yes -Save
```

## Linux commands

Add `--port /dev/ttyACM0` to select a board. Helpers also work in an extracted
precompiled release bundle.

```bash
bash Flash/Linux/check-board.sh
bash Flash/Linux/test-recording.sh --seconds 10
bash Flash/Linux/support-report.sh --output "$HOME/board.json"
bash Flash/Linux/backup-settings.sh --output "$HOME/settings.json"
bash Flash/Linux/restore-settings.sh --file "$HOME/settings.json"
# Apply without an interactive prompt, and save for power-up:
bash Flash/Linux/restore-settings.sh --file "$HOME/settings.json" --yes --save
```

## Watchdog and build history

After application initialization, IWDG1 starts with a nominal 32.768-second
timeout, subject to LSI oscillator tolerance. The main loop refreshes it once
per second after work returns. Interrupts do not refresh it. A stuck main loop
or fault handler triggers a reset; paused acquisition and idle USB do not.
Missing samples are reported by checks/counters rather than deliberately
restarting an otherwise responsive board.

No watchdog option bytes are changed. Software activation ends on system reset;
the UF2 loader does not start the watchdog. Debug halt freezes it. Diagnostics
and reports include the reset reason. Physical recovery and update behavior
still require board testing.
[ST watchdog reference](https://github.com/STMicroelectronics/stm32h7xx-hal-driver/blob/master/Src/stm32h7xx_hal_iwdg.c)

Successful builds are saved under `Firmware/build/history/` with UTC time, clock
profile and build ID. Each UF2 archive contains the firmware ZIP and manifest.
To return to an earlier build, extract its ZIP and update with its application
UF2. The normal output folder contains the latest build.
