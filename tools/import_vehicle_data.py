"""Convert generic delimited observations to the Phase 3C canonical JSON form."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

from vehicle_data_validation import (
    ValidationError,
    load_json_object,
    validate_evidence_index,
    validate_import_mapping,
    validate_observation_session,
)


def _parse_interpreted_value(text: str) -> Any:
    value = text.strip()
    if not value:
        return None
    lowered = value.lower()
    if lowered == "true":
        return True
    if lowered == "false":
        return False
    try:
        return int(value, 10)
    except ValueError:
        try:
            return float(value)
        except ValueError:
            return value


def import_delimited(
    input_path: str | Path,
    mapping: dict[str, Any],
    evidence_ids: set[str],
) -> dict[str, Any]:
    validate_import_mapping(mapping, evidence_ids)
    input_config = mapping["input"]
    delimiter = "\t" if input_config["delimiter"] == "\\t" else input_config["delimiter"]
    columns = mapping["columns"]
    source = Path(input_path)

    records: list[dict[str, Any]] = []
    try:
        stream = source.open("r", encoding=input_config["encoding"], newline="")
    except OSError as error:
        raise ValidationError(f"{source}: cannot open input: {error}") from error
    with stream:
        reader = csv.DictReader(stream, delimiter=delimiter)
        if reader.fieldnames is None:
            raise ValidationError(f"{source}: header row is required")
        missing_columns = sorted(set(columns.values()) - set(reader.fieldnames))
        if missing_columns:
            raise ValidationError(f"{source}: missing mapped columns: {', '.join(missing_columns)}")
        for row_number, row in enumerate(reader, start=2):
            try:
                timestamp_us = int(row[columns["timestamp_us"]], 10)
            except (TypeError, ValueError) as error:
                raise ValidationError(f"{source}:{row_number}: timestamp_us must be an integer") from error
            raw_text = row[columns["raw_value"]]
            locator = row[columns["source_locator"]]
            records.append(
                {
                    "sequence": len(records),
                    "timestamp_us": timestamp_us,
                    "phase": row[columns["phase"]].strip(),
                    "signal": row[columns["signal"]].strip(),
                    "raw_value": raw_text if raw_text != "" else None,
                    "interpreted_value": _parse_interpreted_value(row[columns["interpreted_value"]]),
                    "unit": row[columns["unit"]].strip() or None,
                    "source": {
                        "evidence_ref": mapping["evidence_ref"],
                        "locator": locator.strip(),
                    },
                    "confidence": row[columns["confidence"]].strip(),
                }
            )

    session_config = mapping["session"]
    session = {
        "schema_version": 1,
        "session_id": session_config["session_id"],
        "example_only": mapping["example_only"],
        "profile_ref": session_config["profile_ref"],
        "evidence_index": session_config["evidence_index"],
        "started_at": session_config["started_at"],
        "timestamp_basis": session_config["timestamp_basis"],
        "notes": session_config["notes"],
        "records": records,
    }
    validate_observation_session(session, evidence_ids)
    return session


def _parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Import generic Phase 3C tabular observations")
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--mapping", required=True, type=Path)
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    arguments = _parse_arguments()
    evidence_document = load_json_object(arguments.evidence)
    evidence_ids = validate_evidence_index(
        evidence_document, evidence_directory=arguments.evidence.parent
    )
    mapping = load_json_object(arguments.mapping)
    session = import_delimited(arguments.input, mapping, evidence_ids)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(session, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Imported {len(session['records'])} records to {arguments.output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValidationError as error:
        print(f"Import rejected: {error}")
        raise SystemExit(2) from error
