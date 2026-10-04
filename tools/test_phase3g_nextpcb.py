import csv
import unittest
from pathlib import Path
from zipfile import ZipFile


ROOT = Path(__file__).resolve().parents[1]
HW = ROOT / "hardware" / "kcan-rxonly-pcb"
MFG = HW / "manufacturing"
NEXTPCB = MFG / "nextpcb"
BOM_PATH = NEXTPCB / "BMW-E9x-KCAN-RXOnly-PCBA-BOM-NextPCB.csv"
POSITION_PATH = NEXTPCB / "BMW-E9x-KCAN-RXOnly-PCBA-Top.pos"
ZIP_PATH = NEXTPCB / "BMW-E9x-KCAN-RXOnly-PCBA-Centroid-NextPCB.zip"
EXPECTED_BOM_FIELDS = [
    "Designator",
    "Quantity",
    "Manufacturer Part Number",
    "Procurement Type",
    "Customer Note",
]


def read_csv_with_fields(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames is None:
            raise AssertionError(f"CSV without a header: {path}")
        return list(reader.fieldnames), list(reader)


def split_source_references(value: str) -> list[str]:
    return [reference for reference in value.split() if reference]


def split_nextpcb_designators(value: str) -> list[str]:
    return [reference.strip() for reference in value.split(",") if reference.strip()]


def read_position_file(path: Path) -> list[dict[str, str]]:
    placements = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        columns = line.split()
        if len(columns) != 7:
            raise AssertionError(f"Unexpected native KiCad position row: {line}")
        placements.append(
            dict(zip(("Ref", "Val", "Package", "PosX", "PosY", "Rot", "Side"), columns))
        )
    return placements


class Phase3G1NextPcbExportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        _, cls.source_bom = read_csv_with_fields(HW / "BOM_PHASE3G.csv")
        cls.mounted_rows = [row for row in cls.source_bom if row["assembly"] == "MOUNT"]
        cls.expected_refs = {
            reference
            for row in cls.mounted_rows
            for reference in split_source_references(row["reference"])
        }
        cls.bom_fields, cls.nextpcb_bom = read_csv_with_fields(BOM_PATH)
        cls.placements = read_position_file(POSITION_PATH)

    def test_nextpcb_files_and_bom_header_are_exact(self):
        for path in (BOM_PATH, POSITION_PATH, ZIP_PATH):
            self.assertTrue(path.is_file(), path)
            self.assertGreater(path.stat().st_size, 100, path)
        self.assertEqual(EXPECTED_BOM_FIELDS, self.bom_fields)

    def test_bom_uses_per_board_quantities_exact_mpns_and_standard_sourcing(self):
        source_by_refs = {
            frozenset(split_source_references(row["reference"])): row
            for row in self.mounted_rows
        }
        actual_refs = set()
        for row in self.nextpcb_bom:
            designators = split_nextpcb_designators(row["Designator"])
            designator_set = frozenset(designators)
            self.assertIn(designator_set, source_by_refs)
            source = source_by_refs[designator_set]
            self.assertEqual(len(designators), int(row["Quantity"]))
            self.assertEqual(int(source["quantity"]), int(row["Quantity"]))
            self.assertEqual(source["mpn"], row["Manufacturer Part Number"])
            self.assertEqual("", row["Procurement Type"])
            self.assertIn("no substitution", row["Customer Note"].lower())
            self.assertFalse(actual_refs.intersection(designators))
            actual_refs.update(designators)
        self.assertEqual(self.expected_refs, actual_refs)

    def test_critical_devices_and_devkit_headers_keep_exact_mpns(self):
        by_ref = {}
        for row in self.nextpcb_bom:
            for reference in split_nextpcb_designators(row["Designator"]):
                by_ref[reference] = row["Manufacturer Part Number"]
        self.assertEqual("TJA1055T/3/2Z", by_ref["U1"])
        self.assertEqual("CLVC1G125QDBVRQ1", by_ref["U2"])
        self.assertEqual("TLS715B0EJV50XUMA1", by_ref["U3"])
        self.assertEqual("SSQ-122-03-G-S", by_ref["H1"])
        self.assertEqual("SSQ-122-03-G-S", by_ref["H2"])

    def test_bom_and_centroid_have_the_same_unique_mounted_population(self):
        placement_refs = [placement["Ref"] for placement in self.placements]
        self.assertEqual(len(placement_refs), len(set(placement_refs)))
        self.assertEqual(self.expected_refs, set(placement_refs))

        excluded_refs = {
            reference
            for row in self.source_bom
            if row["assembly"] != "MOUNT"
            for reference in split_source_references(row["reference"])
        }
        self.assertFalse(set(placement_refs).intersection(excluded_refs))
        self.assertNotIn("C", {row["Procurement Type"] for row in self.nextpcb_bom})
        self.assertNotIn("DNP", {row["Procurement Type"] for row in self.nextpcb_bom})

    def test_centroid_is_top_only_and_matches_canonical_coordinates_and_rotations(self):
        _, canonical_rows = read_csv_with_fields(
            MFG / "assembly" / "BMW-E9x-KCAN-RXOnly-all-pos.csv"
        )
        canonical = {row["Ref"]: row for row in canonical_rows}
        self.assertEqual(self.expected_refs, set(canonical))
        for placement in self.placements:
            reference = placement["Ref"]
            self.assertEqual("top", placement["Side"])
            self.assertEqual("top", canonical[reference]["Side"])
            self.assertAlmostEqual(float(canonical[reference]["PosX"]), float(placement["PosX"]), places=6)
            self.assertAlmostEqual(float(canonical[reference]["PosY"]), float(placement["PosY"]), places=6)
            self.assertAlmostEqual(float(canonical[reference]["Rot"]), float(placement["Rot"]), places=6)

    def test_centroid_zip_contains_only_the_unchanged_native_position_file(self):
        with ZipFile(ZIP_PATH) as archive:
            self.assertEqual([POSITION_PATH.name], archive.namelist())
            self.assertEqual(POSITION_PATH.read_bytes(), archive.read(POSITION_PATH.name))


if __name__ == "__main__":
    unittest.main()
