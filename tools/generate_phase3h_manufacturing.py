#!/usr/bin/env python3
"""Create deterministic Phase 3H assembly and fabrication metadata.

KiCad produces the copper, drill, drawing, position, image, and STEP files.
This helper filters the reviewed Phase 3H BOM/placement data, builds the
manufacturer ZIP, and records SHA-256 hashes for the complete review package.
It never talks to a manufacturer and does not alter the electrical design.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo


ROOT = Path(__file__).resolve().parents[1]
HW = ROOT / "hardware" / "kcan-bidirectional-pcb"
MANUFACTURING = HW / "manufacturing"

FABRICATION_NOTES = """BMW E9x K-CAN full bidirectional Phase 3H - REVIEW ONLY / DO NOT ORDER

Board: 120.0 x 80.0 mm, two layers, FR-4, 1.6 mm, 1 oz copper.
Surface finish for quote: lead-free HASL or ENIG; no electrical substitution.
Minimum routed track: 0.20 mm. Minimum clearance: 0.20 mm.
All vias, including the four U3 peripheral thermal vias: 0.60 mm pad / 0.30 mm drill.
U3 exposed pad: 2.65 x 3.00 mm, no drilled hole and no via-in-pad.
U3 peripheral vias are tented on both sides. Do not add epoxy fill, copper cap,
special via plating, or any sub-0.30 mm drill.
Mounting holes: four 3.20 mm NPTH.

Critical electrical feature:
The TWAI TX path is continuous: ESP32 GPIO5 -> R5 -> U2 A/B -> TJA1055 TXD.
There is no TX jumper, solder bridge, physical gap, or assembly option.
TP_TX_MCU and TP_TXD are measurement points and do not interrupt either net.

Exact critical MPNs are listed in the PCBA BOM. No silent substitution.
ESP32-S3-DEVKITC-1-N8R8 is user installed. GPIO6 controls STB through R10;
GPIO15 controls EN through R12. Listen-only is a firmware mode, not a PCB limit.
This package is for manufacturing review only and is not a purchase release.
"""


def read_csv(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames is None:
            raise ValueError(f"CSV without a header: {path}")
        return list(reader.fieldnames), list(reader)


def mounted_references(bom_rows: list[dict[str, str]]) -> set[str]:
    refs: set[str] = set()
    for row in bom_rows:
        if row["assembly"] != "MOUNT":
            continue
        refs.update(row["reference"].split())
    return refs


def write_csv(
    path: Path,
    fieldnames: list[str],
    rows: list[dict[str, str]],
    *,
    preserve_equivalent: bool = False,
) -> None:
    if preserve_equivalent and path.is_file():
        existing_fields, existing_rows = read_csv(path)
        if existing_fields == fieldnames and existing_rows == rows:
            return
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def build_pcba_files() -> None:
    bom_fields, bom_rows = read_csv(HW / "BOM_PHASE3H.csv")
    mounted_rows = [row for row in bom_rows if row["assembly"] == "MOUNT"]
    canonical_bom = MANUFACTURING / "bom" / "BMW-E9x-KCAN-Bidirectional-PCBA-BOM.csv"
    write_csv(
        canonical_bom,
        bom_fields,
        mounted_rows,
        preserve_equivalent=True,
    )

    jlcpcb_bom_rows = [
        {
            "Comment": row["mpn"],
            "Designator": ", ".join(row["reference"].split()),
            "Footprint": row["package"],
            "LCSC Part #": "",
        }
        for row in mounted_rows
    ]
    write_csv(
        MANUFACTURING / "bom" / "BMW-E9x-KCAN-Bidirectional-PCBA-BOM-JLCPCB.csv",
        ["Comment", "Designator", "Footprint", "LCSC Part #"],
        jlcpcb_bom_rows,
    )

    position_path = MANUFACTURING / "assembly" / "BMW-E9x-KCAN-Bidirectional-all-pos.csv"
    position_fields, position_rows = read_csv(position_path)
    ref_column = "Ref" if "Ref" in position_fields else "RefDes"
    expected_refs = mounted_references(bom_rows)
    filtered = [row for row in position_rows if row[ref_column] in expected_refs]
    actual_refs = {row[ref_column] for row in filtered}
    missing = expected_refs - actual_refs
    if missing:
        raise ValueError(f"Mounted BOM references missing from placement data: {sorted(missing)}")
    canonical_cpl = MANUFACTURING / "cpl" / "BMW-E9x-KCAN-Bidirectional-PCBA-CPL.csv"
    write_csv(
        canonical_cpl,
        position_fields,
        filtered,
        preserve_equivalent=True,
    )

    jlcpcb_cpl_rows: list[dict[str, str]] = []
    for row in filtered:
        side = row["Side"]
        if side not in {"top", "bottom"}:
            raise ValueError(f"Unsupported placement side for {row[ref_column]}: {side}")
        jlcpcb_cpl_rows.append(
            {
                "Designator": row[ref_column],
                "Mid X": row["PosX"],
                "Mid Y": row["PosY"],
                "Layer": "Top" if side == "top" else "Bottom",
                "Rotation": row["Rot"],
            }
        )
    write_csv(
        MANUFACTURING / "cpl" / "BMW-E9x-KCAN-Bidirectional-PCBA-CPL-JLCPCB.csv",
        ["Designator", "Mid X", "Mid Y", "Layer", "Rotation"],
        jlcpcb_cpl_rows,
    )


def write_fabrication_notes() -> None:
    (MANUFACTURING / "fabrication-notes.txt").write_text(
        FABRICATION_NOTES, encoding="utf-8"
    )


def zip_info(relative: Path) -> ZipInfo:
    info = ZipInfo(relative.as_posix(), date_time=(2026, 10, 3, 0, 0, 0))
    info.compress_type = ZIP_DEFLATED
    info.external_attr = 0o100644 << 16
    return info


def build_fabrication_zip() -> None:
    zip_path = MANUFACTURING / "BMW-E9x-KCAN-Bidirectional-fabrication.zip"
    inputs: list[tuple[Path, Path]] = []
    for directory in ("gerbers", "drill"):
        for path in sorted((MANUFACTURING / directory).glob("*")):
            if path.is_file():
                inputs.append((path, path.relative_to(MANUFACTURING)))
    notes = MANUFACTURING / "fabrication-notes.txt"
    inputs.append((notes, notes.relative_to(MANUFACTURING)))
    with ZipFile(zip_path, "w") as archive:
        for source, relative in inputs:
            archive.writestr(zip_info(relative), source.read_bytes())


def write_manifest() -> None:
    manifest_path = MANUFACTURING / "MANIFEST.sha256"
    entries: list[str] = []
    for path in sorted(MANUFACTURING.rglob("*")):
        if not path.is_file() or path == manifest_path:
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        entries.append(f"{digest}  {path.relative_to(MANUFACTURING).as_posix()}")
    manifest_path.write_text("\n".join(entries) + "\n", encoding="ascii")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--finalize", action="store_true")
    args = parser.parse_args()
    build_pcba_files()
    write_fabrication_notes()
    if args.finalize:
        build_fabrication_zip()
        write_manifest()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
