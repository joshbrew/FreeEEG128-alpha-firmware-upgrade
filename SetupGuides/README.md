# FreeEEG128 guides and reference images

| Task | Guide |
| --- | --- |
| Windows build, first installation and USB updates | [Windows commands](../Flash/Windows/README.md) |
| Linux build, first installation and USB updates | [Linux commands](../Flash/Linux/README.md) |
| Connect ST-Link and recover a board | [Programming and updates](FreeEEG128_Programming_and_Updates.md) |
| Identify the board headers | [Wiring diagram: PNG](images/freeeeg128-swd.png) / [SVG](images/freeeeg128-swd.svg) |
| Configure sampling/gain and saved settings | [Application controls](../Firmware/Alpha/README.md#usb-serial-controls) |
| Check recording, back up settings or save a report | [Host helpers](../Flash/HELPERS.md) |
| Understand the bootloader and flash layout | [UF2 guide](../Firmware/UF2/README.md) |
| Review test results and remaining bench checks | [Validation](../Firmware/VALIDATION.md) |
| Inspect hardware and manufacturing outputs | [Assembly files](../AssemblyFiles/README.md) |

[Repository organization and moved-file map](Repository_Layout.md).

## Original reference images

- [Component-side photo supplied for the wiring diagram](images/IMG20211127130429.jpg).
- [Opposite-side photo](images/IMG20211127130444.jpg).
- [Historical screenshot 06:07](images/Screenshot%20from%202021-10-05%2006-07-12.png).
- [Historical screenshot 06:12](images/Screenshot%20from%202021-10-05%2006-12-14.png).

All board/probe reference images are under `images/`. The wiring diagram identifies
J1 beside RESET and J2 beside BOOT using the supplied photo's orientation, with
electrical pin numbers checked against the PCB. Opposite-side views are mirrored.

The retained OpenVIBE integration has its own [compatibility note](../OpenVIBE/README.md).
