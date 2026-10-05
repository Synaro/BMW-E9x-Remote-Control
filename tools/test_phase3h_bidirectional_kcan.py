import csv
import hashlib
import re
import unittest
from pathlib import Path
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[1]
HW = ROOT / "hardware" / "kcan-bidirectional-pcb"
KICAD = HW / "kicad"
MFG = HW / "manufacturing"
BOARD = KICAD / "BMW-E9x-KCAN-Bidirectional.kicad_pcb"
SCHEMATIC = KICAD / "BMW-E9x-KCAN-Bidirectional.kicad_sch"
NAME = "BMW-E9x-KCAN-Bidirectional"
U3_FOOTPRINT = (
    KICAD
    / "footprints"
    / "Phase3H.pretty"
    / "Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_NoViaInPad.kicad_mod"
)


def read_csv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames is None:
            raise AssertionError(f"CSV without a header: {path}")
        return list(reader.fieldnames), list(reader)


def split_refs(value: str) -> list[str]:
    return [ref for ref in re.split(r"[,\s]+", value.strip()) if ref]


def sexpr_blocks(text: str, prefix: str) -> list[str]:
    blocks: list[str] = []
    offset = 0
    while True:
        start = text.find(prefix, offset)
        if start < 0:
            return blocks
        depth = 0
        quoted = False
        escaped = False
        for index in range(start, len(text)):
            char = text[index]
            if quoted:
                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == '"':
                    quoted = False
                continue
            if char == '"':
                quoted = True
            elif char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    blocks.append(text[start : index + 1])
                    offset = index + 1
                    break
        else:
            raise AssertionError(f"Unbalanced s-expression: {prefix}")


def footprint(board_text: str, reference: str) -> str:
    marker = f'(property "Reference" "{reference}"'
    matches = [
        block for block in sexpr_blocks(board_text, "(footprint") if marker in block
    ]
    if len(matches) != 1:
        raise AssertionError(f"Expected one footprint {reference}, found {len(matches)}")
    return matches[0]


def pad_has_net(fp: str, pad: str, net: str) -> bool:
    return any(
        f'"{net}"' in block for block in sexpr_blocks(fp, f'(pad "{pad}"')
    )


def position_rows(path: Path) -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        columns = line.split()
        if len(columns) != 7:
            raise AssertionError(f"Unexpected KiCad position row: {line}")
        rows.append(
            dict(zip(("Ref", "Val", "Package", "PosX", "PosY", "Rot", "Side"), columns))
        )
    return rows


class Phase3HBidirectionalKcanTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.board = BOARD.read_text(encoding="utf-8")
        cls.schematic = SCHEMATIC.read_text(encoding="utf-8")
        _, cls.bom = read_csv(HW / "BOM_PHASE3H.csv")
        _, cls.netlist = read_csv(HW / "netlist_PHASE3H.csv")
        _, cls.pin_review = read_csv(HW / "pin-review.csv")
        cls.mounted = [row for row in cls.bom if row["assembly"] == "MOUNT"]
        cls.mounted_refs = {
            ref for row in cls.mounted for ref in split_refs(row["reference"])
        }

    def assert_pad_net(self, reference: str, pad: str, net: str):
        self.assertTrue(
            pad_has_net(footprint(self.board, reference), pad, net),
            f"{reference}.{pad} must be on {net}",
        )

    def test_native_and_manufacturing_artifacts_exist(self):
        required = (
            KICAD / f"{NAME}.kicad_pro",
            SCHEMATIC,
            BOARD,
            KICAD / "phase3h-erc.rpt",
            KICAD / "phase3h-drc-final.rpt",
            MFG / f"{NAME}-fabrication.zip",
            MFG / "bom" / f"{NAME}-PCBA-BOM.csv",
            MFG / "bom" / f"{NAME}-PCBA-BOM-JLCPCB.csv",
            MFG / "cpl" / f"{NAME}-PCBA-CPL.csv",
            MFG / "cpl" / f"{NAME}-PCBA-CPL-JLCPCB.csv",
            MFG / "nextpcb" / f"{NAME}-PCBA-BOM-NextPCB.csv",
            MFG / "nextpcb" / f"{NAME}-PCBA-Top.pos",
            MFG / "nextpcb" / f"{NAME}-PCBA-Centroid-NextPCB.zip",
            MFG / "drawings" / f"{NAME}-schematic.pdf",
            MFG / "drawings" / f"{NAME}-assembly.pdf",
            MFG / "drawings" / f"{NAME}-fabrication.pdf",
            MFG / "3d" / f"{NAME}.step",
            MFG / "reports" / f"{NAME}.ipc",
            MFG / "images" / f"{NAME}-front.png",
            MFG / "images" / f"{NAME}-back.png",
            MFG / "MANIFEST.sha256",
        )
        for path in required:
            self.assertTrue(path.is_file(), path)
            self.assertGreater(path.stat().st_size, 100, path)

    def test_erc_drc_and_unrouted_checks_are_clean(self):
        erc = (KICAD / "phase3h-erc.rpt").read_text(encoding="utf-8")
        drc = (KICAD / "phase3h-drc-final.rpt").read_text(encoding="utf-8")
        self.assertIn("ERC messages: 0", erc)
        self.assertIn("Found 0 DRC violations", drc)
        self.assertIn("Found 0 unconnected pads", drc)
        self.assertIn("Found 0 Footprint errors", drc)

    def test_complete_tx_path_exists_in_actual_board_netlist(self):
        # ESP32 GPIO5 -> R5 -> U2 A -> U2 B -> TJA1055 TXD.
        for reference, pad in (("H1", "5"), ("R5", "1"), ("TP7", "1")):
            self.assert_pad_net(reference, pad, "KCAN_TX_MCU")
        for reference, pad in (("R5", "2"), ("R4", "2"), ("U2", "3")):
            self.assert_pad_net(reference, pad, "KCAN_TX_A")
        for reference, pad in (("U2", "4"), ("U1", "2"), ("R2", "2"), ("TP5", "1")):
            self.assert_pad_net(reference, pad, "KCAN_TXD")

        routed_nets = {
            match.group(1)
            for segment in sexpr_blocks(self.board, "(segment")
            if (match := re.search(r'\(net\s+"([^"]+)"\)', segment))
        }
        self.assertTrue(
            {"KCAN_TX_MCU", "KCAN_TX_A", "KCAN_TXD"}.issubset(routed_nets)
        )

    def test_complete_rx_path_exists_in_actual_board_netlist(self):
        # TJA1055 RXD -> R7 -> ESP32 GPIO4.
        for reference, pad in (("U1", "3"), ("R6", "2"), ("R7", "1"), ("TP11", "1")):
            self.assert_pad_net(reference, pad, "KCAN_RXD_OD")
        for reference, pad in (("R7", "2"), ("H1", "4")):
            self.assert_pad_net(reference, pad, "KCAN_RX_MCU")

    def test_phase3h_cannot_regress_to_rx_only_gap_or_tx_interlock(self):
        forbidden = (
            "TX_PHYSICAL_GAP_KEEP_OUT",
            "R_LINK_TX",
            "TX_ENABLE",
            "TX_GATE_OUT",
            "TXD_SAFE",
            "PHYSICAL_GAP",
            "ISOLATED_TX",
        )
        combined = "\n".join((self.board, self.schematic))
        for token in forbidden:
            self.assertNotIn(token, combined)
        self.assertNotIn('(property "Reference" "JP1"', self.board)
        self.assertNotIn('(property "Reference" "R3"', self.board)
        self.assertEqual(0, self.board.count("(keepout"))

    def test_exact_critical_parts_and_u2_power_domains(self):
        by_ref = {row["reference"]: row for row in self.bom}
        self.assertEqual("TJA1055T/3/2Z", by_ref["U1"]["mpn"])
        self.assertEqual("SN74LXC1T45QDCKRQ1", by_ref["U2"]["mpn"])
        self.assertEqual("TLS715B0EJV50XUMA1", by_ref["U3"]["mpn"])
        for pad, net in (("1", "3V3"), ("2", "GND"), ("5", "3V3"), ("6", "5V_TJA")):
            self.assert_pad_net("U2", pad, net)

    def test_tja1055_mode_control_and_devkit_gpio_mapping(self):
        for reference, pad, net in (
            ("H1", "6", "KCAN_STB_MCU"),
            ("U1", "5", "KCAN_STB"),
            ("H1", "8", "KCAN_EN_MCU"),
            ("U1", "6", "KCAN_EN"),
            ("H1", "7", "KCAN_ERR_MCU"),
            ("U1", "4", "KCAN_ERR_OD"),
        ):
            self.assert_pad_net(reference, pad, net)
        reviewed = {(row["reference"], row["pin"]): row for row in self.pin_review}
        self.assertEqual("TWAI_RX", reviewed[("H1", "4")]["notes"])
        self.assertIn("TWAI_TX", reviewed[("H1", "5")]["notes"])
        self.assertTrue(all(row["status"] == "PASS" for row in self.pin_review))

    def test_u3_dfm_geometry_remains_standard_and_has_no_via_in_pad(self):
        footprint_text = U3_FOOTPRINT.read_text(encoding="utf-8")
        exposed = sexpr_blocks(footprint_text, '(pad "9"')
        self.assertEqual(1, len(exposed))
        self.assertIn("(size 2.65 3)", exposed[0])
        self.assertNotIn("thru_hole", exposed[0])
        self.assertNotIn("(drill ", exposed[0])

        vias = sexpr_blocks(self.board, "(via\n")
        expected_positions = ("57.2 70", "58.8 70", "57.2 74", "58.8 74")
        for position in expected_positions:
            matches = [via for via in vias if f"(at {position})" in via]
            self.assertEqual(1, len(matches), position)
            self.assertIn("(size 0.6)", matches[0])
            self.assertIn("(drill 0.3)", matches[0])
            self.assertIn('(net "GND")', matches[0])
        self.assertNotIn("(drill 0.2)", self.board)

    def test_devkit_usb_5v_remains_disconnected_from_tja_supply(self):
        h1_pad_21 = sexpr_blocks(footprint(self.board, "H1"), '(pad "21"')[0]
        self.assertNotIn("(net ", h1_pad_21)
        self.assert_pad_net("U3", "8", "5V_TJA")
        self.assert_pad_net("U1", "10", "5V_TJA")
        self.assert_pad_net("U2", "6", "5V_TJA")

    def test_all_manufacturer_exports_match_mounted_population(self):
        _, canonical_bom = read_csv(MFG / "bom" / f"{NAME}-PCBA-BOM.csv")
        _, canonical_cpl = read_csv(MFG / "cpl" / f"{NAME}-PCBA-CPL.csv")
        jlc_fields, jlc_bom = read_csv(MFG / "bom" / f"{NAME}-PCBA-BOM-JLCPCB.csv")
        jlc_cpl_fields, jlc_cpl = read_csv(MFG / "cpl" / f"{NAME}-PCBA-CPL-JLCPCB.csv")
        next_fields, next_bom = read_csv(
            MFG / "nextpcb" / f"{NAME}-PCBA-BOM-NextPCB.csv"
        )
        next_pos = position_rows(MFG / "nextpcb" / f"{NAME}-PCBA-Top.pos")

        self.assertEqual(self.mounted, canonical_bom)
        self.assertEqual(self.mounted_refs, {row["Ref"] for row in canonical_cpl})
        self.assertEqual(
            ["Comment", "Designator", "Footprint", "LCSC Part #"], jlc_fields
        )
        self.assertEqual(
            ["Designator", "Mid X", "Mid Y", "Layer", "Rotation"],
            jlc_cpl_fields,
        )
        self.assertEqual(
            self.mounted_refs,
            {ref for row in jlc_bom for ref in split_refs(row["Designator"])},
        )
        self.assertEqual(self.mounted_refs, {row["Designator"] for row in jlc_cpl})
        self.assertTrue(all(row["Layer"] in {"Top", "Bottom"} for row in jlc_cpl))
        self.assertEqual(
            ["Designator", "Quantity", "Manufacturer Part Number", "Procurement Type", "Customer Note"],
            next_fields,
        )
        self.assertEqual(
            self.mounted_refs,
            {ref for row in next_bom for ref in split_refs(row["Designator"])},
        )
        self.assertEqual(self.mounted_refs, {row["Ref"] for row in next_pos})

        canonical = {row["Ref"]: row for row in canonical_cpl}
        jlc = {row["Designator"]: row for row in jlc_cpl}
        nxt = {row["Ref"]: row for row in next_pos}
        for ref in self.mounted_refs:
            self.assertEqual(canonical[ref]["PosX"], jlc[ref]["Mid X"])
            self.assertEqual(canonical[ref]["PosY"], jlc[ref]["Mid Y"])
            self.assertEqual(canonical[ref]["Rot"], jlc[ref]["Rotation"])
            self.assertAlmostEqual(float(canonical[ref]["PosX"]), float(nxt[ref]["PosX"]), places=4)
            self.assertAlmostEqual(float(canonical[ref]["PosY"]), float(nxt[ref]["PosY"]), places=4)
            self.assertAlmostEqual(float(canonical[ref]["Rot"]), float(nxt[ref]["Rot"]), places=4)

    def test_nextpcb_centroid_zip_contains_only_native_position_file(self):
        position = MFG / "nextpcb" / f"{NAME}-PCBA-Top.pos"
        archive_path = MFG / "nextpcb" / f"{NAME}-PCBA-Centroid-NextPCB.zip"
        with ZipFile(archive_path) as archive:
            self.assertEqual([position.name], archive.namelist())
            self.assertEqual(position.read_bytes(), archive.read(position.name))

    def test_manifest_covers_and_authenticates_every_manufacturing_file(self):
        manifest = MFG / "MANIFEST.sha256"
        entries = {}
        for line in manifest.read_text(encoding="ascii").splitlines():
            digest, relative = line.split("  ", maxsplit=1)
            entries[relative] = digest
        expected = {
            path.relative_to(MFG).as_posix()
            for path in MFG.rglob("*")
            if path.is_file() and path != manifest
        }
        self.assertEqual(expected, set(entries))
        for relative, expected_digest in entries.items():
            actual = hashlib.sha256((MFG / relative).read_bytes()).hexdigest()
            self.assertEqual(expected_digest, actual, relative)

    def test_phase3h_contains_no_vehicle_command_or_bmw_can_identifier(self):
        text_files = [
            ROOT / "docs" / "phase3h-bidirectional-kcan.md",
            HW / "README.md",
            HW / "netlist_PHASE3H.csv",
            HW / "pin-review.csv",
        ]
        combined = "\n".join(path.read_text(encoding="utf-8") for path in text_files)
        self.assertNotRegex(combined, r"(?i)twai_(?:node_)?transmit\s*\(")
        self.assertNotRegex(combined, r"(?i)CAN\s+ID\s*[:=]\s*0x[0-9a-f]+")
        for forbidden in ("CAS command", "DDE command", "KL50 command", "EWS bypass"):
            self.assertNotIn(forbidden, combined)


if __name__ == "__main__":
    unittest.main()
