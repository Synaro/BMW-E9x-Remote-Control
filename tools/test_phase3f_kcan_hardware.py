import csv
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HW = ROOT / "hardware" / "kcan-rxonly"
BOM_PATH = HW / "BOM.csv"
NETLIST_PATH = HW / "netlist.csv"
WIRING_PATH = HW / "wiring.md"
DESIGN_PATH = ROOT / "docs" / "phase3f-kcan-rxonly-design-freeze.md"
PROCUREMENT_PATH = HW / "procurement.csv"
PROCUREMENT_AUDIT_PATH = ROOT / "docs" / "phase3f-procurement-audit.md"


def read_csv(path: Path):
    with path.open(newline="", encoding="utf-8") as handle:
        return list(csv.DictReader(handle))


class Phase3FKcanHardwareTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.bom = read_csv(BOM_PATH)
        cls.netlist = read_csv(NETLIST_PATH)
        cls.wiring = WIRING_PATH.read_text(encoding="utf-8")
        cls.design = DESIGN_PATH.read_text(encoding="utf-8")
        cls.procurement = read_csv(PROCUREMENT_PATH)
        cls.procurement_audit = PROCUREMENT_AUDIT_PATH.read_text(encoding="utf-8")

    def test_required_artifacts_are_nonempty(self):
        for path in (
            BOM_PATH,
            NETLIST_PATH,
            WIRING_PATH,
            DESIGN_PATH,
            PROCUREMENT_PATH,
            PROCUREMENT_AUDIT_PATH,
        ):
            self.assertTrue(path.is_file(), path)
            self.assertGreater(path.stat().st_size, 100, path)

    def test_bom_has_required_columns_and_unique_references(self):
        required = {
            "category",
            "reference",
            "designation",
            "manufacturer",
            "mpn",
            "quantity_to_buy",
            "quantity_used",
            "package",
            "tolerance",
            "power_rating",
            "voltage_rating",
            "function",
            "acceptable_alternative",
            "forbidden_substitution",
            "purchase_stage",
            "estimated_unit_eur",
            "assembly",
        }
        self.assertEqual(required, set(self.bom[0]))
        refs = [row["reference"] for row in self.bom]
        self.assertEqual(len(refs), len(set(refs)))
        self.assertTrue(all(all(value != "" for value in row.values()) for row in self.bom))

    def test_exact_transceiver_and_buffer_are_frozen(self):
        by_ref = {row["reference"]: row for row in self.bom}
        self.assertEqual("TJA1055T/3/2Z", by_ref["U1"]["mpn"])
        self.assertEqual("CLVC1G125QDBVRQ1", by_ref["U2"]["mpn"])
        self.assertEqual("ESP32-S3-DEVKITC-1-N8R8", by_ref["DEV1"]["mpn"])
        self.assertIn("ISO 11898-2", by_ref["U1"]["forbidden_substitution"])
        self.assertIn("TJA1055T/3/C,518 EOL", by_ref["U1"]["forbidden_substitution"])
        self.assertIn("No form-fit-function substitute", by_ref["U1"]["acceptable_alternative"])

    def test_corrected_packages_and_prototyping_parts_are_frozen(self):
        by_ref = {row["reference"]: row for row in self.bom}
        expected = {
            "C_VCC_100N": "885012207098",
            "C_U2_100N": "885012207098",
            "C_VCC_22U": "LMJ316BB7226MLHT",
            "C_BAT_10N": "885012207092",
            "LEAD_XH": "ASXHSXH22K152",
            "PCB1": "PR2H1-D",
            "ADP_U1": "PA0003C",
            "ADP_U2": "PA0086C",
            "TP_SET": "5001",
        }
        for reference, mpn in expected.items():
            self.assertEqual(mpn, by_ref[reference]["mpn"], reference)

        self.assertEqual("0", by_ref["CRIMP"]["quantity_to_buy"])
        self.assertEqual("DO_NOT_BUY", by_ref["CRIMP"]["purchase_stage"])
        self.assertEqual("DNP", by_ref["CRIMP"]["assembly"])

    def test_tx_link_is_physically_absent_and_not_ordered(self):
        link = next(row for row in self.bom if row["reference"] == "R_LINK_TX")
        self.assertEqual("0", link["quantity_to_buy"])
        self.assertEqual("0", link["quantity_used"])
        self.assertEqual("DO_NOT_BUY", link["purchase_stage"])
        self.assertEqual("DNP", link["assembly"])

        links = [row for row in self.netlist if row["through_ref"] == "R_LINK_TX"]
        self.assertEqual(1, len(links))
        self.assertEqual("DNP", links[0]["through_value"])
        self.assertEqual("DNP", links[0]["assembly"])

    def test_txd_has_only_five_volt_pullup_as_populated_source(self):
        txd_rows = [
            row
            for row in self.netlist
            if row["to_ref"] == "U1" and row["to_pin"] == "2"
        ]
        populated = [row for row in txd_rows if row["assembly"] == "MOUNT"]
        self.assertEqual(1, len(populated), txd_rows)
        self.assertEqual("5V_TJA", populated[0]["from_ref"])
        self.assertEqual("R_TXD", populated[0]["through_ref"])
        self.assertEqual("10k", populated[0]["through_value"])
        self.assertFalse(any(row["from_ref"] in {"DEV1", "3V3", "U2"} for row in populated))

    def test_oe_and_receive_mode_have_no_gpio_control(self):
        oe = [row for row in self.netlist if row["to_ref"] == "U2" and row["to_pin"] == "1"]
        self.assertEqual(1, len(oe))
        self.assertEqual(("3V3", "R_OE", "10k"), (oe[0]["from_ref"], oe[0]["through_ref"], oe[0]["through_value"]))

        mode_rows = [
            row
            for row in self.netlist
            if row["to_ref"] == "U1" and row["to_pin"] in {"5", "6"}
        ]
        self.assertTrue(mode_rows)
        self.assertFalse(any(row["from_ref"] == "DEV1" for row in mode_rows))
        self.assertTrue(any(row["through_ref"] == "JP_RX_MODE" for row in self.netlist))

    def test_all_tja1055_pins_are_accounted_for(self):
        pins = set()
        for row in self.netlist:
            if row["from_ref"] == "U1" and row["from_pin"].isdigit():
                pins.add(int(row["from_pin"]))
            if row["to_ref"] == "U1" and row["to_pin"].isdigit():
                pins.add(int(row["to_pin"]))
        self.assertEqual(set(range(1, 15)), pins)

    def test_termination_is_matched_weak_and_never_cross_bus_120_ohm(self):
        by_ref = {row["reference"]: row for row in self.bom}
        for ref in ("R_RTH", "R_RTL"):
            self.assertEqual("RT0805BRD075K62L", by_ref[ref]["mpn"])
            self.assertEqual("0.1 %", by_ref[ref]["tolerance"])

        forbidden_values = {"120", "120R", "120ohm", "120 Ohm"}
        self.assertFalse(
            any(row["through_value"] in forbidden_values for row in self.netlist)
        )
        self.assertTrue(any(row["through_value"] == "5.62k" for row in self.netlist))
        self.assertIn("aucune résistance de 120", self.wiring)

    def test_wake_and_inh_follow_nxp_recommendations(self):
        wake = [
            row
            for row in self.netlist
            if row["to_ref"] == "U1" and row["to_pin"] == "7"
        ]
        self.assertEqual(1, len(wake))
        self.assertEqual(("U1", "14", "DIRECT"), (wake[0]["from_ref"], wake[0]["from_pin"], wake[0]["through_ref"]))

        inh = [row for row in self.netlist if row["from_ref"] == "U1" and row["from_pin"] == "1"]
        self.assertEqual(1, len(inh))
        self.assertEqual(("OPEN", "DNP"), (inh[0]["through_ref"], inh[0]["assembly"]))

    def test_documents_keep_vehicle_connection_and_phase4_forbidden(self):
        combined = self.design + self.wiring + self.procurement_audit
        self.assertIn("Ne pas raccorder à la BMW", combined)
        self.assertIn("TWAI_MODE_LISTEN_ONLY", self.design)
        self.assertIn("aucune Phase 4", self.design)
        self.assertIn("aucun ID BMW", self.design)
        self.assertIn("R_LINK_TX", combined)

    def test_procurement_has_required_columns_and_traceable_bom_coverage(self):
        required = {
            "category",
            "description",
            "manufacturer",
            "manufacturer_part_number",
            "distributor",
            "distributor_sku",
            "qty_used",
            "qty_to_order",
            "package",
            "estimated_unit_price",
            "estimated_total",
            "buy_stage",
            "source_url",
            "notes",
            "verified_date",
            "bom_references",
            "currency",
            "stock_snapshot",
        }
        self.assertEqual(required, set(self.procurement[0]))
        self.assertTrue(all(row["verified_date"] == "2026-10-01" for row in self.procurement))
        self.assertTrue(all(row["currency"] == "EUR" for row in self.procurement))

        covered = {
            reference
            for row in self.procurement
            for reference in row["bom_references"].split("|")
            if "_PREVIOUS" not in reference
        }
        self.assertEqual({row["reference"] for row in self.bom}, covered)

    def test_procurement_audit_covers_every_bom_reference(self):
        audited = {
            line.split("|")[1].strip()
            for line in self.procurement_audit.splitlines()
            if line.startswith("|") and len(line.split("|")) > 2
        }
        self.assertTrue(
            {row["reference"] for row in self.bom}.issubset(audited),
            "Every engineering BOM reference must have an explicit audit row",
        )

    def test_procurement_critical_orderable_parts_are_exact(self):
        by_ref = {
            reference: row
            for row in self.procurement
            for reference in row["bom_references"].split("|")
            if "_PREVIOUS" not in reference
        }
        expected = {
            "U1": ("TJA1055T/3/2Z", "568-TJA1055T/3/2ZCT-ND", "2"),
            "U2": ("CLVC1G125QDBVRQ1G4", "595-VC1G125QDBVRQ1G4", "3"),
            "DEV1": (
                "ESP32-S3-DEVKITC-1-N8R8",
                "356-EP32S3DVKTC1N8R8",
                "1",
            ),
            "F_BAT": ("MINISMDC010F-2", "MINISMDC010F-2CT-ND", "5"),
            "D_BAT": ("SS16-E3/61T", "SS16-E3/61TGICT-ND", "3"),
            "ADP_U1": ("PA0003C", "315-PA0003C-ND", "2"),
            "ADP_U2": ("PA0086C", "315-PA0086C-ND", "2"),
        }
        for reference, values in expected.items():
            row = by_ref[reference]
            self.assertEqual(values, (
                row["manufacturer_part_number"],
                row["distributor_sku"],
                row["qty_to_order"],
            ), reference)
            self.assertEqual("BUY_NOW", row["buy_stage"])
            self.assertTrue(row["source_url"].startswith("https://"))

    def test_procurement_totals_and_stages_are_stable(self):
        totals = {}
        for row in self.procurement:
            expected_total = round(
                float(row["estimated_unit_price"]) * int(row["qty_to_order"]),
                2,
            )
            self.assertAlmostEqual(expected_total, float(row["estimated_total"]), places=2)
            totals[row["buy_stage"]] = totals.get(row["buy_stage"], 0.0) + float(
                row["estimated_total"]
            )

        self.assertAlmostEqual(64.58, totals["BUY_NOW"], places=2)
        self.assertAlmostEqual(105.00, totals["BUY_NOW_IF_NOT_OWNED"], places=2)
        self.assertAlmostEqual(611.00, totals["BUY_LATER"], places=2)
        self.assertAlmostEqual(0.00, totals["DO_NOT_BUY"], places=2)

    def test_retired_or_forbidden_parts_cannot_reenter_buy_now(self):
        forbidden = {
            "ESP32-S3-DEVKITC-1-N8",
            "1210",
            "SBB0014A",
            "5015",
            "GRM21BR71H104KA01L",
            "SXH-001T-P0.6",
            "TJA1055T/3/C,518",
        }
        purchased = {
            row["manufacturer_part_number"]
            for row in self.procurement
            if row["buy_stage"] == "BUY_NOW"
        }
        self.assertTrue(forbidden.isdisjoint(purchased))

        do_not_buy = {
            reference: row
            for row in self.procurement
            if row["buy_stage"] == "DO_NOT_BUY"
            for reference in row["bom_references"].split("|")
        }
        for reference in ("R_LINK_TX", "CRIMP", "D_CAN", "L_CAN"):
            self.assertEqual("0", do_not_buy[reference]["qty_to_order"])


if __name__ == "__main__":
    unittest.main()
