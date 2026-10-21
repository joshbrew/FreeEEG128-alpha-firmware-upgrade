# FreeEEG128 board design and production files

| Material | Location |
| --- | --- |
| KiCad project | [FreeEEG128-alpha.kicad_pro](KiCAD/FreeEEG128-alpha.kicad_pro) |
| Main schematic | [FreeEEG128-alpha.kicad_sch](KiCAD/FreeEEG128-alpha.kicad_sch) |
| PCB layout | [FreeEEG128-alpha.kicad_pcb](KiCAD/FreeEEG128-alpha.kicad_pcb) |
| Gerbers and drill files | [gerber.zip](KiCAD/out/gerber.zip) |
| Assembly package | [assembly.zip](KiCAD/out/assembly.zip) |
| Assembly drawing | [FreeEEG128-alpha.pdf](KiCAD/out/assembly/FreeEEG128-alpha.pdf) |
| Bill of materials | [FreeEEG128-alpha.csv](KiCAD/out/bom/FreeEEG128-alpha.csv) |
| Top / bottom placement | [top.pos](KiCAD/out/assembly/FreeEEG128-alpha-top.pos) / [bottom.pos](KiCAD/out/assembly/FreeEEG128-alpha-bottom.pos) |
| PCB preview | [PCB image](KiCAD/out/kicad_pcb/FreeEEG128-alpha_kicad_pcb.png) |
| 3D previews | [Top](KiCAD/out/3d/FreeEEG128-alpha_3d_top.png) / [bottom](KiCAD/out/3d/FreeEEG128-alpha_3d_bottom.png) |
| Converter datasheet | [ADS131M08](datasheets/ads131m08.pdf) |
| Reference datasheet | [REF2025](datasheets/ref2025.pdf) |

The complete original KiCAD tree is kept together, including hierarchical
schematics, footprint/symbol libraries, 3D models and fabrication outputs.
The main board is FreeEEG128-alpha; the FreeEEG32-named design files within the
historical KiCAD tree are retained as original supporting material. No fabrication
settings or board dimensions have been inferred from the photograph.

The firmware update does not alter the board design. Header J1/J2 pin numbers in
the [wiring guide](../SetupGuides/FreeEEG128_Programming_and_Updates.md) come from
the versioned PCB. All relocated original files retain their original Git content.

[Windows installation](../Flash/Windows/README.md) / [Linux installation](../Flash/Linux/README.md).
