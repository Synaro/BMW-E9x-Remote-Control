"""Build an observational engine-state timeline from a generic RPM CSV.

Thresholds are mandatory mapping inputs and remain explicitly candidate-only.
This host tool neither decodes BMW traffic nor controls a vehicle.
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

from vehicle_data_validation import (
    ValidationError,
    load_json_object,
    validate_engine_run_timeline,
    validate_evidence_index,
)


def _require_mapping(mapping: dict[str, Any], evidence_ids: set[str]) -> None:
    required = {"schema_version", "mapping_id", "example_only", "input", "columns", "timeline", "candidate_thresholds"}
    missing = sorted(required - set(mapping))
    if missing:
        raise ValidationError(f"engine mapping: missing required fields: {', '.join(missing)}")
    if mapping["schema_version"] != 1:
        raise ValidationError("engine mapping.schema_version: expected 1")
    input_config = mapping["input"]
    if input_config.get("delimiter") not in {",", ";", "\\t", "|"}:
        raise ValidationError("engine mapping.input.delimiter: unsupported delimiter")
    if input_config.get("encoding") not in {"utf-8", "utf-8-sig"}:
        raise ValidationError("engine mapping.input.encoding: unsupported encoding")
    if input_config.get("timestamp_unit") not in {"s", "ms", "us"}:
        raise ValidationError("engine mapping.input.timestamp_unit: unsupported unit")
    columns = mapping["columns"]
    if set(columns) != {"timestamp", "rpm"} or not all(isinstance(value, str) and value for value in columns.values()):
        raise ValidationError("engine mapping.columns: timestamp and rpm columns are required")
    timeline = mapping["timeline"]
    timeline_required = {"timeline_id", "profile_ref", "evidence_index", "evidence_ref", "source"}
    if not isinstance(timeline, dict) or timeline_required - set(timeline):
        raise ValidationError("engine mapping.timeline: incomplete timeline metadata")
    if timeline["evidence_ref"] not in evidence_ids:
        raise ValidationError("engine mapping.timeline.evidence_ref: unknown evidence")
    thresholds = mapping["candidate_thresholds"]
    if not isinstance(thresholds, dict) or thresholds.get("candidate_only") is not True:
        raise ValidationError("engine mapping.candidate_thresholds: candidate_only must be true")


def _timestamp_to_us(value: str, unit: str, location: str) -> int:
    try:
        numeric = float(value.strip().replace(",", "."))
    except ValueError as error:
        raise ValidationError(f"{location}: invalid timestamp") from error
    if numeric < 0:
        raise ValidationError(f"{location}: timestamp cannot be negative")
    multiplier = {"s": 1_000_000, "ms": 1_000, "us": 1}[unit]
    return round(numeric * multiplier)


def _rpm(value: str, location: str) -> float:
    try:
        numeric = float(value.strip().replace(",", "."))
    except ValueError as error:
        raise ValidationError(f"{location}: invalid rpm") from error
    if numeric < 0:
        raise ValidationError(f"{location}: rpm cannot be negative")
    return numeric


def build_timeline(
    input_path: str | Path,
    mapping: dict[str, Any],
    evidence_ids: set[str],
) -> dict[str, Any]:
    _require_mapping(mapping, evidence_ids)
    input_config = mapping["input"]
    delimiter = "\t" if input_config["delimiter"] == "\\t" else input_config["delimiter"]
    columns = mapping["columns"]
    thresholds = mapping["candidate_thresholds"]
    source_path = Path(input_path)
    try:
        stream = source_path.open("r", encoding=input_config["encoding"], newline="")
    except OSError as error:
        raise ValidationError(f"{source_path}: cannot open input: {error}") from error

    parsed: list[tuple[int, float, int]] = []
    with stream:
        reader = csv.DictReader(stream, delimiter=delimiter)
        if reader.fieldnames is None:
            raise ValidationError(f"{source_path}: header row is required")
        missing = sorted(set(columns.values()) - set(reader.fieldnames))
        if missing:
            raise ValidationError(f"{source_path}: missing mapped columns: {', '.join(missing)}")
        for row_number, row in enumerate(reader, start=2):
            parsed.append(
                (
                    _timestamp_to_us(row[columns["timestamp"]], input_config["timestamp_unit"], f"{source_path}:{row_number}"),
                    _rpm(row[columns["rpm"]], f"{source_path}:{row_number}"),
                    row_number,
                )
            )
    if not parsed:
        raise ValidationError(f"{source_path}: no engine-speed samples")
    origin = parsed[0][0]

    samples: list[dict[str, Any]] = []
    running = False
    in_band_count = 0
    previous_state: str | None = None
    previous_timestamp = -1
    for sequence, (absolute_timestamp, rpm, row_number) in enumerate(parsed):
        timestamp_us = absolute_timestamp - origin
        if timestamp_us < previous_timestamp:
            raise ValidationError(f"{source_path}:{row_number}: timestamps must be monotonic")
        previous_timestamp = timestamp_us

        if rpm <= thresholds["stopped_max_rpm"]:
            state = "STOPPED"
            running = False
            in_band_count = 0
        elif running:
            if rpm < thresholds["running_exit_rpm"]:
                state = "UNKNOWN"
                running = False
                in_band_count = 0
            else:
                state = "RUNNING"
        else:
            state = "CRANKING"
            if thresholds["running_band_min_rpm"] <= rpm <= thresholds["running_band_max_rpm"]:
                in_band_count += 1
            else:
                in_band_count = 0
            if in_band_count >= thresholds["consecutive_samples"]:
                state = "RUNNING"
                running = True

        transition = None
        if previous_state is not None and previous_state != state:
            transition = {
                ("STOPPED", "CRANKING"): "STOPPED_TO_CRANKING",
                ("CRANKING", "RUNNING"): "CRANKING_TO_RUNNING",
                ("RUNNING", "STOPPED"): "RUNNING_TO_STOPPED",
            }.get((previous_state, state), "TO_UNKNOWN")
        samples.append(
            {
                "sequence": sequence,
                "timestamp_us": timestamp_us,
                "rpm": rpm,
                "observed_state": state,
                "transition": transition,
                "source_row": row_number,
                "notes": "Candidate classification; not validated for vehicle control.",
            }
        )
        previous_state = state

    timeline_config = mapping["timeline"]
    timeline = {
        "schema_version": 1,
        "timeline_id": timeline_config["timeline_id"],
        "example_only": mapping["example_only"],
        "profile_ref": timeline_config["profile_ref"],
        "evidence_index": timeline_config["evidence_index"],
        "evidence_ref": timeline_config["evidence_ref"],
        "source": timeline_config["source"],
        "timestamp_basis": "SESSION_RELATIVE",
        "candidate_thresholds": thresholds,
        "qualification": "OBSERVED",
        "samples": samples,
    }
    validate_engine_run_timeline(timeline, evidence_ids)
    return timeline


def _parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Import a generic engine-speed CSV into an observational timeline")
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--mapping", required=True, type=Path)
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    arguments = _parse_arguments()
    evidence = load_json_object(arguments.evidence)
    evidence_ids = validate_evidence_index(evidence, evidence_directory=arguments.evidence.parent)
    timeline = build_timeline(arguments.input, load_json_object(arguments.mapping), evidence_ids)
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(timeline, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"Imported {len(timeline['samples'])} RPM samples to {arguments.output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValidationError as error:
        print(f"Engine-speed import rejected: {error}")
        raise SystemExit(2) from error
