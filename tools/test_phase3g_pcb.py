import csv
import hashlib
import re
import unittest
from pathlib import Path
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[1]
HW = ROOT / "hardware" / "kcan-rxonly-pcb"
KICAD = HW / "kicad"
MFG = HW / "manufacturing"
BOARD = KICAD / "BMW-E9x-KCAN-RXOnly.kicad_pcb"
SCHEMATIC = KICAD / "BMW-E9x-KCAN-RXOnly.kicad_sch"
U3_FOOTPRINT = (
    KICAD
    / "footprints"
    / "Phase3G.pretty"
    / "Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_NoViaInPad.kicad_mod"
)


def read_csv(path: Path) -> list[dict[str, str]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        return list(csv.DictReader(handle))


def read_csv_with_fields(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames is None:
            raise AssertionError(f"CSV without a header: {path}")
        return list(reader.fieldnames), list(reader)


def split_designators(value: str) -> list[str]:
    return [designator for designator in re.split(r"[,\s]+", value.strip()) if designator]


def sexpr_blocks(text: str, prefix: str) -> list[str]:
    """Extract balanced KiCad s-expression blocks beginning with prefix."""
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
    matches = [block for block in sexpr_blocks(board_text, "(footprint") if marker in block]
    if len(matches) != 1:
        raise AssertionError(f"Expected one footprint {reference}, found {len(matches)}")
    return matches[0]


def pad_has_net(fp: str, pad: str, net: str) -> bool:
    return any(f'"{net}"' in block for block in sexpr_blocks(fp, f'(pad "{pad}"'))


class Phase3GPcbTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.board = BOARD.read_text(encoding="utf-8")
        cls.schematic = SCHEMATIC.read_text(encoding="utf-8")
        cls.bom = read_csv(HW / "BOM_PHASE3G.csv")
        cls.procurement = read_csv(HW / "procurement_PHASE3G.csv")
        cls.netlist = read_csv(HW / "netlist_PHASE3G.csv")
        cls.pin_review = read_csv(HW / "pin-review.csv")

    def test_required_native_and_manufacturing_artifacts_exist(self):
        required = (
            KICAD / "BMW-E9x-KCAN-RXOnly.kicad_pro",
            SCHEMATIC,
            BOARD,
            KICAD / "phase3g-erc.rpt",
            KICAD / "phase3g-drc-final.rpt",
            MFG / "BMW-E9x-KCAN-RXOnly-fabrication.zip",
            MFG / "bom" / "BMW-E9x-KCAN-RXOnly-PCBA-BOM.csv",
            MFG / "bom" / "BMW-E9x-KCAN-RXOnly-PCBA-BOM-JLCPCB.csv",
            MFG / "cpl" / "BMW-E9x-KCAN-RXOnly-PCBA-CPL.csv",
            MFG / "cpl" / "BMW-E9x-KCAN-RXOnly-PCBA-CPL-JLCPCB.csv",
            MFG / "drawings" / "BMW-E9x-KCAN-RXOnly-schematic.pdf",
            MFG / "drawings" / "BMW-E9x-KCAN-RXOnly-assembly.pdf",
            MFG / "drawings" / "BMW-E9x-KCAN-RXOnly-fabrication.pdf",
            MFG / "3d" / "BMW-E9x-KCAN-RXOnly.step",
            MFG / "images" / "BMW-E9x-KCAN-RXOnly-front.png",
            MFG / "images" / "BMW-E9x-KCAN-RXOnly-back.png",
        )
        for path in required:
            self.assertTrue(path.is_file(), path)
            self.assertGreater(path.stat().st_size, 100, path)

    def test_erc_and_drc_are_clean(self):
        erc = (KICAD / "phase3g-erc.rpt").read_text(encoding="utf-8")
        drc = (KICAD / "phase3g-drc-final.rpt").read_text(encoding="utf-8")
        self.assertIn("ERC messages: 0", erc)
        self.assertIn("Found 0 DRC violations", drc)
        self.assertIn("Found 0 unconnected pads", drc)

    def test_exact_transceiver_buffer_and_regulator(self):
        by_ref = {row["reference"]: row for row in self.bom}
        self.assertEqual("TJA1055T/3/2Z", by_ref["U1"]["mpn"])
        self.assertEqual("CLVC1G125QDBVRQ1", by_ref["U2"]["mpn"])
        self.assertEqual("TLS715B0EJV50XUMA1", by_ref["U3"]["mpn"])
        self.assertIn("PG-DSO-8-52", by_ref["U3"]["package"])
        self.assertIn("TLS715B0EJV50XUMA1", self.schematic)
        self.assertIn("Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_NoViaInPad", self.board)

    def test_regulator_pinout_and_exposed_pad_are_unchanged_electrically(self):
        u3 = footprint(self.board, "U3")
        expected = {"1": "BAT_PROTECTED", "2": "BAT_PROTECTED", "3": "GND", "8": "5V_TJA", "9": "GND"}
        for pad, net in expected.items():
            self.assertTrue(pad_has_net(u3, pad, net), f"U3.{pad} -> {net}")
        footprint_text = U3_FOOTPRINT.read_text(encoding="utf-8")
        exposed_pads = sexpr_blocks(footprint_text, '(pad "9"')
        self.assertEqual(1, len(exposed_pads))
        self.assertIn('(pad "9" smd rect', exposed_pads[0])
        self.assertIn('(size 2.65 3)', exposed_pads[0])
        self.assertIn('(layers "F.Cu" "F.Mask")', exposed_pads[0])
        self.assertNotIn("thru_hole", exposed_pads[0])
        self.assertNotIn("(drill ", exposed_pads[0])

    def test_phase3g1_has_four_standard_tented_peripheral_u3_vias(self):
        vias = sexpr_blocks(self.board, "(via\n")
        expected_positions = ("57.2 70", "58.8 70", "57.2 74", "58.8 74")
        matched = []
        for position in expected_positions:
            matches = [via for via in vias if f"(at {position})" in via]
            self.assertEqual(1, len(matches), position)
            via = matches[0]
            self.assertIn("(size 0.6)", via)
            self.assertIn("(drill 0.3)", via)
            self.assertIn('(layers "F.Cu" "B.Cu")', via)
            self.assertIn("(front yes)", via)
            self.assertIn("(back yes)", via)
            self.assertIn('(net "GND")', via)
            matched.append(via)
        self.assertEqual(4, len(matched))

    def test_phase3g1_has_no_sub_030_mm_via_drill_or_via_in_pad(self):
        via_drills = []
        for via in sexpr_blocks(self.board, "(via\n"):
            match = re.search(r"\(drill ([0-9.]+)\)", via)
            self.assertIsNotNone(match, via)
            via_drills.append(float(match.group(1)))
        self.assertGreater(len(via_drills), 4)
        self.assertGreaterEqual(min(via_drills), 0.30)
        self.assertNotIn("(drill 0.2)", self.board)
        footprint_text = U3_FOOTPRINT.read_text(encoding="utf-8")
        self.assertNotIn("thru_hole", footprint_text)
        self.assertFalse(
            (U3_FOOTPRINT.parent / "Infineon_PG-DSO-8-52_3.9x4.9mm_EP2.65x3mm_ThermalVias.kicad_mod").exists()
        )

    def test_phase3g1_u3_stencil_matches_infineon_segmented_pattern(self):
        footprint_text = U3_FOOTPRINT.read_text(encoding="utf-8")
        paste_pads = [
            pad
            for pad in sexpr_blocks(footprint_text, '(pad ""')
            if '(layers "F.Paste")' in pad
        ]
        self.assertEqual(4, len(paste_pads))
        expected_centers = ("-0.665 -0.75", "-0.665 0.75", "0.665 -0.75", "0.665 0.75")
        for center in expected_centers:
            matches = [pad for pad in paste_pads if f"(at {center})" in pad]
            self.assertEqual(1, len(matches), center)
            self.assertIn("(size 1.125 1.3)", matches[0])
        printed_area = 4 * 1.125 * 1.3
        exposed_pad_area = 2.65 * 3.0
        self.assertAlmostEqual(73.5849, 100 * printed_area / exposed_pad_area, places=4)

    def test_devkit_5v_is_physically_unconnected(self):
        h1 = footprint(self.board, "H1")
        pin_21 = sexpr_blocks(h1, '(pad "21"')[0]
        self.assertNotIn("(net ", pin_21)
        row = next(row for row in self.pin_review if row["reference"] == "H1" and row["pin"] == "21")
        self.assertEqual("5V", row["function"])
        self.assertEqual("NC", row["net"])
        self.assertEqual("PASS", row["status"])

    def test_tx_physical_gap_has_no_link_footprint_or_common_net(self):
        self.assertNotIn('(property "Reference" "R_LINK_TX"', self.board)
        self.assertEqual(2, self.board.count('(name "TX_PHYSICAL_GAP_KEEP_OUT")'))
        self.assertEqual(2, self.board.count("(keepout\n\t\t\t(tracks not_allowed)"))
        self.assertTrue(pad_has_net(footprint(self.board, "U2"), "4", "TX_GATE_OUT"))
        self.assertTrue(pad_has_net(footprint(self.board, "U1"), "2", "TXD_SAFE"))
        self.assertTrue(pad_has_net(footprint(self.board, "TP5"), "1", "TX_GATE_OUT"))
        self.assertTrue(pad_has_net(footprint(self.board, "TP6"), "1", "TXD_SAFE"))
        self.assertFalse(any(row["assembly"] == "MOUNT" and row["reference"] == "R_LINK_TX" for row in self.bom))

    def test_power_domain_has_no_downstream_fuse_or_reverse_diode(self):
        self.assertNotIn('"F_5V"', self.board)
        self.assertNotIn('"D_REG_REV"', self.board)
        self.assertTrue(pad_has_net(footprint(self.board, "U3"), "8", "5V_TJA"))
        self.assertTrue(pad_has_net(footprint(self.board, "U1"), "10", "5V_TJA"))
        self.assertFalse(any(row["reference"] in {"F_5V", "D_REG_REV"} for row in self.bom))

    def test_board_stackup_and_rules_are_conservative(self):
        stats = (MFG / "reports" / "board-statistics.txt").read_text(encoding="utf-8")
        for expected in (
            "Largeur: 120,0000 mm",
            "Hauteur: 80,0000 mm",
            "Isolation minimum de piste: 0,2016 mm",
            "Largeur minimum de piste: 0,2000 mm",
            "Diamètre de perçage min: 0,3000 mm",
            "Epaisseur du PCB: 1,6000 mm",
        ):
            self.assertIn(expected, stats)
        self.assertIn("(general\n\t\t(thickness 1.6)", self.board)

    def test_pin_review_is_complete_and_all_pass(self):
        self.assertGreaterEqual(len(self.pin_review), 90)
        self.assertTrue(all(row["status"] == "PASS" for row in self.pin_review))
        u3_ep = next(row for row in self.pin_review if row["reference"] == "U3" and row["pin"] == "EP")
        self.assertIn("PG-DSO-8-52", u3_ep["notes"])

    def test_pcba_bom_and_cpl_exclude_non_assembled_items(self):
        pcba_bom = read_csv(MFG / "bom" / "BMW-E9x-KCAN-RXOnly-PCBA-BOM.csv")
        cpl = read_csv(MFG / "cpl" / "BMW-E9x-KCAN-RXOnly-PCBA-CPL.csv")
        self.assertTrue(all(row["assembly"] == "MOUNT" for row in pcba_bom))
        pcba_refs = {ref for row in pcba_bom for ref in row["reference"].split()}
        cpl_refs = {row["Ref"] for row in cpl}
        self.assertEqual(pcba_refs, cpl_refs)
        self.assertNotIn("DEV1", pcba_refs)
        self.assertNotIn("R_LINK_TX", pcba_refs)
        self.assertFalse(any(ref.startswith("TP") for ref in pcba_refs))

    def test_jlcpcb_bom_and_cpl_match_canonical_assembly_data(self):
        canonical_bom = read_csv(MFG / "bom" / "BMW-E9x-KCAN-RXOnly-PCBA-BOM.csv")
        canonical_cpl = read_csv(MFG / "cpl" / "BMW-E9x-KCAN-RXOnly-PCBA-CPL.csv")
        jlcpcb_bom_fields, jlcpcb_bom = read_csv_with_fields(
            MFG / "bom" / "BMW-E9x-KCAN-RXOnly-PCBA-BOM-JLCPCB.csv"
        )
        jlcpcb_cpl_fields, jlcpcb_cpl = read_csv_with_fields(
            MFG / "cpl" / "BMW-E9x-KCAN-RXOnly-PCBA-CPL-JLCPCB.csv"
        )

        self.assertEqual(
            ["Comment", "Designator", "Footprint", "LCSC Part #"],
            jlcpcb_bom_fields,
        )
        self.assertEqual(
            ["Designator", "Mid X", "Mid Y", "Layer", "Rotation"],
            jlcpcb_cpl_fields,
        )

        expected_refs = {ref for row in canonical_bom for ref in row["reference"].split()}
        bom_refs = {ref for row in jlcpcb_bom for ref in split_designators(row["Designator"])}
        cpl_refs = [row["Designator"] for row in jlcpcb_cpl]
        self.assertEqual(expected_refs, bom_refs)
        self.assertEqual(expected_refs, set(cpl_refs))
        self.assertEqual(len(expected_refs), len(cpl_refs))
        self.assertEqual(len(cpl_refs), len(set(cpl_refs)))

        forbidden = {"DEV1", "R_LINK_TX", "SH_RX_MODE"}
        self.assertFalse(expected_refs & forbidden)
        self.assertFalse(any(ref.startswith("TP") or ref.startswith("MH") for ref in expected_refs))

        canonical_by_ref = {row["Ref"]: row for row in canonical_cpl}
        for row in jlcpcb_cpl:
            canonical = canonical_by_ref[row["Designator"]]
            self.assertEqual(canonical["PosX"], row["Mid X"])
            self.assertEqual(canonical["PosY"], row["Mid Y"])
            self.assertEqual(canonical["Rot"], row["Rotation"])
            self.assertEqual("Top" if canonical["Side"] == "top" else "Bottom", row["Layer"])
            self.assertIn(row["Layer"], {"Top", "Bottom"})

        canonical_bom_by_designators = {
            frozenset(row["reference"].split()): row for row in canonical_bom
        }
        for row in jlcpcb_bom:
            designators = split_designators(row["Designator"])
            canonical = canonical_bom_by_designators[frozenset(designators)]
            self.assertEqual(canonical["mpn"], row["Comment"])
            self.assertEqual(canonical["package"], row["Footprint"])
            self.assertEqual("", row["LCSC Part #"])
            if len(designators) > 1:
                self.assertIn(",", row["Designator"])

    def test_phase3g_procurement_excludes_perfboard_adapters(self):
        text = (HW / "procurement_PHASE3G.csv").read_text(encoding="utf-8")
        for obsolete in ("PA0003C", "PA0086C", "PR2H1-D", "Keystone 5001"):
            self.assertNotIn(obsolete, text)
        critical = {row["mpn"] for row in self.procurement}
        self.assertTrue({"TJA1055T/3/2Z", "CLVC1G125QDBVRQ1", "TLS715B0EJV50XUMA1"}.issubset(critical))

    def test_fabrication_zip_contains_only_review_release_inputs(self):
        zip_path = MFG / "BMW-E9x-KCAN-RXOnly-fabrication.zip"
        with ZipFile(zip_path) as archive:
            names = set(archive.namelist())
        self.assertIn("fabrication-notes.txt", names)
        self.assertTrue(any(name.endswith("F_Cu.gtl") for name in names))
        self.assertTrue(any(name.endswith("B_Cu.gbl") for name in names))
        self.assertTrue(any(name.endswith("Edge_Cuts.gm1") for name in names))
        self.assertTrue(any(name.endswith("PTH.drl") for name in names))
        self.assertTrue(any(name.endswith("NPTH.drl") for name in names))
        self.assertFalse(any(name.endswith((".kicad_pcb", ".kicad_sch", ".ses", ".dsn")) for name in names))

    def test_manifest_hashes_match_all_manufacturing_files(self):
        lines = (MFG / "MANIFEST.sha256").read_text(encoding="ascii").splitlines()
        self.assertGreater(len(lines), 20)
        for line in lines:
            digest, relative = line.split("  ", 1)
            path = MFG / Path(relative)
            self.assertTrue(path.is_file(), path)
            self.assertEqual(digest, hashlib.sha256(path.read_bytes()).hexdigest(), relative)

    def test_phase3g1_fabrication_notes_request_only_standard_vias(self):
        notes = (MFG / "fabrication-notes.txt").read_text(encoding="utf-8")
        self.assertIn("0.60 mm pad / 0.30 mm drill", notes)
        self.assertIn("no drilled hole and no via-in-pad", notes)
        self.assertIn("tented on both sides", notes)
        self.assertIn("Do not add epoxy fill", notes)
        self.assertNotIn("0.20 mm drill", notes)

    def test_phase3g1_thermal_envelope_is_documented_conservatively(self):
        decision = (ROOT / "docs" / "phase3g1-dfm-cost-optimization.md").read_text(
            encoding="utf-8"
        )
        for required in (
            "Pmax = 0,263836 W",
            "RthJA_requise <= (150 - 85) / 0,263836 = 246,37 K/W",
            "deltaT = 0,263836 x 153 = 40,37 °C",
            "Tj = 85 + 40,37 = 125,37 °C",
            "marge = 150 - 125,37 = 24,63 °C",
            "459,429 mm²",
            "73,58 %",
            "pas `RELEASED FOR ORDER`",
        ):
            self.assertIn(required, decision)

    def test_gerber_review_renders_and_drill_maps_exist(self):
        for name in (
            "gerber-front-copper.png",
            "gerber-back-copper.png",
            "gerber-front-silkscreen.png",
            "gerber-front-mask.png",
            "gerber-outline.png",
        ):
            path = MFG / "review" / name
            self.assertTrue(path.is_file(), path)
            self.assertGreater(path.stat().st_size, 1000, path)
        maps = list((MFG / "drill").glob("*-drl_map.pdf"))
        self.assertEqual(2, len(maps))

    def test_phase3g_files_contain_no_active_twai_transmission_api(self):
        inspected = "\n".join(
            path.read_text(encoding="utf-8", errors="replace")
            for path in [HW / "README.md", HW / "power-budget.md", ROOT / "tools" / "generate_phase3g_kicad.py"]
        )
        self.assertNotRegex(inspected, r"\btwai_(?:node_)?transmit\s*\(")
        self.assertNotIn("TWAI_MODE_NORMAL", inspected)


if __name__ == "__main__":
    unittest.main()
