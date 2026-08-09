"""Strict Phase 3C validation for vehicle profiles and OEM observations.

The module deliberately uses only the Python standard library. It validates
cross-file provenance and semantic invariants that JSON Schema cannot express
without application-specific code. It contains no vehicle commands, CAN IDs,
or BMW signal decoding.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from datetime import datetime
from pathlib import Path, PurePosixPath, PureWindowsPath
from typing import Any, Iterable, Mapping


CONFIDENCE_LEVELS = {"UNKNOWN", "LOW", "MEDIUM", "HIGH", "VALIDATED"}
EVIDENCE_KINDS = {
    "ISTA_CAPTURE",
    "TESTO_EXPORT",
    "INPA_OUTPUT",
    "TOOL32_OUTPUT",
    "FUTURE_CAN_CAPTURE",
    "MANUAL_NOTE",
}
CHECKLIST_STATUSES = {"UNKNOWN", "OBSERVED", "CONFIRMED", "VALIDATED", "BLOCKED"}
PHASES = {
    "UNKNOWN",
    "PRECHECK",
    "AUTHORIZATION",
    "KL15_ACTIVE",
    "CRANK_REQUESTED",
    "CRANKING",
    "ENGINE_RUNNING",
    "ABORTED",
    "SHUTDOWN",
}
SIGNALS = {
    "KL15",
    "KL50",
    "ENGINE_SPEED",
    "ENGINE_STATE",
    "TRANSMISSION_POSITION",
    "BRAKE",
    "KEY_STATE",
    "START_AUTHORIZATION",
    "BATTERY_VOLTAGE",
}
REQUIRED_CHECKLIST_ITEMS = {
    "exact_cas_identification",
    "exact_dde_identification",
    "oem_start_architecture",
    "kl15",
    "kl50",
    "engine_speed",
    "engine_running_state",
    "transmission_pn",
    "brake",
    "oem_start_authorization",
    "battery_voltage",
    "timeout_strategy",
    "stop_strategy",
    "communication_loss",
    "post_reset_behavior",
    "start_inhibit_conditions",
}


class ValidationError(ValueError):
    """Raised when a Phase 3C artifact violates its contract."""


def load_json_object(path: str | Path) -> dict[str, Any]:
    source = Path(path)
    try:
        value = json.loads(source.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ValidationError(f"{source}: invalid JSON: {error}") from error
    if not isinstance(value, dict):
        raise ValidationError(f"{source}: root must be an object")
    return value


def _require_object(value: Any, location: str) -> Mapping[str, Any]:
    if not isinstance(value, dict):
        raise ValidationError(f"{location}: must be an object")
    return value


def _require_array(value: Any, location: str) -> list[Any]:
    if not isinstance(value, list):
        raise ValidationError(f"{location}: must be an array")
    return value


def _required(document: Mapping[str, Any], fields: Iterable[str], location: str) -> None:
    missing = sorted(set(fields) - set(document))
    if missing:
        raise ValidationError(f"{location}: missing required fields: {', '.join(missing)}")


def _require_schema_v1(document: Mapping[str, Any], location: str) -> None:
    if document.get("schema_version") != 1:
        raise ValidationError(f"{location}.schema_version: expected 1")


def _require_nonempty_string(value: Any, location: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValidationError(f"{location}: must be a non-empty string")
    return value


def _require_timestamp(value: Any, location: str) -> None:
    text = _require_nonempty_string(value, location)
    try:
        parsed = datetime.fromisoformat(text.replace("Z", "+00:00"))
    except ValueError as error:
        raise ValidationError(f"{location}: invalid ISO-8601 timestamp") from error
    if parsed.tzinfo is None:
        raise ValidationError(f"{location}: timezone is required")


def _validate_evidence_ref(reference: Any, evidence_ids: set[str], location: str) -> None:
    value = _require_nonempty_string(reference, location)
    if value not in evidence_ids:
        raise ValidationError(f"{location}: unknown evidence reference {value!r}")


def validate_evidence_index(
    document: Mapping[str, Any], *, evidence_directory: str | Path | None = None
) -> set[str]:
    _required(document, {"schema_version", "index_id", "example_only", "entries"}, "evidence")
    _require_schema_v1(document, "evidence")
    _require_nonempty_string(document["index_id"], "evidence.index_id")
    if not isinstance(document["example_only"], bool):
        raise ValidationError("evidence.example_only: must be boolean")

    evidence_ids: set[str] = set()
    base = Path(evidence_directory).resolve() if evidence_directory is not None else None
    for index, raw_entry in enumerate(_require_array(document["entries"], "evidence.entries")):
        location = f"evidence.entries[{index}]"
        entry = _require_object(raw_entry, location)
        _required(
            entry,
            {
                "evidence_id",
                "kind",
                "relative_path",
                "captured_at",
                "sha256",
                "contains_personal_data",
                "description",
            },
            location,
        )
        evidence_id = _require_nonempty_string(entry["evidence_id"], f"{location}.evidence_id")
        if evidence_id in evidence_ids:
            raise ValidationError(f"{location}.evidence_id: duplicate {evidence_id!r}")
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,95}", evidence_id):
            raise ValidationError(f"{location}.evidence_id: invalid stable identifier")
        evidence_ids.add(evidence_id)

        if entry["kind"] not in EVIDENCE_KINDS:
            raise ValidationError(f"{location}.kind: unsupported evidence kind")
        relative_text = _require_nonempty_string(entry["relative_path"], f"{location}.relative_path")
        posix_path = PurePosixPath(relative_text.replace("\\", "/"))
        if PureWindowsPath(relative_text).is_absolute() or posix_path.is_absolute() or ".." in posix_path.parts:
            raise ValidationError(f"{location}.relative_path: must stay inside the evidence directory")
        _require_timestamp(entry["captured_at"], f"{location}.captured_at")
        digest = entry["sha256"]
        if digest is not None and (not isinstance(digest, str) or re.fullmatch(r"[0-9a-f]{64}", digest) is None):
            raise ValidationError(f"{location}.sha256: expected lowercase SHA-256 or null")
        if not isinstance(entry["contains_personal_data"], bool):
            raise ValidationError(f"{location}.contains_personal_data: must be boolean")
        _require_nonempty_string(entry["description"], f"{location}.description")
        if base is not None:
            evidence_path = (base / Path(*posix_path.parts)).resolve()
            if not evidence_path.is_relative_to(base):
                raise ValidationError(f"{location}.relative_path: resolved path escapes evidence directory")
            if not evidence_path.is_file():
                raise ValidationError(f"{location}.relative_path: referenced evidence file does not exist")
            if digest is not None:
                hasher = hashlib.sha256()
                try:
                    with evidence_path.open("rb") as stream:
                        for chunk in iter(lambda: stream.read(65536), b""):
                            hasher.update(chunk)
                except OSError as error:
                    raise ValidationError(f"{location}.relative_path: cannot read evidence file") from error
                if hasher.hexdigest() != digest:
                    raise ValidationError(f"{location}.sha256: digest does not match evidence file")
    return evidence_ids


def _validate_assertion(value: Any, evidence_ids: set[str], location: str, *, vin: bool = False) -> None:
    assertion = _require_object(value, location)
    required = {"value", "observed_at", "confidence", "evidence_refs"}
    if vin:
        required.add("anonymized")
    _required(assertion, required, location)
    confidence = assertion["confidence"]
    if confidence not in CONFIDENCE_LEVELS:
        raise ValidationError(f"{location}.confidence: unsupported level")
    references = _require_array(assertion["evidence_refs"], f"{location}.evidence_refs")
    if len(references) != len(set(references)):
        raise ValidationError(f"{location}.evidence_refs: duplicate reference")
    for index, reference in enumerate(references):
        _validate_evidence_ref(reference, evidence_ids, f"{location}.evidence_refs[{index}]")

    if confidence == "UNKNOWN":
        if assertion["value"] is not None or assertion["observed_at"] is not None or references:
            raise ValidationError(f"{location}: UNKNOWN must not claim a value, date, or evidence")
    else:
        if assertion["value"] is None or (isinstance(assertion["value"], str) and not assertion["value"].strip()):
            raise ValidationError(f"{location}.value: observed data requires a value")
        _require_timestamp(assertion["observed_at"], f"{location}.observed_at")
        if not references:
            raise ValidationError(f"{location}.evidence_refs: observed data requires provenance")
    if vin and not isinstance(assertion["anonymized"], bool):
        raise ValidationError(f"{location}.anonymized: must be boolean")


def validate_vehicle_profile(document: Mapping[str, Any], evidence_ids: set[str]) -> None:
    _required(
        document,
        {
            "schema_version",
            "profile_id",
            "example_only",
            "recorded_at",
            "evidence_index",
            "vehicle",
            "cas",
            "dde",
            "other_control_units",
        },
        "profile",
    )
    _require_schema_v1(document, "profile")
    _require_nonempty_string(document["profile_id"], "profile.profile_id")
    _require_timestamp(document["recorded_at"], "profile.recorded_at")
    _require_nonempty_string(document["evidence_index"], "profile.evidence_index")
    if not isinstance(document["example_only"], bool):
        raise ValidationError("profile.example_only: must be boolean")

    vehicle = _require_object(document["vehicle"], "profile.vehicle")
    vehicle_fields = {"vin", "model", "chassis", "production_date", "engine", "transmission"}
    _required(vehicle, vehicle_fields, "profile.vehicle")
    for field in sorted(vehicle_fields):
        _validate_assertion(vehicle[field], evidence_ids, f"profile.vehicle.{field}", vin=field == "vin")

    for ecu_name in ("cas", "dde"):
        ecu = _require_object(document[ecu_name], f"profile.{ecu_name}")
        ecu_fields = {"type_version", "hardware_reference", "software_reference", "zb_number"}
        _required(ecu, ecu_fields, f"profile.{ecu_name}")
        for field in sorted(ecu_fields):
            _validate_assertion(ecu[field], evidence_ids, f"profile.{ecu_name}.{field}")

    roles: set[str] = set()
    for index, raw_ecu in enumerate(_require_array(document["other_control_units"], "profile.other_control_units")):
        location = f"profile.other_control_units[{index}]"
        ecu = _require_object(raw_ecu, location)
        fields = {"role", "type_version", "hardware_reference", "software_reference", "zb_number"}
        _required(ecu, fields, location)
        role = _require_nonempty_string(ecu["role"], f"{location}.role")
        if role in roles:
            raise ValidationError(f"{location}.role: duplicate role {role!r}")
        roles.add(role)
        for field in sorted(fields - {"role"}):
            _validate_assertion(ecu[field], evidence_ids, f"{location}.{field}")


def _validate_record_semantics(record: Mapping[str, Any], location: str) -> None:
    signal = record["signal"]
    interpreted = record["interpreted_value"]
    unit = record["unit"]
    if signal in {"KL15", "KL50", "BRAKE"}:
        if not isinstance(interpreted, bool) or unit is not None:
            raise ValidationError(f"{location}: {signal} requires a boolean interpreted value and null unit")
    elif signal == "ENGINE_SPEED":
        if isinstance(interpreted, bool) or not isinstance(interpreted, (int, float)) or unit != "rpm":
            raise ValidationError(f"{location}: ENGINE_SPEED requires a numeric value in rpm")
        if interpreted < 0:
            raise ValidationError(f"{location}: ENGINE_SPEED cannot be negative")
    elif signal == "BATTERY_VOLTAGE":
        if isinstance(interpreted, bool) or not isinstance(interpreted, (int, float)) or unit != "V":
            raise ValidationError(f"{location}: BATTERY_VOLTAGE requires a numeric value in V")
        if interpreted < 0:
            raise ValidationError(f"{location}: BATTERY_VOLTAGE cannot be negative")
    elif signal == "ENGINE_STATE":
        if interpreted not in {"UNKNOWN", "STOPPED", "CRANKING", "RUNNING"} or unit is not None:
            raise ValidationError(f"{location}: invalid ENGINE_STATE")
    elif signal == "TRANSMISSION_POSITION":
        if interpreted not in {"UNKNOWN", "P", "N", "OTHER"} or unit is not None:
            raise ValidationError(f"{location}: invalid TRANSMISSION_POSITION")
    elif unit is not None:
        raise ValidationError(f"{location}: state-like signals require a null unit")


def validate_observation_session(document: Mapping[str, Any], evidence_ids: set[str]) -> None:
    _required(
        document,
        {"schema_version", "session_id", "example_only", "profile_ref", "evidence_index", "started_at", "records"},
        "observation",
    )
    _require_schema_v1(document, "observation")
    _require_nonempty_string(document["session_id"], "observation.session_id")
    _require_nonempty_string(document["profile_ref"], "observation.profile_ref")
    _require_nonempty_string(document["evidence_index"], "observation.evidence_index")
    _require_timestamp(document["started_at"], "observation.started_at")
    if not isinstance(document["example_only"], bool):
        raise ValidationError("observation.example_only: must be boolean")

    previous_timestamp = -1
    for index, raw_record in enumerate(_require_array(document["records"], "observation.records")):
        location = f"observation.records[{index}]"
        record = _require_object(raw_record, location)
        _required(
            record,
            {
                "sequence",
                "timestamp_us",
                "phase",
                "signal",
                "raw_value",
                "interpreted_value",
                "unit",
                "source",
                "confidence",
            },
            location,
        )
        if record["sequence"] != index:
            raise ValidationError(f"{location}.sequence: expected {index}")
        timestamp = record["timestamp_us"]
        if isinstance(timestamp, bool) or not isinstance(timestamp, int) or timestamp < 0:
            raise ValidationError(f"{location}.timestamp_us: expected non-negative integer")
        if timestamp < previous_timestamp:
            raise ValidationError(f"{location}.timestamp_us: timestamps must be monotonic")
        previous_timestamp = timestamp
        if record["phase"] not in PHASES:
            raise ValidationError(f"{location}.phase: unsupported phase")
        if record["signal"] not in SIGNALS:
            raise ValidationError(f"{location}.signal: unsupported signal")
        if record["raw_value"] is None and record["interpreted_value"] is None:
            raise ValidationError(f"{location}: raw and interpreted values cannot both be null")
        if record["confidence"] not in CONFIDENCE_LEVELS:
            raise ValidationError(f"{location}.confidence: unsupported level")
        source = _require_object(record["source"], f"{location}.source")
        _required(source, {"evidence_ref", "locator"}, f"{location}.source")
        _validate_evidence_ref(source["evidence_ref"], evidence_ids, f"{location}.source.evidence_ref")
        _require_nonempty_string(source["locator"], f"{location}.source.locator")
        _validate_record_semantics(record, location)


def validate_prerequisites(document: Mapping[str, Any], evidence_ids: set[str]) -> None:
    _required(
        document,
        {"schema_version", "checklist_id", "example_only", "profile_ref", "evidence_index", "updated_at", "items"},
        "checklist",
    )
    _require_schema_v1(document, "checklist")
    _require_nonempty_string(document["checklist_id"], "checklist.checklist_id")
    _require_nonempty_string(document["profile_ref"], "checklist.profile_ref")
    _require_nonempty_string(document["evidence_index"], "checklist.evidence_index")
    _require_timestamp(document["updated_at"], "checklist.updated_at")
    if not isinstance(document["example_only"], bool):
        raise ValidationError("checklist.example_only: must be boolean")

    items = _require_object(document["items"], "checklist.items")
    missing = REQUIRED_CHECKLIST_ITEMS - set(items)
    extra = set(items) - REQUIRED_CHECKLIST_ITEMS
    if missing or extra:
        raise ValidationError(
            "checklist.items: exact item set required; "
            f"missing={sorted(missing)}, extra={sorted(extra)}"
        )
    for name in sorted(REQUIRED_CHECKLIST_ITEMS):
        location = f"checklist.items.{name}"
        item = _require_object(items[name], location)
        _required(item, {"status", "evidence_refs", "updated_at", "notes", "blocker_reason"}, location)
        status = item["status"]
        if status not in CHECKLIST_STATUSES:
            raise ValidationError(f"{location}.status: unsupported status")
        references = _require_array(item["evidence_refs"], f"{location}.evidence_refs")
        if len(references) != len(set(references)):
            raise ValidationError(f"{location}.evidence_refs: duplicate reference")
        for index, reference in enumerate(references):
            _validate_evidence_ref(reference, evidence_ids, f"{location}.evidence_refs[{index}]")
        _require_timestamp(item["updated_at"], f"{location}.updated_at")
        if status in {"OBSERVED", "CONFIRMED", "VALIDATED"} and not references:
            raise ValidationError(f"{location}: {status} requires evidence")
        if status == "UNKNOWN" and references:
            raise ValidationError(f"{location}: UNKNOWN cannot claim evidence")
        blocker = item["blocker_reason"]
        if status == "BLOCKED":
            _require_nonempty_string(blocker, f"{location}.blocker_reason")
        elif blocker is not None:
            raise ValidationError(f"{location}.blocker_reason: only valid for BLOCKED")


def validate_import_mapping(document: Mapping[str, Any], evidence_ids: set[str]) -> None:
    _required(
        document,
        {"schema_version", "mapping_id", "example_only", "input", "columns", "session", "evidence_ref"},
        "mapping",
    )
    _require_schema_v1(document, "mapping")
    _require_nonempty_string(document["mapping_id"], "mapping.mapping_id")
    if not isinstance(document["example_only"], bool):
        raise ValidationError("mapping.example_only: must be boolean")
    input_config = _require_object(document["input"], "mapping.input")
    _required(input_config, {"format", "delimiter", "encoding", "has_header"}, "mapping.input")
    if input_config["format"] != "delimited":
        raise ValidationError("mapping.input.format: only delimited is supported")
    if input_config["delimiter"] not in {",", ";", "\\t", "|"}:
        raise ValidationError("mapping.input.delimiter: unsupported delimiter")
    if input_config["encoding"] not in {"utf-8", "utf-8-sig"}:
        raise ValidationError("mapping.input.encoding: unsupported encoding")
    if input_config["has_header"] is not True:
        raise ValidationError("mapping.input.has_header: headers are mandatory")

    columns = _require_object(document["columns"], "mapping.columns")
    required_columns = {
        "timestamp_us",
        "phase",
        "signal",
        "raw_value",
        "interpreted_value",
        "unit",
        "confidence",
        "source_locator",
    }
    if set(columns) != required_columns:
        raise ValidationError("mapping.columns: exact canonical field set required")
    source_columns: list[str] = []
    for canonical_name in sorted(required_columns):
        source_columns.append(_require_nonempty_string(columns[canonical_name], f"mapping.columns.{canonical_name}"))
    if len(source_columns) != len(set(source_columns)):
        raise ValidationError("mapping.columns: a source column cannot map to multiple canonical fields")

    session = _require_object(document["session"], "mapping.session")
    _required(session, {"session_id", "profile_ref", "evidence_index", "started_at", "notes"}, "mapping.session")
    for field in ("session_id", "profile_ref", "evidence_index"):
        _require_nonempty_string(session[field], f"mapping.session.{field}")
    _require_timestamp(session["started_at"], "mapping.session.started_at")
    _validate_evidence_ref(document["evidence_ref"], evidence_ids, "mapping.evidence_ref")


def _parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate Phase 3C vehicle-data artifacts")
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--observation", type=Path)
    parser.add_argument("--checklist", type=Path)
    parser.add_argument("--mapping", type=Path)
    return parser.parse_args()


def main() -> int:
    arguments = _parse_arguments()
    evidence = load_json_object(arguments.evidence)
    evidence_ids = validate_evidence_index(evidence, evidence_directory=arguments.evidence.parent)
    validators = (
        (arguments.profile, validate_vehicle_profile),
        (arguments.observation, validate_observation_session),
        (arguments.checklist, validate_prerequisites),
        (arguments.mapping, validate_import_mapping),
    )
    for path, validator in validators:
        if path is not None:
            validator(load_json_object(path), evidence_ids)
    print("Phase 3C vehicle data: VALID")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValidationError as error:
        print(f"Phase 3C vehicle data: INVALID: {error}")
        raise SystemExit(2) from error
