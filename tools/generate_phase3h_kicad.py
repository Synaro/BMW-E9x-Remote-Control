#!/usr/bin/env python3
"""Generate the Phase 3H full-bidirectional K-CAN KiCad design.

Run the schematic half with the repository Python after installing
``kicad-sch-api==0.5.6``. Run the PCB half with KiCad's bundled Python so that
``pcbnew`` is available. Generated files are committed; CI validates them and
does not need either generator dependency.
"""

from __future__ import annotations

import argparse
import os
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "hardware" / "kcan-bidirectional-pcb" / "kicad"
SOURCE_BOARD = (
    ROOT / "hardware" / "kcan-rxonly-pcb" / "kicad"
    / "BMW-E9x-KCAN-RXOnly.kicad_pcb"
)
NAME = "BMW-E9x-KCAN-Bidirectional"

SYMBOLS = Path(os.environ.get(
    "KICAD_SYMBOL_DIR",
    r"C:\Users\synar\AppData\Local\Programs\KiCad\10.0\share\kicad\symbols",
))
FOOTPRINTS = Path(os.environ.get(
    "KICAD_FOOTPRINT_DIR",
    r"C:\Users\synar\AppData\Local\Programs\KiCad\10.0\share\kicad\footprints",
))


FOOTPRINT = {
    "U1": "Package_SO:SO-14_3.9x8.65mm_P1.27mm",
    "U2": "Package_TO_SOT_SMD:SOT-363_SC-70-6",
    "U3": "Phase3H:Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_NoViaInPad",
    "DEV": "Connector_PinSocket_2.54mm:PinSocket_1x22_P2.54mm_Vertical",
    "R0805": "Resistor_SMD:R_0805_2012Metric",
    "R1206": "Resistor_SMD:R_1206_3216Metric",
    "C0805": "Capacitor_SMD:C_0805_2012Metric",
    "C1206": "Capacitor_SMD:C_1206_3216Metric",
    "F1812": "Phase3H:Fuse_1812_4532Metric_Pad1.30x3.40mm_Reviewed",
    "D_SMA": "Diode_SMD:D_SMA",
    "J2": "Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical",
    "J3": "Connector_JST:JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical",
    "TP": "TestPoint:TestPoint_Pad_D1.5mm",
    "MH": "MountingHole:MountingHole_3.2mm_M3",
}


TESTPOINTS = [
    ("TP1", "TP_BAT", "BAT_LOCAL"),
    ("TP2", "TP_5V", "5V_TJA"),
    ("TP3", "TP_3V3", "3V3"),
    ("TP5", "TP_TXD", "KCAN_TXD"),
    ("TP7", "TP_TX_MCU", "KCAN_TX_MCU"),
    ("TP8", "TP_RESET", "RESET"),
    ("TP9", "TP_STB", "KCAN_STB"),
    ("TP10", "TP_EN", "KCAN_EN"),
    ("TP11", "TP_RXD", "KCAN_RXD_OD"),
    ("TP12", "TP_ERR", "KCAN_ERR_OD"),
    ("TP13", "TP_CANH", "KCAN_H"),
    ("TP14", "TP_CANL", "KCAN_L"),
    ("TP15", "TP_GND", "GND"),
]


# Phase 3G.1 keeps the regulator exposed pad free of drilled holes.  Four
# ordinary tented vias outside the exposed-pad copper connect the large F.Cu
# heat spreader to the B.Cu GND plane without invoking a via-in-pad process.
U3_PERIPHERAL_THERMAL_VIA_OFFSETS = (
    (-0.8, -2.0),
    (0.8, -2.0),
    (-0.8, 2.0),
    (0.8, 2.0),
)


def _symbol_table() -> str:
    libs = [
        "Connector", "Connector_Generic", "Device", "74xGxx",
    ]
    entries = "\n".join(
        f'  (lib (name "{lib}")(type "KiCad")(uri "${{KICAD10_SYMBOL_DIR}}/{lib}.kicad_sym")(options "")(descr ""))'
        for lib in libs
    )
    return f"(sym_lib_table\n  (version 7)\n{entries}\n)\n"


def _footprint_table() -> str:
    libs = sorted({value.split(":", 1)[0] for value in FOOTPRINT.values()})
    entries = "\n".join(
        (
            '  (lib (name "Phase3H")(type "KiCad")'
            '(uri "${KIPRJMOD}/footprints/Phase3H.pretty")'
            '(options "")(descr "Reviewed local Phase 3H footprints"))'
            if lib == "Phase3H" else
            f'  (lib (name "{lib}")(type "KiCad")(uri "${{KICAD10_FOOTPRINT_DIR}}/{lib}.pretty")(options "")(descr ""))'
        )
        for lib in libs
    )
    return f"(fp_lib_table\n  (version 7)\n{entries}\n)\n"


def write_local_footprints() -> None:
    """Create the reviewed PG-DSO-8-52 footprint under a truthful name.

    KiCad 10 ships the verified 3.9 x 4.9 mm / EP 2.65 x 3.00 mm geometry
    under an older PG-DSO-8-27 library name.  Infineon's current product page
    identifies TLS715B0EJV50XUMA1 as PG-DSO-8-52.  The package drawing in the
    2024-12-12 datasheet carries the same dimensions.  Keeping a local copy
    avoids silently attaching the wrong package identifier to the PCBA BOM.
    """
    source_name = "Infineon_PG-DSO-8-27_3.9x4.9mm_EP2.65x3mm_ThermalVias"
    old_target_name = "Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_ThermalVias"
    target_name = "Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_NoViaInPad"
    source = FOOTPRINTS / "Package_SO.pretty" / f"{source_name}.kicad_mod"
    target_dir = OUT / "footprints" / "Phase3H.pretty"
    target_dir.mkdir(parents=True, exist_ok=True)
    contents = source.read_text(encoding="utf-8")
    contents = contents.replace(source_name, target_name)
    contents = contents.replace(
        "https://www.infineon.com/cms/en/product/packages/PG-DSO/PG-DSO-8-27",
        "https://www.infineon.com/package/PG-DSO-8-52",
    )
    contents = contents.replace(
        "Infineon  PG-DSO, 8 Pin",
        "Infineon PG-DSO-8-52, TLS715B0EJV50, 8 Pin plus exposed pad",
    )
    # The Infineon PG-DSO-8-52 package drawing specifies the 2.65 x 3.00 mm
    # NSMD exposed pad and four 1.125 x 1.30 mm stencil apertures.  Remove the
    # nine 0.20 mm via-in-pad holes from KiCad's older -27 source footprint;
    # Phase 3G.1 adds standard 0.60/0.30 mm tented vias beside the pad on the
    # PCB instead.  Keeping paste off drilled holes avoids solder wicking and
    # the filled/capped-via process detected during the first JLCPCB DFM pass.
    contents = contents.replace('(pad "" smd roundrect', '(pad "" smd rect')
    contents = contents.replace("(at -0.66 -0.75)", "(at -0.665 -0.75)")
    contents = contents.replace("(at -0.66 0.75)", "(at -0.665 0.75)")
    contents = contents.replace("(at 0.66 -0.75)", "(at 0.665 -0.75)")
    contents = contents.replace("(at 0.66 0.75)", "(at 0.665 0.75)")
    contents = contents.replace("(size 1.11 1.25)", "(size 1.125 1.3)")
    contents = contents.replace("\n\t\t(roundrect_rratio 0.225225)", "")

    contents, removed_vias = re.subn(
        r'\n\t\(pad "9" thru_hole circle.*?\n\t\)',
        "",
        contents,
        flags=re.DOTALL,
    )
    if removed_vias != 9:
        raise RuntimeError(f"Expected nine source thermal vias, found {removed_vias}")

    pad9_pattern = re.compile(
        r'\n\t\(pad "9" smd rect\n(?:(?!\n\t\)).)*?\n\t\)',
        flags=re.DOTALL,
    )
    removed_bottom_pad = 0

    def keep_front_exposed_pad(match: re.Match[str]) -> str:
        nonlocal removed_bottom_pad
        block = match.group(0)
        if '(layers "B.Cu")' in block:
            removed_bottom_pad += 1
            return ""
        return block

    contents = pad9_pattern.sub(keep_front_exposed_pad, contents)
    if removed_bottom_pad != 1:
        raise RuntimeError(
            f"Expected one source B.Cu exposed-pad duplicate, found {removed_bottom_pad}"
        )
    # KiCad does not currently ship a model carrying the -52 package name.
    # Use the exact 3.9 x 4.9 mm / 1.27 mm mechanical envelope model; the
    # exposed-pad geometry remains authoritative in the footprint copper.
    contents = contents.replace(
        "Package_SO.3dshapes/Infineon_PG-DSO-8-27_3.9x4.9mm_EP2.65x3mm.step",
        "Package_SO.3dshapes/SO-8_3.9x4.9mm_P1.27mm.step",
    )
    (target_dir / f"{old_target_name}.kicad_mod").unlink(missing_ok=True)
    (target_dir / f"{target_name}.kicad_mod").write_text(
        contents, encoding="utf-8"
    )

    # KiCad 10's fuse footprint references a 3D model that is not shipped in
    # the Windows model bundle.  Preserve the exact land pattern in a reviewed
    # local copy and use the available 1812 body envelope only for visual STEP
    # review.  The copper, paste, mask, courtyard, and BOM remain authoritative.
    fuse_source_name = "Fuse_1812_4532Metric_Pad1.30x3.40mm_HandSolder"
    fuse_target_name = "Fuse_1812_4532Metric_Pad1.30x3.40mm_Reviewed"
    fuse_source = FOOTPRINTS / "Fuse.pretty" / f"{fuse_source_name}.kicad_mod"
    fuse_contents = fuse_source.read_text(encoding="utf-8")
    fuse_contents = fuse_contents.replace(fuse_source_name, fuse_target_name)
    fuse_contents = fuse_contents.replace(
        "${KICAD10_3DMODEL_DIR}/Fuse.3dshapes/Fuse_1812_4532Metric.step",
        "${KICAD10_3DMODEL_DIR}/Capacitor_SMD.3dshapes/C_1812_4532Metric.step",
    )
    (target_dir / f"{fuse_target_name}.kicad_mod").write_text(
        fuse_contents, encoding="utf-8"
    )


def write_project_tables() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    write_local_footprints()
    (OUT / "sym-lib-table").write_text(_symbol_table(), encoding="utf-8")
    (OUT / "fp-lib-table").write_text(_footprint_table(), encoding="utf-8")
    project = OUT / f"{NAME}.kicad_pro"
    if not project.exists():
        project.write_text(
            '{\n  "board": {},\n  "boards": [],\n  "cvpcb": {},\n'
            '  "erc": {},\n  "libraries": {},\n  "meta": {"filename": "'
            + NAME + '.kicad_pro", "version": 1},\n  "net_settings": {},\n'
            '  "pcbnew": {},\n  "schematic": {},\n  "text_variables": {}\n}\n',
            encoding="utf-8",
        )


def generate_schematic() -> None:
    os.environ["KICAD_SYMBOL_DIR"] = str(SYMBOLS)
    import kicad_sch_api as ksa

    write_project_tables()
    sch = ksa.create_schematic(NAME)
    sch.set_paper_size("A3")
    sch.set_title_block(
        title="BMW E9x K-CAN FULL BIDIRECTIONAL — PHASE 3H",
        date="2026-10-05",
        rev="3H-A",
        company="Synaro / open hardware prototype",
        comments={
            1: "FULL RX + TX HARDWARE — NO BMW COMMANDS IN THIS PHASE",
            2: "ESP32 USB ONLY; 5V_TJA FROM TLS715B0EJV50",
            3: "CONTINUOUS TX: GPIO5 -> U2 -> TJA1055 TXD",
        },
    )

    def add(
        lib: str,
        ref: str,
        value: str,
        pos: tuple[float, float],
        fp: str,
        rotation: float = 0.0,
    ) -> None:
        sch.components.add(lib, ref, value, pos, footprint=fp, rotation=rotation)

    add("Connector_Generic:Conn_01x14", "U1", "TJA1055T/3/2Z", (95, 85), FOOTPRINT["U1"])
    add("Connector_Generic:Conn_01x06", "U2", "SN74LXC1T45QDCKRQ1", (170, 85), FOOTPRINT["U2"])
    add("Connector_Generic:Conn_01x08", "U3", "TLS715B0EJV50XUMA1", (45, 85), FOOTPRINT["U3"])
    add("Connector_Generic:Conn_01x22", "H1", "ESP32-S3-DevKitC-1 J1", (225, 76), FOOTPRINT["DEV"])
    add("Connector_Generic:Conn_01x22", "H2", "ESP32-S3-DevKitC-1 J3", (255, 76), FOOTPRINT["DEV"])

    parts = [
        ("Device:Polyfuse", "F1", "MINISMDC010F-2 / F_BAT", (25, 135), FOOTPRINT["F1812"]),
        ("Device:D_Schottky", "D1", "SS16 / D_BAT", (45, 135), FOOTPRINT["D_SMA"]),
        ("Device:R", "R1", "1k / R_BAT", (30, 135), FOOTPRINT["R1206"], 90),
        ("Device:R", "R2", "10k / R_TXD_BIAS", (60, 135), FOOTPRINT["R0805"], 90),
        ("Device:R", "R4", "100k / R_A", (120, 135), FOOTPRINT["R0805"], 90),
        ("Device:R", "R5", "1k / R_TX_SER", (150, 135), FOOTPRINT["R0805"], 90),
        ("Device:R", "R6", "3.3k / R_RX_PULL", (180, 135), FOOTPRINT["R0805"], 90),
        ("Device:R", "R7", "1k / R_RX_SER", (210, 135), FOOTPRINT["R0805"], 90),
        ("Device:R", "R8", "3.3k / R_ERR_PULL", (240, 135), FOOTPRINT["R0805"], 90),
        ("Device:R", "R9", "1k / R_ERR_SER", (270, 135), FOOTPRINT["R0805"], 90),
        ("Device:R", "R10", "1k / R_STB_SER", (45, 160), FOOTPRINT["R0805"], 90),
        ("Device:R", "R11", "10k / R_STB_PD", (80, 160), FOOTPRINT["R0805"], 90),
        ("Device:R", "R12", "1k / R_EN_SER", (115, 160), FOOTPRINT["R0805"], 90),
        ("Device:R", "R13", "10k / R_EN_PD", (150, 160), FOOTPRINT["R0805"], 90),
        ("Device:R", "R14", "5.62k / R_RTH", (185, 160), FOOTPRINT["R0805"], 90),
        ("Device:R", "R15", "5.62k / R_RTL", (220, 160), FOOTPRINT["R0805"], 90),
        ("Device:C", "C1", "100n 50V / C_REG_IN", (35, 190), FOOTPRINT["C0805"], 90),
        ("Device:C", "C2", "2.2u 25V / C_REG_OUT", (80, 190), FOOTPRINT["C0805"], 90),
        ("Device:C", "C3", "100n / C_VCC", (125, 190), FOOTPRINT["C0805"], 90),
        ("Device:C", "C4", "22u 10V / C_VCC", (165, 190), FOOTPRINT["C1206"], 90),
        ("Device:C", "C5", "10n 50V / C_BAT", (205, 190), FOOTPRINT["C0805"], 90),
        ("Device:C", "C6", "100n / C_U2", (245, 190), FOOTPRINT["C0805"], 90),
        ("Device:C", "C7", "100n / C_U2_5V", (270, 190), FOOTPRINT["C0805"], 90),
        ("Device:C", "C8", "100n / C_U2_3V3_LOCAL", (295, 190), FOOTPRINT["C0805"], 90),
        ("Connector_Generic:Conn_01x02", "J1", "BENCH 10..18V / J_BAT", (25, 55), FOOTPRINT["J2"]),
        ("Connector_Generic:Conn_01x03", "J2", "LSFT BENCH BUS / J_KCAN", (65, 55), FOOTPRINT["J3"]),
    ]
    for item in parts:
        add(*item)

    for index, (ref, semantic_ref, _) in enumerate(TESTPOINTS):
        x = 30 + (index % 8) * 28
        y = 220 + (index // 8) * 20
        add("Connector:TestPoint", ref, semantic_ref.removeprefix("TP_"), (x, y), FOOTPRINT["TP"])

    pin_nets: dict[str, dict[str, str]] = {
        "U1": {"2":"KCAN_TXD","3":"KCAN_RXD_OD","4":"KCAN_ERR_OD","5":"KCAN_STB","6":"KCAN_EN","7":"BAT_LOCAL","8":"RTH","9":"RTL","10":"5V_TJA","11":"KCAN_H","12":"KCAN_L","13":"GND","14":"BAT_LOCAL"},
        "U2": {"1":"3V3","2":"GND","3":"KCAN_TX_A","4":"KCAN_TXD","5":"3V3","6":"5V_TJA"},
        "U3": {"1":"BAT_PROTECTED","2":"BAT_PROTECTED","3":"GND","8":"5V_TJA"},
        "H1": {"1":"3V3","3":"RESET","4":"KCAN_RX_MCU","5":"KCAN_TX_MCU","6":"KCAN_STB_MCU","7":"KCAN_ERR_MCU","8":"KCAN_EN_MCU"},
        "H2": {"1":"GND"},
        "F1": {"1":"BAT_INPUT","2":"BAT_FUSED"},
        "D1": {"1":"BAT_PROTECTED","2":"BAT_FUSED"},
        "R1": {"1":"BAT_PROTECTED","2":"BAT_LOCAL"},
        "R2": {"1":"5V_TJA","2":"KCAN_TXD"},
        "R4": {"1":"3V3","2":"KCAN_TX_A"},
        "R5": {"1":"KCAN_TX_MCU","2":"KCAN_TX_A"},
        "R6": {"1":"3V3","2":"KCAN_RXD_OD"},
        "R7": {"1":"KCAN_RXD_OD","2":"KCAN_RX_MCU"},
        "R8": {"1":"3V3","2":"KCAN_ERR_OD"},
        "R9": {"1":"KCAN_ERR_OD","2":"KCAN_ERR_MCU"},
        "R10": {"1":"KCAN_STB_MCU","2":"KCAN_STB"},
        "R11": {"1":"KCAN_STB","2":"GND"},
        "R12": {"1":"KCAN_EN_MCU","2":"KCAN_EN"},
        "R13": {"1":"KCAN_EN","2":"GND"},
        "R14": {"1":"KCAN_H","2":"RTH"},
        "R15": {"1":"KCAN_L","2":"RTL"},
        "C1": {"1":"BAT_PROTECTED","2":"GND"},
        "C2": {"1":"5V_TJA","2":"GND"},
        "C3": {"1":"5V_TJA","2":"GND"},
        "C4": {"1":"5V_TJA","2":"GND"},
        "C5": {"1":"BAT_LOCAL","2":"GND"},
        "C6": {"1":"3V3","2":"GND"},
        "C7": {"1":"5V_TJA","2":"GND"},
        "C8": {"1":"3V3","2":"GND"},
        "J1": {"1":"BAT_INPUT","2":"GND"},
        "J2": {"1":"KCAN_H","2":"KCAN_L","3":"GND"},
    }
    for ref, _, net in TESTPOINTS:
        pin_nets[ref] = {"1": net}

    # Direct pin labels keep the netlist unambiguous; the larger A3 spacing and
    # horizontal passives keep the exported drawing readable.
    for ref, pins in pin_nets.items():
        for pin, net in pins.items():
            sch.add_label(net, pin=(ref, pin), size=0.65)

    unused = {
        "U1": ["1"], "U3": ["4", "5", "6", "7"],
        "H1": [str(x) for x in range(2, 23) if x not in {3,4,5,6,7,8}],
        "H2": [str(x) for x in range(2, 23)],
    }
    for ref, pins in unused.items():
        for pin in pins:
            sch.no_connects.add(sch.get_component_pin_position(ref, pin))

    sch.add_text("POWER: 10..18V -> F_BAT -> D_BAT -> TLS715B0EJV50 -> 5V_TJA", (80, 25), size=1.5)
    sch.add_text("ESP32 USB ONLY — DEVKIT 5V PIN NC — COMMON GND ONLY", (70, 32), size=1.5)
    sch.add_text("FULL TX: GPIO5 -> R5 -> U2 A/B -> KCAN_TXD -> U1.TXD", (205, 32), size=1.5)
    sch.add_text("U2 VCCA=3V3, VCCB=5V_TJA, DIR=3V3; NO TX JUMPER OR GAP", (205, 39), size=1.3)
    sch.save_as(OUT / f"{NAME}.kicad_sch")


def generate_pcb() -> None:
    import pcbnew

    write_project_tables()
    board = pcbnew.BOARD()
    board.SetFileName(str(OUT / f"{NAME}.kicad_pcb"))
    settings = board.GetDesignSettings()
    settings.SetCopperLayerCount(2)
    settings.SetBoardThickness(pcbnew.FromMM(1.6))
    # 0.20 mm (7.9 mil) remains comfortably above the public prototype-house
    # minima while matching the manufacturer's PG-DSO-8 EP land-pattern
    # clearances.  The routed board is still checked against this value by DRC.
    settings.m_MinClearance = pcbnew.FromMM(0.20)
    # Every routed segment is at least 0.20 mm; the rule intentionally stays
    # above the retained prototype-house minima instead of allowing a future
    # 0.15 mm neck-down merely because it is manufacturable.
    settings.m_TrackMinWidth = pcbnew.FromMM(0.20)
    settings.m_ViasMinSize = pcbnew.FromMM(0.6)
    # Phase 3G.1 uses only ordinary 0.30 mm mechanically drilled vias.  U3's
    # exposed pad itself has no drilled hole and requires no filled/capped via.
    settings.m_MinThroughDrill = pcbnew.FromMM(0.3)

    nets: dict[str, object] = {}
    for name in [
        "GND","3V3","BAT_INPUT","BAT_FUSED","BAT_PROTECTED","BAT_LOCAL","5V_TJA",
        "TXD_SAFE","TX_GPIO","TX_GATE_IN","TX_GATE_OUT","OE_SAFE","RXD_OD","RXD_GPIO",
        "ERR_OD","ERR_GPIO","RX_MODE_FEED","STB_SAFE","EN_SAFE","RTH","RTL","KCAN_H",
        "KCAN_L","RESET",
    ]:
        net = pcbnew.NETINFO_ITEM(board, name)
        board.Add(net)
        nets[name] = net

    def load_fp(lib_id: str):
        lib, name = lib_id.split(":", 1)
        library = (
            OUT / "footprints" / "Phase3G.pretty"
            if lib == "Phase3G" else FOOTPRINTS / f"{lib}.pretty"
        )
        fp = pcbnew.FootprintLoad(str(library), name)
        if fp is None:
            raise RuntimeError(f"Footprint not found: {lib_id}")
        return fp

    footprints: dict[str, object] = {}

    def add_fp(ref: str, value: str, lib_id: str, x: float, y: float, rot: float = 0.0):
        fp = load_fp(lib_id)
        fp.SetReference(ref)
        fp.SetValue(value)
        fp.SetPosition(pcbnew.VECTOR2I_MM(x, y))
        fp.SetOrientationDegrees(rot)
        board.Add(fp)
        footprints[ref] = fp
        return fp

    add_fp("J1", "J_BAT 10..18V BENCH", FOOTPRINT["J2"], 25, 73, 90)
    add_fp("F1", "F_BAT MINISMDC010F-2", FOOTPRINT["F1812"], 36, 73)
    add_fp("D1", "D_BAT SS16", FOOTPRINT["D_SMA"], 46, 73)
    add_fp("U3", "TLS715B0EJV50XUMA1", FOOTPRINT["U3"], 58, 72)
    add_fp("C1", "C_REG_IN 100n 50V", FOOTPRINT["C0805"], 51, 65)
    add_fp("C2", "C_REG_OUT 2.2u 25V", FOOTPRINT["C0805"], 65, 65)
    add_fp("R1", "R_BAT 1k", FOOTPRINT["R1206"], 45, 57)
    add_fp("C5", "C_BAT 10n 50V", FOOTPRINT["C0805"], 51, 54)
    add_fp("C3", "C_VCC 100n", FOOTPRINT["C0805"], 65, 57)
    add_fp("C4", "C_VCC 22u 10V", FOOTPRINT["C1206"], 70, 61)
    add_fp("U1", "TJA1055T/3/2Z", FOOTPRINT["U1"], 56, 41, 90)
    add_fp("J2", "J_KCAN H L GND", FOOTPRINT["J3"], 25, 29, 90)
    add_fp("R14", "R_RTH 5.62k", FOOTPRINT["R0805"], 39, 28)
    add_fp("R15", "R_RTL 5.62k", FOOTPRINT["R0805"], 39, 34)
    add_fp("R2", "R_TXD 10k", FOOTPRINT["R0805"], 69, 38)
    add_fp("JP1", "RX MODE REMOVE AT BOOT", FOOTPRINT["JP2"], 73, 48, 90)
    add_fp("R10", "R_STB_SER 1k", FOOTPRINT["R0805"], 67, 46)
    add_fp("R11", "R_STB_PD 10k", FOOTPRINT["R0805"], 67, 49)
    add_fp("R12", "R_EN_SER 1k", FOOTPRINT["R0805"], 67, 52)
    add_fp("R13", "R_EN_PD 10k", FOOTPRINT["R0805"], 67, 55)
    add_fp("R6", "R_RX_PULL 3.3k", FOOTPRINT["R0805"], 74, 31)
    add_fp("R7", "R_RX_SER 1k", FOOTPRINT["R0805"], 81, 31)
    add_fp("R8", "R_ERR_PULL 3.3k", FOOTPRINT["R0805"], 74, 35)
    add_fp("R9", "R_ERR_SER 1k", FOOTPRINT["R0805"], 81, 35)
    add_fp("U2", "CLVC1G125QDBVRQ1", FOOTPRINT["U2"], 84, 48)
    add_fp("R3", "R_OE 10k", FOOTPRINT["R0805"], 82, 55)
    add_fp("R4", "R_A 100k", FOOTPRINT["R0805"], 89, 55)
    add_fp("R5", "R_TX_SER 1k", FOOTPRINT["R0805"], 92, 48)
    add_fp("C6", "C_U2 100n", FOOTPRINT["C0805"], 84, 41)
    add_fp("H1", "ESP32-S3 DEVKIT J1", FOOTPRINT["DEV"], 98, 22)
    add_fp("H2", "ESP32-S3 DEVKIT J3", FOOTPRINT["DEV"], 123.4, 22)

    tp_positions = {
        "TP_CANH":(24,19), "TP_CANL":(31,19), "TP_GND":(38,19),
        "TP_TXD":(64,19), "TP_GATE_Y":(84,19), "TP_TWAI_TX":(91,19),
        "TP_RXD":(70,25), "TP_ERR":(78,25), "TP_OE":(86,25),
        "TP_STB":(65,84), "TP_EN":(73,84), "TP_BAT":(43,84),
        "TP_5V":(65,74), "TP_3V3":(91,74), "TP_RESET":(98,84),
    }
    tp_aliases = {semantic_ref: ref for ref, semantic_ref, _ in TESTPOINTS}
    for semantic_ref, (x, y) in tp_positions.items():
        add_fp(tp_aliases[semantic_ref], semantic_ref.removeprefix("TP_"), FOOTPRINT["TP"], x, y)
    for ref, x, y in [("MH1",24,14),("MH2",136,14),("MH3",24,86),("MH4",136,86)]:
        add_fp(ref, "M3", FOOTPRINT["MH"], x, y)

    pad_nets = {
        "J1":{"1":"BAT_INPUT","2":"GND"}, "F1":{"1":"BAT_INPUT","2":"BAT_FUSED"},
        "D1":{"1":"BAT_PROTECTED","2":"BAT_FUSED"},
        "U3":{"1":"BAT_PROTECTED","2":"BAT_PROTECTED","3":"GND","8":"5V_TJA","9":"GND"},
        "C1":{"1":"BAT_PROTECTED","2":"GND"}, "C2":{"1":"5V_TJA","2":"GND"},
        "R1":{"1":"BAT_PROTECTED","2":"BAT_LOCAL"}, "C5":{"1":"BAT_LOCAL","2":"GND"},
        "C3":{"1":"5V_TJA","2":"GND"}, "C4":{"1":"5V_TJA","2":"GND"},
        "U1":{"2":"TXD_SAFE","3":"RXD_OD","4":"ERR_OD","5":"STB_SAFE","6":"EN_SAFE","7":"BAT_LOCAL","8":"RTH","9":"RTL","10":"5V_TJA","11":"KCAN_H","12":"KCAN_L","13":"GND","14":"BAT_LOCAL"},
        "J2":{"1":"KCAN_H","2":"KCAN_L","3":"GND"},
        "R14":{"1":"KCAN_H","2":"RTH"}, "R15":{"1":"KCAN_L","2":"RTL"},
        "R2":{"1":"5V_TJA","2":"TXD_SAFE"}, "JP1":{"1":"5V_TJA","2":"RX_MODE_FEED"},
        "R10":{"1":"RX_MODE_FEED","2":"STB_SAFE"}, "R11":{"1":"STB_SAFE","2":"GND"},
        "R12":{"1":"RX_MODE_FEED","2":"EN_SAFE"}, "R13":{"1":"EN_SAFE","2":"GND"},
        "R6":{"1":"3V3","2":"RXD_OD"}, "R7":{"1":"RXD_OD","2":"RXD_GPIO"},
        "R8":{"1":"3V3","2":"ERR_OD"}, "R9":{"1":"ERR_OD","2":"ERR_GPIO"},
        "U2":{"1":"OE_SAFE","2":"TX_GATE_IN","3":"GND","4":"TX_GATE_OUT","5":"3V3"},
        "R3":{"1":"3V3","2":"OE_SAFE"}, "R4":{"1":"3V3","2":"TX_GATE_IN"},
        "R5":{"1":"TX_GPIO","2":"TX_GATE_IN"}, "C6":{"1":"3V3","2":"GND"},
        "H1":{"1":"3V3","3":"RESET","4":"RXD_GPIO","5":"TX_GPIO","7":"ERR_GPIO"},
        "H2":{"1":"GND"},
    }
    for ref, _, net_name in TESTPOINTS:
        pad_nets[ref] = {"1": net_name}

    for ref, mapping in pad_nets.items():
        fp = footprints[ref]
        for pad_number, net_name in mapping.items():
            matching_pads = [
                pad for pad in fp.Pads() if pad.GetNumber() == str(pad_number)
            ]
            if not matching_pads:
                raise RuntimeError(f"Missing pad {ref}.{pad_number}")
            for pad in matching_pads:
                pad.SetNet(nets[net_name])

    u3_position = footprints["U3"].GetPosition()
    for dx, dy in U3_PERIPHERAL_THERMAL_VIA_OFFSETS:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(u3_position + pcbnew.VECTOR2I_MM(dx, dy))
        via.SetWidth(pcbnew.FromMM(0.60))
        via.SetDrill(pcbnew.FromMM(0.30))
        via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        via.SetNet(nets["GND"])
        via.SetFrontTentingMode(pcbnew.TENTING_MODE_TENTED)
        via.SetBackTentingMode(pcbnew.TENTING_MODE_TENTED)
        board.Add(via)

    def edge(x1: float, y1: float, x2: float, y2: float) -> None:
        shape = pcbnew.PCB_SHAPE(board)
        shape.SetShape(pcbnew.SHAPE_T_SEGMENT)
        shape.SetStart(pcbnew.VECTOR2I_MM(x1, y1))
        shape.SetEnd(pcbnew.VECTOR2I_MM(x2, y2))
        shape.SetLayer(pcbnew.Edge_Cuts)
        shape.SetWidth(pcbnew.FromMM(0.1))
        board.Add(shape)

    edge(20,10,140,10); edge(140,10,140,90); edge(140,90,20,90); edge(20,90,20,10)

    # A two-layer rule area makes the intentional TX discontinuity visually
    # and electrically unmistakable.  It is exported to the router as part of
    # the board and prevents tracks, vias, and copper pours from crossing the
    # physical gap between TP_TXD and TP_GATE_Y.
    def keepout(layer: int) -> None:
        area = pcbnew.ZONE(board)
        area.SetZoneName("TX_PHYSICAL_GAP_KEEP_OUT")
        area.SetLayer(layer)
        area.SetIsRuleArea(True)
        area.SetDoNotAllowTracks(True)
        area.SetDoNotAllowVias(True)
        area.SetDoNotAllowZoneFills(True)
        polygon = area.Outline()
        index = polygon.NewOutline()
        outline = polygon.Outline(index)
        # Keep 0.25 mm minimum clearance to the neighbouring RXD/ERR test-point
        # pads while retaining a 6 mm copper-free barrier centred between the
        # physically independent TX endpoints.
        for x, y in [(71, 16), (77, 16), (77, 24), (71, 24)]:
            outline.Append(pcbnew.VECTOR2I_MM(x, y))
        board.Add(area)

    keepout(pcbnew.F_Cu)
    keepout(pcbnew.B_Cu)

    def text(value: str, x: float, y: float, size: float = 1.2, layer=None) -> None:
        item = pcbnew.PCB_TEXT(board)
        item.SetText(value)
        item.SetPosition(pcbnew.VECTOR2I_MM(x, y))
        item.SetLayer(pcbnew.F_SilkS if layer is None else layer)
        item.SetTextSize(pcbnew.VECTOR2I_MM(size, size))
        item.SetTextThickness(pcbnew.FromMM(max(0.18, size * 0.15)))
        board.Add(item)

    text("BMW E9x K-CAN RX-ONLY", 76, 12, 1.6)
    text("PHASE 3G PROTOTYPE / BENCH ONLY", 76, 15, 1.2)
    text("NO VEHICLE CONNECTION BEFORE QUALIFICATION", 74, 88, 1.0)
    text("TX LINK DNP", 75, 21, 1.2)
    text("DO NOT POPULATE - NO COPPER", 74, 23.0, 1.0)
    text("USB", 111, 88, 1.2)
    text("POWER", 45, 88, 1.0)
    text("BUS", 27, 41, 1.0)
    text("LOGIC", 105, 18, 1.0)

    pcbnew.SaveBoard(str(OUT / f"{NAME}.kicad_pcb"), board)
    pcbnew.ExportSpecctraDSN(board, str(OUT / f"{NAME}.dsn"))


def import_routing(session_path: Path) -> None:
    """Import a checked Specctra session into the generated PCB."""
    import pcbnew

    board_path = OUT / f"{NAME}.kicad_pcb"
    board = pcbnew.LoadBoard(str(board_path))
    settings = board.GetDesignSettings()
    settings.m_MinSilkTextHeight = pcbnew.FromMM(1.00)
    settings.m_MinSilkTextThickness = pcbnew.FromMM(0.15)
    if not pcbnew.ImportSpecctraSES(board, str(session_path)):
        raise RuntimeError(f"Could not import routing session: {session_path}")
    pcbnew.SaveBoard(str(board_path), board)


def apply_phase3g1_dfm() -> None:
    """Apply the reviewed U3 no-via-in-pad DFM revision to the routed PCB."""
    import pcbnew

    write_project_tables()
    board_path = OUT / f"{NAME}.kicad_pcb"
    schematic_path = OUT / f"{NAME}.kicad_sch"
    board = pcbnew.LoadBoard(str(board_path))

    old_name = "Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_ThermalVias"
    new_name = FOOTPRINT["U3"].split(":", 1)[1]
    matches = [fp for fp in board.GetFootprints() if fp.GetReference() == "U3"]
    if len(matches) != 1:
        raise RuntimeError(f"Expected one U3 footprint, found {len(matches)}")
    old_fp = matches[0]
    current_name = old_fp.GetFPID().GetLibItemName()

    if current_name == old_name:
        net_by_pad: dict[str, object] = {}
        for pad in old_fp.Pads():
            if pad.GetNetCode() != 0:
                net_by_pad[pad.GetNumber()] = pad.GetNet()

        library = OUT / "footprints" / "Phase3G.pretty"
        new_fp = pcbnew.FootprintLoad(str(library), new_name)
        if new_fp is None:
            raise RuntimeError(f"Footprint not found: {new_name}")
        new_fp.SetReference(old_fp.GetReference())
        new_fp.SetValue(old_fp.GetValue())
        new_fp.SetPosition(old_fp.GetPosition())
        new_fp.SetOrientationDegrees(old_fp.GetOrientationDegrees())
        new_fp.Reference().SetLayer(pcbnew.F_Fab)
        new_fp.Reference().SetVisible(True)
        new_fp.Value().SetVisible(False)

        board.Remove(old_fp)
        board.Add(new_fp)
        for pad in new_fp.Pads():
            net = net_by_pad.get(pad.GetNumber())
            if net is not None:
                pad.SetNet(net)
        u3 = new_fp
    elif current_name == new_name:
        u3 = old_fp
    else:
        raise RuntimeError(f"Unexpected U3 footprint: {current_name}")

    pad9 = [pad for pad in u3.Pads() if pad.GetNumber() == "9"]
    if len(pad9) != 1 or pad9[0].GetAttribute() != pcbnew.PAD_ATTRIB_SMD:
        raise RuntimeError("Phase 3G.1 U3 must have one SMD exposed pad and no via-in-pad")
    if not pad9[0].IsOnLayer(pcbnew.F_Cu) or pad9[0].IsOnLayer(pcbnew.B_Cu):
        raise RuntimeError("Phase 3G.1 U3 exposed pad must be F.Cu only")

    gnd = board.FindNet("GND")
    if gnd is None:
        raise RuntimeError("GND net missing from routed board")
    expected_positions = [
        u3.GetPosition() + pcbnew.VECTOR2I_MM(dx, dy)
        for dx, dy in U3_PERIPHERAL_THERMAL_VIA_OFFSETS
    ]
    expected_keys = {(position.x, position.y) for position in expected_positions}
    existing_by_position = {
        (item.GetPosition().x, item.GetPosition().y): item
        for item in board.GetTracks()
        if isinstance(item, pcbnew.PCB_VIA)
        and (item.GetPosition().x, item.GetPosition().y) in expected_keys
    }
    for position in expected_positions:
        existing = existing_by_position.get((position.x, position.y))
        if existing is not None:
            if (
                existing.GetNetname() != "GND"
                or existing.GetWidth(pcbnew.F_Cu) != pcbnew.FromMM(0.60)
                or existing.GetDrillValue() != pcbnew.FromMM(0.30)
            ):
                raise RuntimeError(f"Unexpected item at U3 thermal-via position {position}")
            via = existing
        else:
            via = pcbnew.PCB_VIA(board)
            via.SetPosition(position)
            via.SetWidth(pcbnew.FromMM(0.60))
            via.SetDrill(pcbnew.FromMM(0.30))
            via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            via.SetNet(gnd)
            board.Add(via)
        via.SetFrontTentingMode(pcbnew.TENTING_MODE_TENTED)
        via.SetBackTentingMode(pcbnew.TENTING_MODE_TENTED)

    board.GetDesignSettings().m_MinThroughDrill = pcbnew.FromMM(0.30)
    if not pcbnew.ZONE_FILLER(board).Fill(board.Zones()):
        raise RuntimeError("Copper-zone fill failed")
    pcbnew.SaveBoard(str(board_path), board)

    schematic = schematic_path.read_text(encoding="utf-8")
    if old_name in schematic:
        schematic = schematic.replace(old_name, new_name)
        schematic_path.write_text(schematic, encoding="utf-8")
    elif new_name not in schematic:
        raise RuntimeError("Unexpected U3 schematic footprint reference")


def finalize_pcb() -> None:
    """Add reviewed copper pours and production-facing board markings."""
    import pcbnew

    board_path = OUT / f"{NAME}.kicad_pcb"
    board = pcbnew.LoadBoard(str(board_path))

    # Component references belong on the fabrication/assembly layer.  The
    # operational silkscreen uses semantic names so test points and safety
    # controls remain understandable without a lookup table.
    for fp in board.GetFootprints():
        fp.Reference().SetLayer(pcbnew.F_Fab)
        fp.Reference().SetVisible(True)
        fp.Value().SetVisible(False)

    def add_text(value: str, x: float, y: float, size: float = 1.0) -> None:
        item = pcbnew.PCB_TEXT(board)
        item.SetText(value)
        item.SetPosition(pcbnew.VECTOR2I_MM(x, y))
        item.SetLayer(pcbnew.F_SilkS)
        item.SetTextSize(pcbnew.VECTOR2I_MM(size, size))
        item.SetTextThickness(pcbnew.FromMM(max(0.15, size * 0.15)))
        board.Add(item)

    semantic_labels = {
        "BAT": (43, 86), "5V": (65, 76), "3V3": (91, 76),
        "OE": (86, 27), "GATE_Y": (84, 17), "TXD": (64, 17),
        "TWAI_TX": (91, 17), "RESET": (98, 86), "STB": (65, 86),
        "EN": (73, 86), "RXD": (70, 27), "ERR": (78, 27),
        "CANH": (24, 17), "CANL": (31, 17), "GND": (38, 17),
    }
    for label, (x, y) in semantic_labels.items():
        add_text(label, x, y, 1.0)

    for label, x, y, size in [
        ("BAT 10..18V", 25, 78, 1.0), ("1:+  2:GND", 25, 80, 1.0),
        ("KCAN 1:H 2:L 3:GND", 31, 38, 1.0),
        ("JP_RX_MODE", 75, 59, 1.0), ("OPEN @ BOOT", 82, 62, 1.0),
        ("U1 TJA1055", 56, 47, 1.0), ("U2 TX GATE", 84, 45, 1.0),
        ("U3 5V LDO", 58, 77, 1.0),
        ("5V_TJA ISOLATED FROM DEVKIT 5V/VBUS", 103, 82, 1.0),
        ("DEVKIT USB OUT", 111, 86, 1.0),
    ]:
        add_text(label, x, y, size)

    gnd = board.FindNet("GND")
    if gnd is None:
        raise RuntimeError("GND net missing from routed board")

    def add_zone(
        name: str,
        layer: int,
        points: list[tuple[float, float]],
        connection: int,
        priority: int,
    ) -> None:
        zone = pcbnew.ZONE(board)
        zone.SetZoneName(name)
        zone.SetLayer(layer)
        zone.SetNetCode(gnd.GetNetCode())
        zone.SetMinThickness(pcbnew.FromMM(0.25))
        zone.SetLocalClearance(pcbnew.FromMM(0.30))
        zone.SetPadConnection(connection)
        zone.SetAssignedPriority(priority)
        polygon = zone.Outline()
        outline_index = polygon.NewOutline()
        outline = polygon.Outline(outline_index)
        for x, y in points:
            outline.Append(pcbnew.VECTOR2I_MM(x, y))
        board.Add(zone)

    existing = {zone.GetZoneName() for zone in board.Zones()}
    if "GND_BOTTOM_PLANE" not in existing:
        add_zone(
            "GND_BOTTOM_PLANE", pcbnew.B_Cu,
            [(20.8, 10.8), (139.2, 10.8), (139.2, 89.2), (20.8, 89.2)],
            pcbnew.ZONE_CONNECTION_THERMAL, 0,
        )
    if "U3_TOP_HEATSPREADER_600MM2_NOMINAL" not in existing:
        add_zone(
            "U3_TOP_HEATSPREADER_600MM2_NOMINAL", pcbnew.F_Cu,
            [(43, 62), (73, 62), (73, 82), (43, 82)],
            pcbnew.ZONE_CONNECTION_FULL, 1,
        )

    if not pcbnew.ZONE_FILLER(board).Fill(board.Zones()):
        raise RuntimeError("Copper-zone fill failed")
    pcbnew.SaveBoard(str(board_path), board)


def generate_phase3h_pcb() -> None:
    """Derive Phase 3H from the routed Phase 3G.1 board without rewriting it.

    The historical board is read-only input.  Obsolete TX-gap copper and mode
    jumper branches are removed before the dual-supply translator and complete
    TX path are routed on the new board.
    """
    import shutil
    import pcbnew

    write_project_tables()
    board_path = OUT / f"{NAME}.kicad_pcb"
    shutil.copy2(SOURCE_BOARD, board_path)

    # Remove the two historical keepout-zone S-expressions before pcbnew loads
    # the board.  KiCad 10.0.0's Windows SWIG binding can return untyped
    # SwigPyObject handles for ZONE items when a board is mutated in the same
    # process.  This bounded parser only edits the generated Phase 3H copy and
    # leaves every other zone and the Phase 3G source file byte-for-byte intact.
    board_text = board_path.read_text(encoding="utf-8")

    def expression_end_at(text: str, expression_start: int) -> int:
        depth = 0
        in_string = False
        escaped = False
        expression_end = None
        for offset in range(expression_start, len(text)):
            char = text[offset]
            if in_string:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    in_string = False
                continue
            if char == '"':
                in_string = True
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    expression_end = offset + 1
                    break
        if expression_end is None:
            raise RuntimeError("Unterminated KiCad S-expression")
        return expression_end

    def remove_expressions(text: str, head: str, predicate) -> str:
        cursor = 0
        prefix = f"\n\t({head}"
        while True:
            start = text.find(prefix, cursor)
            if start < 0:
                return text
            expression_start = start + 2
            end = expression_end_at(text, expression_start)
            expression = text[expression_start:end]
            if predicate(expression):
                text = text[:start] + text[end:]
                cursor = start
            else:
                cursor = end

    obsolete_refs = {"U2", "R3", "JP1", "TP4", "TP5", "TP6"}
    board_text = remove_expressions(
        board_text,
        "footprint",
        lambda expression: any(
            f'(property "Reference" "{reference}"' in expression
            for reference in obsolete_refs
        ),
    )
    obsolete_route_nets = {
        "TX_GATE_IN", "TX_GATE_OUT", "OE_SAFE", "RX_MODE_FEED"
    }
    for head in ("segment", "via"):
        board_text = remove_expressions(
            board_text,
            head,
            lambda expression: any(
                f'(net "{net_name}")' in expression
                for net_name in obsolete_route_nets
            ),
        )
    # Remove the two short 3V3 branches that ended only on the deleted Phase
    # 3G U2 and R3 pads.  Their upstream junctions remain part of the 3V3 rail.
    obsolete_3v_stubs = {
        "85.1375 47.05", "81.0875 55", "82.3263 53.7612",
        "88.0875 53.7612",
    }
    board_text = remove_expressions(
        board_text,
        "segment",
        lambda expression: '(net "3V3")' in expression
        and any(coordinate in expression for coordinate in obsolete_3v_stubs),
    )
    board_text = remove_expressions(
        board_text,
        "zone",
        lambda expression: '(name "TX_PHYSICAL_GAP_KEEP_OUT")' in expression,
    )
    board_text = remove_expressions(board_text, "gr_text", lambda _: True)

    for old_name, new_name in {
        "TX_GPIO": "KCAN_TX_MCU",
        "TX_GATE_IN": "KCAN_TX_A",
        "TXD_SAFE": "KCAN_TXD",
        "RXD_OD": "KCAN_RXD_OD",
        "RXD_GPIO": "KCAN_RX_MCU",
        "ERR_OD": "KCAN_ERR_OD",
        "ERR_GPIO": "KCAN_ERR_MCU",
        "STB_SAFE": "KCAN_STB",
        "EN_SAFE": "KCAN_EN",
    }.items():
        board_text = board_text.replace(f'"{old_name}"', f'"{new_name}"')
    board_path.write_text(board_text, encoding="utf-8", newline="\n")

    board = pcbnew.LoadBoard(str(board_path))
    board.SetFileName(str(board_path))
    title = board.GetTitleBlock()
    title.SetTitle("BMW E9x K-CAN FULL BIDIRECTIONAL — PHASE 3H")
    title.SetRevision("3H-A")
    title.SetDate("2026-10-05")
    title.SetComment(0, "FULL RX + TX HARDWARE; NO BMW COMMANDS")

    # Hold stable wrappers for the source items.  Re-enumerating BOARD
    # collections after removals can dereference an invalid SWIG iterator in
    # KiCad 10.0.0 on Windows.
    footprints_by_ref = {
        item.GetReference(): item for item in board.GetFootprints()
    }
    track_items = list(board.GetTracks())

    def fp(reference: str):
        item = footprints_by_ref.get(reference)
        if item is None:
            raise RuntimeError(f"Expected one footprint {reference}, found none")
        return item

    def pad(reference: str, number: str):
        matches = [item for item in fp(reference).Pads() if item.GetNumber() == number]
        if len(matches) != 1:
            raise RuntimeError(f"Expected one pad {reference}.{number}, found {len(matches)}")
        return matches[0]

    def ensure_net(name: str):
        existing = board.FindNet(name)
        if existing is not None:
            return existing
        net = pcbnew.NETINFO_ITEM(board, name)
        board.Add(net)
        return net

    def migrate_net(old_name: str, new_name: str) -> None:
        old = board.FindNet(old_name)
        if old is None:
            return
        new = ensure_net(new_name)
        for item in footprints_by_ref.values():
            for item_pad in item.Pads():
                if item_pad.GetNetname() == old_name:
                    item_pad.SetNet(new)
        for track in track_items:
            if track.GetNetname() == old_name:
                track.SetNet(new)

    def remove_tracks_on(net_names: set[str]) -> None:
        retained = []
        for track in track_items:
            if track.GetNetname() in net_names:
                board.Remove(track)
            else:
                retained.append(track)
        track_items[:] = retained

    def remove_footprint(reference: str) -> None:
        item = footprints_by_ref.pop(reference, None)
        if item is None:
            raise RuntimeError(f"Expected one footprint {reference}, found none")
        board.Remove(item)

    nets = {
        name: ensure_net(name)
        for name in (
            "GND", "3V3", "5V_TJA", "KCAN_TX_MCU", "KCAN_TX_A", "KCAN_TXD",
            "KCAN_RXD_OD", "KCAN_RX_MCU", "KCAN_ERR_OD", "KCAN_ERR_MCU",
            "KCAN_STB_MCU", "KCAN_STB", "KCAN_EN_MCU", "KCAN_EN",
        )
    }

    library = FOOTPRINTS / "Package_TO_SOT_SMD.pretty"
    u2 = pcbnew.FootprintLoad(str(library), "SOT-363_SC-70-6")
    if u2 is None:
        raise RuntimeError("SOT-363_SC-70-6 footprint not found")
    u2.SetReference("U2")
    u2.SetValue("SN74LXC1T45QDCKRQ1")
    u2.SetPosition(pcbnew.VECTOR2I_MM(84, 22))
    u2.SetOrientationDegrees(180)
    board.Add(u2)
    footprints_by_ref["U2"] = u2

    capacitor_library = FOOTPRINTS / "Capacitor_SMD.pretty"
    c7 = pcbnew.FootprintLoad(str(capacitor_library), "C_0805_2012Metric")
    if c7 is None:
        raise RuntimeError("C_0805_2012Metric footprint not found")
    c7.SetReference("C7")
    c7.SetValue("100n / C_U2_5V")
    c7.SetPosition(pcbnew.VECTOR2I_MM(88, 26))
    board.Add(c7)
    footprints_by_ref["C7"] = c7

    c8 = pcbnew.FootprintLoad(str(capacitor_library), "C_0805_2012Metric")
    if c8 is None:
        raise RuntimeError("C_0805_2012Metric footprint not found")
    c8.SetReference("C8")
    c8.SetValue("100n / C_U2_3V3")
    c8.SetPosition(pcbnew.VECTOR2I_MM(80, 17))
    board.Add(c8)
    footprints_by_ref["C8"] = c8

    testpoint_library = FOOTPRINTS / "TestPoint.pretty"
    tp5 = pcbnew.FootprintLoad(str(testpoint_library), "TestPoint_Pad_D1.5mm")
    if tp5 is None:
        raise RuntimeError("TestPoint_Pad_D1.5mm footprint not found")
    tp5.SetReference("TP5")
    tp5.SetValue("TXD")
    tp5.SetPosition(pcbnew.VECTOR2I_MM(64, 19))
    board.Add(tp5)
    footprints_by_ref["TP5"] = tp5

    pad_assignments = {
        ("U2", "1"): "3V3",
        ("U2", "2"): "GND",
        ("U2", "3"): "KCAN_TX_A",
        ("U2", "4"): "KCAN_TXD",
        ("U2", "5"): "3V3",
        ("U2", "6"): "5V_TJA",
        ("C7", "1"): "5V_TJA",
        ("C7", "2"): "GND",
        ("C8", "1"): "3V3",
        ("C8", "2"): "GND",
        ("TP5", "1"): "KCAN_TXD",
        ("U1", "2"): "KCAN_TXD",
        ("R2", "2"): "KCAN_TXD",
        ("H1", "6"): "KCAN_STB_MCU",
        ("R10", "1"): "KCAN_STB_MCU",
        ("H1", "8"): "KCAN_EN_MCU",
        ("R12", "1"): "KCAN_EN_MCU",
    }
    for (reference, number), net_name in pad_assignments.items():
        pad(reference, number).SetNet(nets[net_name])

    def point(item_pad):
        position = item_pad.GetPosition()
        return (pcbnew.ToMM(position.x), pcbnew.ToMM(position.y))

    def add_track(net_name: str, points: list[tuple[float, float]], layer=pcbnew.F_Cu) -> None:
        for start, end in zip(points, points[1:]):
            track = pcbnew.PCB_TRACK(board)
            track.SetStart(pcbnew.VECTOR2I_MM(*start))
            track.SetEnd(pcbnew.VECTOR2I_MM(*end))
            track.SetWidth(pcbnew.FromMM(0.20))
            track.SetLayer(layer)
            track.SetNet(nets[net_name])
            board.Add(track)

    def add_via(net_name: str, position: tuple[float, float]) -> None:
        via = pcbnew.PCB_VIA(board)
        via.SetPosition(pcbnew.VECTOR2I_MM(*position))
        via.SetWidth(pcbnew.FromMM(0.60))
        via.SetDrill(pcbnew.FromMM(0.30))
        via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        via.SetNet(nets[net_name])
        board.Add(via)

    # Translator input.  The GPIO5-to-R5 copper from Phase 3G.1 is preserved;
    # the translator-side route uses a clear B.Cu corridor above the DevKit.
    tx_a_via_low = (88.05, 48.713)
    tx_a_via_top = (88.0, 20.0)
    add_track("KCAN_TX_A", [
        point(pad("R5", "2")), (91.075, 46.163), (90.6, 46.163), tx_a_via_low,
    ])
    add_track("KCAN_TX_A", [
        point(pad("R4", "2")), (89.9125, 50.575), tx_a_via_low,
    ])
    add_via("KCAN_TX_A", tx_a_via_low)
    add_track(
        "KCAN_TX_A",
        [tx_a_via_low, (88.0, 24.0), tx_a_via_top],
        pcbnew.B_Cu,
    )
    add_via("KCAN_TX_A", tx_a_via_top)
    add_track("KCAN_TX_A", [tx_a_via_top, point(pad("U2", "3"))])

    # Dual-rail supply and local decoupling.  The 5 V path also restores the
    # layer transition formerly made by the removed through-hole JP1 pad.
    add_via("5V_TJA", (73.0, 48.0))
    five_volt_via_top = (78.0, 29.0)
    add_track("5V_TJA", [(73.0, 48.0), (77.0, 44.0), (77.0, 31.0), five_volt_via_top], pcbnew.B_Cu)
    u2_5v_via = (81.0, 25.0)
    add_via("5V_TJA", u2_5v_via)
    add_track("5V_TJA", [point(pad("U2", "6")), u2_5v_via])
    add_track("5V_TJA", [five_volt_via_top, u2_5v_via], pcbnew.B_Cu)
    c7_5v_via = (86.0, 28.0)
    add_via("5V_TJA", c7_5v_via)
    add_track("5V_TJA", [five_volt_via_top, c7_5v_via], pcbnew.B_Cu)
    add_track("5V_TJA", [c7_5v_via, point(pad("C7", "1"))])

    u2_3v_left_via = (79.0, 21.0)
    u2_3v_right_via = (86.0, 25.0)
    c8_3v_via = (78.0, 17.0)
    for via_position in (u2_3v_left_via, u2_3v_right_via, c8_3v_via):
        add_via("3V3", via_position)
    add_track("3V3", [point(pad("U2", "5")), u2_3v_left_via])
    add_track("3V3", [point(pad("U2", "1")), u2_3v_right_via])
    add_track("3V3", [point(pad("C8", "1")), c8_3v_via])
    add_track(
        "3V3",
        [point(pad("H1", "1")), (98.0, 18.5), (78.0, 18.5), c8_3v_via],
        pcbnew.B_Cu,
    )
    add_track("3V3", [u2_3v_left_via, (82.0, 18.5)], pcbnew.B_Cu)
    add_track("3V3", [u2_3v_right_via, (86.0, 18.5)], pcbnew.B_Cu)

    for item_pad, ground_via in (
        (pad("U2", "2"), (87.0, 22.5)),
        (pad("C7", "2"), (90.0, 26.0)),
        (pad("C8", "2"), (82.0, 17.0)),
    ):
        add_track("GND", [point(item_pad), ground_via])
        add_via("GND", ground_via)

    # Continuous TX path.  R2 remains a bias on the same 5 V logic domain;
    # neither it nor TP5 interrupts the signal.
    add_track("KCAN_TXD", [
        point(pad("U2", "4")), (80.0, 19.0), point(pad("TP5", "1")),
    ])

    # Normal TJA1055 mode-control pins.  Both are firmware-controlled and
    # passively LOW during reset; there is no jumper or TX-enable mechanism.
    stb_via = (66.0, 43.5)
    en_via = (64.0, 49.5)
    stb_start_via = (102.0, 34.7)
    en_start_via = (96.0, 39.78)
    add_track("KCAN_STB_MCU", [point(pad("H1", "6")), stb_start_via])
    add_via("KCAN_STB_MCU", stb_start_via)
    add_track(
        "KCAN_STB_MCU",
        [stb_start_via, (104.0, 36.7), (104.0, 85.0), (60.0, 85.0), (60.0, 45.0), stb_via],
        pcbnew.B_Cu,
    )
    add_via("KCAN_STB_MCU", stb_via)
    add_track("KCAN_STB_MCU", [stb_via, point(pad("R10", "1"))])
    add_track("KCAN_EN_MCU", [point(pad("H1", "8")), en_start_via])
    add_via("KCAN_EN_MCU", en_start_via)
    en_hop_high = (62.0, 86.0)
    en_hop_low = (62.0, 84.0)
    en_hop_right_low = (94.0, 84.0)
    en_hop_right_high = (94.0, 86.0)
    add_track(
        "KCAN_EN_MCU",
        [en_start_via, (94.0, 42.0), en_hop_right_low],
        pcbnew.B_Cu,
    )
    add_via("KCAN_EN_MCU", en_hop_right_low)
    add_track("KCAN_EN_MCU", [en_hop_right_low, en_hop_right_high])
    add_via("KCAN_EN_MCU", en_hop_right_high)
    add_track(
        "KCAN_EN_MCU",
        [en_hop_right_high, (94.0, 88.0), (62.0, 88.0), en_hop_high],
        pcbnew.B_Cu,
    )
    add_via("KCAN_EN_MCU", en_hop_high)
    add_track("KCAN_EN_MCU", [en_hop_high, en_hop_low])
    add_via("KCAN_EN_MCU", en_hop_low)
    add_track("KCAN_EN_MCU", [en_hop_low, (62.0, 52.0), en_via], pcbnew.B_Cu)
    add_via("KCAN_EN_MCU", en_via)
    add_track("KCAN_EN_MCU", [en_via, point(pad("R12", "1"))])

    # All design text on the derived board is regenerated so no RX-only or
    # physical-gap statement can survive in the Phase 3H manufacturing data.
    def add_text(value: str, x: float, y: float, size: float = 1.0) -> None:
        item = pcbnew.PCB_TEXT(board)
        item.SetText(value)
        item.SetPosition(pcbnew.VECTOR2I_MM(x, y))
        item.SetLayer(pcbnew.F_SilkS)
        item.SetTextSize(pcbnew.VECTOR2I_MM(size, size))
        item.SetTextThickness(pcbnew.FromMM(max(0.15, size * 0.15)))
        board.Add(item)

    for value, x, y, size in (
        ("BMW E9x K-CAN BIDIRECTIONAL", 76, 12, 1.6),
        ("PHASE 3H / RX + TX", 76, 15, 1.2),
        ("NO BMW COMMANDS IN THIS HARDWARE PHASE", 77, 88, 1.0),
        ("TXD", 75, 40, 1.0), ("TX_MCU", 92, 16, 1.0),
        ("RXD", 70, 27, 1.0), ("ERR", 78, 27, 1.0),
        ("STB", 65, 86, 1.0), ("EN", 73, 86, 1.0),
        ("CANH", 24, 17, 1.0), ("CANL", 31, 17, 1.0),
        ("GND", 38, 17, 1.0), ("BAT", 43, 86, 1.0),
        ("5V", 65, 76, 1.0), ("3V3", 91, 76, 1.0),
        ("RESET", 98, 86, 1.0),
        ("BAT 10..18V", 25, 78, 1.0), ("1:+  2:GND", 25, 80, 1.0),
        ("KCAN 1:H 2:L 3:GND", 31, 38, 1.0),
        ("U1 TJA1055", 56, 47, 1.0),
        ("U2 3V3-5V TX", 84, 45, 1.0),
        ("U3 5V LDO", 58, 77, 1.0),
        ("DEVKIT 5V/VBUS NOT CONNECTED", 106, 82, 1.0),
        ("DEVKIT USB", 111, 86, 1.0),
    ):
        add_text(value, x, y, size)

    for item in footprints_by_ref.values():
        item.Reference().SetLayer(pcbnew.F_Fab)
        item.Reference().SetVisible(True)
        item.Value().SetVisible(False)

    if not pcbnew.ZONE_FILLER(board).Fill(board.Zones()):
        raise RuntimeError("Copper-zone fill failed")
    pcbnew.SaveBoard(str(board_path), board)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "mode",
        choices=(
            "schematic",
            "pcb",
            "tables",
            "import-ses",
            "phase3g1-dfm",
            "finalize",
        ),
    )
    parser.add_argument("--ses", type=Path)
    args = parser.parse_args()
    if args.mode == "schematic":
        generate_schematic()
    elif args.mode == "pcb":
        generate_phase3h_pcb()
    elif args.mode == "import-ses":
        if args.ses is None:
            parser.error("import-ses requires --ses")
        import_routing(args.ses.resolve())
    elif args.mode == "phase3g1-dfm":
        apply_phase3g1_dfm()
    elif args.mode == "finalize":
        finalize_pcb()
    else:
        write_project_tables()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
