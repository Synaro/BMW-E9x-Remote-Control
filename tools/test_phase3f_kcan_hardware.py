import csv
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
HW = ROOT / "hardware" / "kcan-rxonly"
BOM_PATH = HW / "BOM.csv"
NETLIST_PATH = HW / "netlist.csv"
WIRING_PATH = HW / "wiring.md"
DESIGN_PATH = ROOT / "docs" / "phase3f-kcan-rxonly-design-freeze.md"


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

    def test_required_artifacts_are_nonempty(self):
        for path in (BOM_PATH, NETLIST_PATH, WIRING_PATH, DESIGN_PATH):
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
        self.assertEqual("ESP32-S3-DEVKITC-1-N8", by_ref["DEV1"]["mpn"])
        self.assertIn("ISO 11898-2", by_ref["U1"]["forbidden_substitution"])

    def test_tx_link_is_physically_absent_and_not_ordered(self):
        link = next(row for row in self.bom if row["reference"] == "R_LINK_TX")
        self.assertEqual("0", link["quantity_to_buy"])
        self.assertEqual("0", link["quantity_used"])
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
        combined = self.design + self.wiring
        self.assertIn("Ne pas raccorder à la BMW", combined)
        self.assertIn("TWAI_MODE_LISTEN_ONLY", self.design)
        self.assertIn("aucune Phase 4", self.design)
        self.assertIn("aucun ID BMW", self.design)
        self.assertIn("R_LINK_TX", combined)


if __name__ == "__main__":
    unittest.main()
