# Stack FreeEEG128 boards using one clock

Use **one master** FreeEEG128 and make the others **followers**. Flash the same
FreeEEG128 firmware onto every board. **No master/follower firmware switch or CPU
clock setting needs changing.** Start with the default 80 MHz build and 500 Hz
sampling. Two boards provide 256 channels, with a separate USB stream per board.

This follows the FreeEEG32 stacking guide, using **FreeEEG128-specific contacts**.
The wiring is checked against this board's PCB and firmware, but **has not been
tested on a physical stack**. Do not copy the 32's resistor/header numbers or
512 Hz setting.

## 1. Prepare the boards with all power disconnected

| Link | Master | Every follower |
| --- | --- | --- |
| **R112 / J302 pins 5–6 — oscillator** | Keep connected | Remove **R112** if fitted and any **J302 5–6** shunt |
| **R110 / J302 pins 3–4 — sync** | Keep connected | Remove **R110** if fitted and any **J302 3–4** shunt |

R112 and R110 are **0-ohm resistors**. The corresponding header shunt bridges the
same connection: **removing just the shunt may leave the connection closed**.
Check continuity with power off. On every follower, the local U7 oscillator output
must be disconnected from OSC_IN, and the local PD4 sync output must be
disconnected from the ADC SYNC/RESET line. Keep the master's two links connected
through their resistors or the listed shunts.

FreeEEG128 routes **U7's 8.192 MHz output into the MCU's HSE input**, then forwards
HSE unchanged through **PA8/MCO1 to all sixteen ADS131M08 CLKIN inputs**. Sharing
the HSE input below therefore supplies a common ADC clock without connecting the
boards' PA8 outputs together. This differs from the 32's separate ADC oscillator
and processor crystal. Leave oscillator power wiring unchanged.

## 2. Connect these three wires

Connect the master to **every follower** using only these contacts:

| Signal | Master | Each follower |
| --- | --- | --- |
| **8.192 MHz clock** | **J303 pin 1** | **J303 pin 1** |
| **ADC sync** | **J301 pin 6** | **J301 pin 6** |
| **GND** | **J303 pin 2** | **J303 pin 2** |

J303.1 is the **OSC_IN side** of R112, also available at J302.5. J301.6 is the
**ADC side** of R110, also available at J302.3. J303.2 is board ground. Do not
connect followers' **J302.6 oscillator outputs** together or reinstall their
5–6 / 3–4 shunts: that would reconnect the isolated drivers.

These are the central **2×4 headers**, not the outer electrode connectors. Pin 1
is the square PCB pad; identify the header and count pins against the board file.
**Do not connect all contacts straight through.** Other contacts carry supply
rails, analog references and MCU signals. Use insulating spacers for a physical
stack. Keep clock/sync wires short with a nearby ground return; additional boards
or longer cables may require a clock distribution buffer, verified on the bench.

## 3. Configure, check and start

1. Power the master first, then the followers, through their normal supplies.
   Connect **each board's USB data port** to the computer. Label boards and note
   their COM/serial ports. Each board must report **clock=HSE**; HSI fallback is a
   recovery path and does not provide the shared ADC clock.
2. Open **menu 6** for each board. Select the **same sample rate** on every board
   (start with **500 Hz**), set the gains, and save if wanted. Choose **0** to leave
   each board paused. Supported rates are **250/500/1000/2000 Hz**. Finish all
   settings changes before the shared sync. Close other EEG software while using
   the menus.
3. Send **RATE 500 to the master only**, even if it already uses 500 Hz. This
   applies the shared sync pulse. Substitute your chosen rate if different. The
   commands below also run CHECK, whose rate measurement takes about 500 ms.
4. Run **menu 7** on every follower. Check that all sixteen ADCs pass and the
   observed rate is correct. Leave the boards paused; fix any failure before
   continuing.
5. Use **menu 6 → 8 (Start streaming)** on the **followers first**, then on the
   **master last**. On FreeEEG128, the master's START sends another shared sync;
   followers' isolated PD4 outputs cannot resynchronize the shared line. Treat
   this final master START as the alignment event. Exclude at least the first
   **30 ms** afterward from every stream and verify alignment as described below.
   Do not change settings or restart the master afterward.

For step 3, run this from the repository folder. Replace the example port with the
**master's** port. Menu 6 installs the helper environment first.

**Windows PowerShell:**

```powershell
.\.venv\Scripts\python.exe Firmware\Alpha\control.py --port COM5 "RATE 500" CHECK
```

**Linux:**

```bash
.venv/bin/python Firmware/Alpha/control.py --port /dev/ttyACM0 "RATE 500" CHECK
```

In an extracted release ZIP, use `control.py` instead of `Firmware/Alpha/control.py`.
In a plain serial terminal, send `!RATE 500` followed by a newline, wait at least
**30 ms**, then send `!CHECK`. There is no separate SYNC command. RATE, START,
settings application and fault recovery can generate sync pulses; the master
controls the connected ADC sync line.

## Keep in mind

- **Each board still has its own 128-channel USB stream** with 393-byte frames.
  Recording software must open all ports and establish corresponding samples.
  Equal packet counters or first-arriving USB packets do not establish alignment.
  This guide does not add an automatic multi-board recorder or convert the legacy
  FreeEEG32 OpenVIBE driver into a 128-channel reader.
- Measure cross-board ADC clock and **DRDY timing**, then record a common test
  signal before relying on alignment. PA8 forwards HSE separately on each board;
  routing delay and clock/sync edge timing must be measured. Discarding 30 ms is
  a settling precaution, not proof of alignment. Menus 7/8 check individual boards,
  not cross-board alignment or clock signal quality.
- Pause all boards and repeat configuration/check/start after changing settings,
  rebooting a board, or master recovery. A master reboot sends a longer ADC reset
  that also resets connected followers' ADC registers; reapply every board's
  settings. A master ADC fault can also disturb the shared timing.
- Stop the stack before updating, rebooting or powering off the master. Its U7
  supplies the shared clock; there is no automatic replacement source. Avoid
  leaving clock/sync driven into an unpowered follower. Keep electrodes
  disconnected while changing wiring or programming.
- At 2000 Hz each board sends **786000 bytes/s** before USB overhead. Allow for the
  combined host bandwidth and monitor missing/DMA/USB counters. Run individual
  recording tests before the final alignment procedure: helpers that restart the
  master also send sync pulses.

## References

Board-specific pin and resistor checks:
[FreeEEG128 PCB](https://github.com/joshbrew/FreeEEG128-alpha-firmware-upgrade/blob/b06c92b0ad6b7d548679d21a3b323fc4afab78ef/AssemblyFiles/KiCAD/FreeEEG128-alpha.kicad_pcb)
and [schematic](https://github.com/joshbrew/FreeEEG128-alpha-firmware-upgrade/blob/b06c92b0ad6b7d548679d21a3b323fc4afab78ef/AssemblyFiles/KiCAD/FreeEEG128-alpha.kicad_sch).
The [TI ADS131M08 datasheet](https://www.ti.com/lit/ds/symlink/ads131m08.pdf),
sections 8.5.2 and 9.1.5 / Figure 9-4, describes common CLKIN and SYNC/RESET for
multiple converters. The firmware uses a short 2 µs sync pulse; a long low pulse
on that same input resets the converters.

[Application controls](../Firmware/Alpha/README.md#usb-serial-controls) /
[Programming and wiring](FreeEEG128_Programming_and_Updates.md) /
[Start here](../README.md).
