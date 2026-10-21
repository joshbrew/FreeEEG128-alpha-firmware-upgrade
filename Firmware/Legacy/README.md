# Historical firmware

Use the current [Windows](../../Flash/Windows/README.md) /
[Linux](../../Flash/Linux/README.md) launchers for builds, flashing and USB updates.
The maintained build uses the pinned [Base archive](../Base/README.md) and editable
[Alpha sources](../Alpha/README.md), independently of the IDE workspace here.

| Material | Historical use |
| --- | --- |
| `STM32CubeIDE/workspace_1.5.1/STM32H743ZI_ads131_80MHz/` | Original FreeEEG128 IDE application, vendor dependencies and peripheral project |
| Workspace `.metadata/` | Original IDE state and local history |
| Project `Debug/` and related outputs | Historical compiled objects and debug material; not current release images |

The complete workspace was relocated together and preserved as original Git
content. Open its existing project in STM32CubeIDE if inspecting the historical
workflow. Keep its relative paths intact. Do not use historical binaries as the
new factory or UF2 package; build current images through the root launcher.
