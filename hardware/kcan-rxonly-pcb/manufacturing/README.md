# Phase 3G manufacturing review package

This directory contains reproducible **review artifacts**, not an order release.
Generate it with:

```powershell
.\tools\generate_phase3g_manufacturing.ps1
```

The script first requires a zero-violation KiCad ERC and DRC, then exports the
Gerbers, Excellon files, filtered PCBA BOM/CPL, drawings, STEP model, rendered
board images, IPC-D-356 netlist, independent PyGerber layer renders, fabrication
ZIP, and SHA-256 manifest.

Phase 3G.1 uses no via-in-pad under U3. Its four peripheral GND thermal vias
are ordinary 0.60 mm pad / 0.30 mm drill vias, tented on both sides. No epoxy
fill, copper cap, special via plating, or sub-0.30 mm drill is requested by
this package. The resulting ZIP still requires a fresh manufacturer DFM and
price check; this statement is not a quotation or an order authorization.

No file in this directory authorizes manufacture, vehicle connection, CAN
transmission, or Phase 4. Human review and the complete bench bring-up procedure
remain mandatory.
