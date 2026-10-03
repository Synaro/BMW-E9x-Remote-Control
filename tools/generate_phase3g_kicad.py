#!/usr/bin/env python3
"""Generate the reviewed Phase 3G KiCad schematic and unrouted PCB.

Run the schematic half with the repository Python after installing
``kicad-sch-api==0.5.6``. Run the PCB half with KiCad's bundled Python so that
``pcbnew`` is available. Generated files are committed; CI validates them and
does not need either generator dependency.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "hardware" / "kcan-rxonly-pcb" / "kicad"
NAME = "BMW-E9x-KCAN-RXOnly"

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
    "U2": "Package_TO_SOT_SMD:SOT-23-5",
    "U3": "Phase3G:Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_ThermalVias",
    "DEV": "Connector_PinSocket_2.54mm:PinSocket_1x22_P2.54mm_Vertical",
    "R0805": "Resistor_SMD:R_0805_2012Metric",
    "R1206": "Resistor_SMD:R_1206_3216Metric",
    "C0805": "Capacitor_SMD:C_0805_2012Metric",
    "C1206": "Capacitor_SMD:C_1206_3216Metric",
    "F1812": "Phase3G:Fuse_1812_4532Metric_Pad1.30x3.40mm_Reviewed",
    "D_SMA": "Diode_SMD:D_SMA",
    "J2": "Connector_JST:JST_XH_B2B-XH-A_1x02_P2.50mm_Vertical",
    "J3": "Connector_JST:JST_XH_B3B-XH-A_1x03_P2.50mm_Vertical",
    "JP2": "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical",
    "TP": "TestPoint:TestPoint_Pad_D1.5mm",
    "MH": "MountingHole:MountingHole_3.2mm_M3",
}


TESTPOINTS = [
    ("TP1", "TP_BAT", "BAT_LOCAL"),
    ("TP2", "TP_5V", "5V_TJA"),
    ("TP3", "TP_3V3", "3V3"),
    ("TP4", "TP_OE", "OE_SAFE"),
    ("TP5", "TP_GATE_Y", "TX_GATE_OUT"),
    ("TP6", "TP_TXD", "TXD_SAFE"),
    ("TP7", "TP_TWAI_TX", "TX_GPIO"),
    ("TP8", "TP_RESET", "RESET"),
    ("TP9", "TP_STB", "STB_SAFE"),
    ("TP10", "TP_EN", "EN_SAFE"),
    ("TP11", "TP_RXD", "RXD_OD"),
    ("TP12", "TP_ERR", "ERR_OD"),
    ("TP13", "TP_CANH", "KCAN_H"),
    ("TP14", "TP_CANL", "KCAN_L"),
    ("TP15", "TP_GND", "GND"),
]


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
            '  (lib (name "Phase3G")(type "KiCad")'
            '(uri "${KIPRJMOD}/footprints/Phase3G.pretty")'
            '(options "")(descr "Reviewed local Phase 3G footprints"))'
            if lib == "Phase3G" else
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
    target_name = "Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_ThermalVias"
    source = FOOTPRINTS / "Package_SO.pretty" / f"{source_name}.kicad_mod"
    target_dir = OUT / "footprints" / "Phase3G.pretty"
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
    # KiCad does not currently ship a model carrying the -52 package name.
    # Use the exact 3.9 x 4.9 mm / 1.27 mm mechanical envelope model; the
    # exposed-pad geometry remains authoritative in the footprint copper.
    contents = contents.replace(
        "Package_SO.3dshapes/Infineon_PG-DSO-8-27_3.9x4.9mm_EP2.65x3mm.step",
        "Package_SO.3dshapes/SO-8_3.9x4.9mm_P1.27mm.step",
    )
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
        title="BMW E9x K-CAN RX-ONLY — PHASE 3G PROTOTYPE",
        date="2026-10-03",
        rev="3G-A",
        company="Synaro / open hardware prototype",
        comments={
            1: "BENCH ONLY — NO VEHICLE CONNECTION BEFORE QUALIFICATION",
            2: "ESP32 USB ONLY; 5V_TJA FROM TLS715B0EJV50",
            3: "NO COPPER BETWEEN U2.Y AND U1.TXD",
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
    add("Connector_Generic:Conn_01x05", "U2", "CLVC1G125QDBVRQ1", (170, 85), FOOTPRINT["U2"])
    add("Connector_Generic:Conn_01x08", "U3", "TLS715B0EJV50XUMA1", (45, 85), FOOTPRINT["U3"])
    add("Connector_Generic:Conn_01x22", "H1", "ESP32-S3-DevKitC-1 J1", (225, 76), FOOTPRINT["DEV"])
    add("Connector_Generic:Conn_01x22", "H2", "ESP32-S3-DevKitC-1 J3", (255, 76), FOOTPRINT["DEV"])

    parts = [
        ("Device:Polyfuse", "F1", "MINISMDC010F-2 / F_BAT", (25, 135), FOOTPRINT["F1812"]),
        ("Device:D_Schottky", "D1", "SS16 / D_BAT", (45, 135), FOOTPRINT["D_SMA"]),
        ("Device:R", "R1", "1k / R_BAT", (30, 135), FOOTPRINT["R1206"], 90),
        ("Device:R", "R2", "10k / R_TXD", (60, 135), FOOTPRINT["R0805"], 90),
        ("Device:R", "R3", "10k / R_OE", (90, 135), FOOTPRINT["R0805"], 90),
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
        ("Connector_Generic:Conn_01x02", "J1", "BENCH 10..18V / J_BAT", (25, 55), FOOTPRINT["J2"]),
        ("Connector_Generic:Conn_01x03", "J2", "LSFT BENCH BUS / J_KCAN", (65, 55), FOOTPRINT["J3"]),
        ("Connector_Generic:Conn_01x02", "JP1", "RX MODE REMOVE AT BOOT", (135, 55), FOOTPRINT["JP2"]),
    ]
    for item in parts:
        add(*item)

    for index, (ref, semantic_ref, _) in enumerate(TESTPOINTS):
        x = 30 + (index % 8) * 28
        y = 220 + (index // 8) * 20
        add("Connector:TestPoint", ref, semantic_ref.removeprefix("TP_"), (x, y), FOOTPRINT["TP"])

    pin_nets: dict[str, dict[str, str]] = {
        "U1": {"2":"TXD_SAFE","3":"RXD_OD","4":"ERR_OD","5":"STB_SAFE","6":"EN_SAFE","7":"BAT_LOCAL","8":"RTH","9":"RTL","10":"5V_TJA","11":"KCAN_H","12":"KCAN_L","13":"GND","14":"BAT_LOCAL"},
        "U2": {"1":"OE_SAFE","2":"TX_GATE_IN","3":"GND","4":"TX_GATE_OUT","5":"3V3"},
        "U3": {"1":"BAT_PROTECTED","2":"BAT_PROTECTED","3":"GND","8":"5V_TJA"},
        "H1": {"1":"3V3","3":"RESET","4":"RXD_GPIO","5":"TX_GPIO","7":"ERR_GPIO"},
        "H2": {"1":"GND"},
        "F1": {"1":"BAT_INPUT","2":"BAT_FUSED"},
        "D1": {"1":"BAT_PROTECTED","2":"BAT_FUSED"},
        "R1": {"1":"BAT_PROTECTED","2":"BAT_LOCAL"},
        "R2": {"1":"5V_TJA","2":"TXD_SAFE"},
        "R3": {"1":"3V3","2":"OE_SAFE"},
        "R4": {"1":"3V3","2":"TX_GATE_IN"},
        "R5": {"1":"TX_GPIO","2":"TX_GATE_IN"},
        "R6": {"1":"3V3","2":"RXD_OD"},
        "R7": {"1":"RXD_OD","2":"RXD_GPIO"},
        "R8": {"1":"3V3","2":"ERR_OD"},
        "R9": {"1":"ERR_OD","2":"ERR_GPIO"},
        "R10": {"1":"RX_MODE_FEED","2":"STB_SAFE"},
        "R11": {"1":"STB_SAFE","2":"GND"},
        "R12": {"1":"RX_MODE_FEED","2":"EN_SAFE"},
        "R13": {"1":"EN_SAFE","2":"GND"},
        "R14": {"1":"KCAN_H","2":"RTH"},
        "R15": {"1":"KCAN_L","2":"RTL"},
        "C1": {"1":"BAT_PROTECTED","2":"GND"},
        "C2": {"1":"5V_TJA","2":"GND"},
        "C3": {"1":"5V_TJA","2":"GND"},
        "C4": {"1":"5V_TJA","2":"GND"},
        "C5": {"1":"BAT_LOCAL","2":"GND"},
        "C6": {"1":"3V3","2":"GND"},
        "J1": {"1":"BAT_INPUT","2":"GND"},
        "J2": {"1":"KCAN_H","2":"KCAN_L","3":"GND"},
        "JP1": {"1":"5V_TJA","2":"RX_MODE_FEED"},
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
        "H1": [str(x) for x in range(2, 23) if x not in {3,4,5,7}],
        "H2": [str(x) for x in range(2, 23)],
    }
    for ref, pins in unused.items():
        for pin in pins:
            sch.no_connects.add(sch.get_component_pin_position(ref, pin))

    sch.add_text("POWER: 10..18V -> F_BAT -> D_BAT -> TLS715B0EJV50 -> 5V_TJA", (80, 25), size=1.5)
    sch.add_text("ESP32 USB ONLY — DEVKIT 5V PIN NC — COMMON GND ONLY", (70, 32), size=1.5)
    sch.add_text("TX LINK DNP / DO NOT POPULATE: NO COMPONENT AND NO COPPER", (205, 32), size=1.5)
    sch.add_text("U2.Y -> TP_GATE_Y [PHYSICAL END]     U1.TXD <- R_TXD <- 5V_TJA", (205, 39), size=1.3)
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
    # The exact Infineon thermal-via footprint contains nine 0.20 mm drills.
    # They are the only sub-0.30 mm drills in the design and are explicitly
    # called out in the fabrication notes; both retained prototype houses can
    # manufacture 0.20 mm mechanically drilled vias in this stack-up.
    settings.m_MinThroughDrill = pcbnew.FromMM(0.2)

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


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "mode",
        choices=("schematic", "pcb", "tables", "import-ses", "finalize"),
    )
    parser.add_argument("--ses", type=Path)
    args = parser.parse_args()
    if args.mode == "schematic":
        generate_schematic()
    elif args.mode == "pcb":
        generate_pcb()
    elif args.mode == "import-ses":
        if args.ses is None:
            parser.error("import-ses requires --ses")
        import_routing(args.ses.resolve())
    elif args.mode == "finalize":
        finalize_pcb()
    else:
        write_project_tables()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
