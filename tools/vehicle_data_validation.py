"""Strict Phase 3C/3D validation for vehicle profiles and OEM observations.

The module deliberately uses only the Python standard library. It validates
cross-file provenance and semantic invariants that JSON Schema cannot express
without application-specific code. It contains no vehicle commands, CAN IDs,
or runtime BMW decoder.
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
QUALIFICATION_LEVELS = {"UNKNOWN", "OBSERVED", "CONFIRMED", "UNTRUSTED", "BLOCKED"}
CHECKLIST_STATUSES = {
    "UNKNOWN",
    "OBSERVED",
    "OBSERVED_BOUNDED",
    "CONFIRMED",
    "UNTRUSTED",
    "NOT_YET_VALIDATED",
    "PENDING",
    "PENDING_LEVEL3_TRACE",
    "VALIDATED",
    "BLOCKED",
}
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
TIMESTAMP_BASES = {"SESSION_RELATIVE", "SOURCE_ABSOLUTE", "UNAVAILABLE"}
REQUIRED_CHECKLIST_ITEMS = {
    "exact_cas_identification",
    "exact_dde_identification",
    "exact_egs_identification",
    "oem_start_architecture",
    "kl15",
    "kl50",
    "kl50_oem_start_transition",
    "kl50_duration",
    "kl50_rpm_temporal_alignment",
    "engine_speed",
    "engine_running_state",
    "stopped_cranking_running_observations",
    "transmission_pn",
    "transmission_prnd_tool32",
    "brake",
    "engine_running_detection_algorithm",
    "terminal_50_start_request",
    "cas_start_button_request",
    "oem_start_request_source",
    "remote_start_actuation_mechanism",
    "msa_start_correlated_bits",
    "msa_bit_functional_meaning",
    "oem_start_authorization",
    "battery_voltage",
    "timeout_strategy",
    "stop_strategy",
    "communication_loss",
    "post_reset_behavior",
    "start_inhibit_conditions",
    "can_identifiers",
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


def _validate_assertion(
    value: Any,
    evidence_ids: set[str],
    location: str,
    *,
    vin: bool = False,
    require_qualification: bool = False,
) -> None:
    assertion = _require_object(value, location)
    required = {"value", "observed_at", "confidence", "evidence_refs"}
    if vin:
        required.add("anonymized")
    _required(assertion, required, location)
    confidence = assertion["confidence"]
    if confidence not in CONFIDENCE_LEVELS:
        raise ValidationError(f"{location}.confidence: unsupported level")
    references = _require_array(assertion["evidence_refs"], f"{location}.evidence_refs")
    qualification = assertion.get("qualification")
    if qualification is not None and qualification not in QUALIFICATION_LEVELS:
        raise ValidationError(f"{location}.qualification: unsupported level")
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
        if require_qualification and qualification is None:
            raise ValidationError(f"{location}.qualification: real observed data requires qualification")
        if qualification in {"UNKNOWN", "BLOCKED"}:
            raise ValidationError(f"{location}.qualification: incompatible with an observed value")
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
        _validate_assertion(
            vehicle[field],
            evidence_ids,
            f"profile.vehicle.{field}",
            vin=field == "vin",
            require_qualification=not document["example_only"],
        )

    for ecu_name in ("cas", "dde"):
        ecu = _require_object(document[ecu_name], f"profile.{ecu_name}")
        ecu_fields = {"type_version", "hardware_reference", "software_reference", "zb_number"}
        _required(ecu, ecu_fields, f"profile.{ecu_name}")
        for field in sorted(ecu_fields):
            _validate_assertion(
                ecu[field],
                evidence_ids,
                f"profile.{ecu_name}.{field}",
                require_qualification=not document["example_only"],
            )

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
            _validate_assertion(
                ecu[field],
                evidence_ids,
                f"{location}.{field}",
                require_qualification=not document["example_only"],
            )


def is_precondition_candidate(qualification: str, eligibility: str) -> bool:
    """Return true only for reviewed facts that remain future candidates.

    This is a host-side data qualification gate, not vehicle control logic.
    UNTRUSTED and BLOCKED facts are structurally incapable of passing it.
    """

    return qualification == "CONFIRMED" and eligibility == "CANDIDATE"


def validate_qualified_observations(document: Mapping[str, Any], evidence_ids: set[str]) -> None:
    _required(
        document,
        {
            "schema_version",
            "bundle_id",
            "example_only",
            "profile_ref",
            "evidence_index",
            "recorded_at",
            "observations",
        },
        "qualified_observations",
    )
    _require_schema_v1(document, "qualified_observations")
    _require_nonempty_string(document["bundle_id"], "qualified_observations.bundle_id")
    _require_nonempty_string(document["profile_ref"], "qualified_observations.profile_ref")
    _require_nonempty_string(document["evidence_index"], "qualified_observations.evidence_index")
    _require_timestamp(document["recorded_at"], "qualified_observations.recorded_at")
    if not isinstance(document["example_only"], bool):
        raise ValidationError("qualified_observations.example_only: must be boolean")

    identifiers: set[str] = set()
    valid_categories = {
        "IDENTIFICATION",
        "KL15",
        "KL50",
        "TRANSMISSION_POSITION",
        "ACTUAL_GEAR",
        "BRAKE",
        "ENGINE_SPEED",
        "ENGINE_STATE",
        "SOURCE_QUALITY",
    }
    for index, raw_observation in enumerate(
        _require_array(document["observations"], "qualified_observations.observations")
    ):
        location = f"qualified_observations.observations[{index}]"
        observation = _require_object(raw_observation, location)
        _required(
            observation,
            {
                "observation_id",
                "category",
                "qualification",
                "precondition_eligibility",
                "source",
                "raw_value",
                "interpreted_value",
                "unit",
                "notes",
            },
            location,
        )
        identifier = _require_nonempty_string(observation["observation_id"], f"{location}.observation_id")
        if identifier in identifiers:
            raise ValidationError(f"{location}.observation_id: duplicate {identifier!r}")
        identifiers.add(identifier)
        if observation["category"] not in valid_categories:
            raise ValidationError(f"{location}.category: unsupported category")
        qualification = observation["qualification"]
        if qualification not in QUALIFICATION_LEVELS:
            raise ValidationError(f"{location}.qualification: unsupported level")
        eligibility = observation["precondition_eligibility"]
        if eligibility not in {"PROHIBITED", "CANDIDATE"}:
            raise ValidationError(f"{location}.precondition_eligibility: unsupported value")
        if qualification in {"UNKNOWN", "OBSERVED", "UNTRUSTED", "BLOCKED"} and eligibility != "PROHIBITED":
            raise ValidationError(f"{location}: {qualification} data must be PROHIBITED as a precondition")
        if observation["raw_value"] is None and observation["interpreted_value"] is None:
            raise ValidationError(f"{location}: raw and interpreted values cannot both be null")

        source = _require_object(observation["source"], f"{location}.source")
        _required(
            source,
            {"tool", "sgbd_prg", "job", "field", "physical_context", "observed_at", "session", "evidence_ref"},
            f"{location}.source",
        )
        _require_nonempty_string(source["tool"], f"{location}.source.tool")
        for optional_name in ("sgbd_prg", "job"):
            if source[optional_name] is not None:
                _require_nonempty_string(source[optional_name], f"{location}.source.{optional_name}")
        _require_nonempty_string(source["field"], f"{location}.source.field")
        _require_nonempty_string(source["physical_context"], f"{location}.source.physical_context")
        _require_timestamp(source["observed_at"], f"{location}.source.observed_at")
        _require_nonempty_string(source["session"], f"{location}.source.session")
        _validate_evidence_ref(source["evidence_ref"], evidence_ids, f"{location}.source.evidence_ref")

        category = observation["category"]
        interpreted = observation["interpreted_value"]
        unit = observation["unit"]
        if category == "TRANSMISSION_POSITION" and (interpreted not in {"P", "R", "N", "D"} or unit is not None):
            raise ValidationError(f"{location}: invalid transmission position")
        if category == "BRAKE" and (not isinstance(interpreted, bool) or unit is not None):
            raise ValidationError(f"{location}: BRAKE requires boolean interpreted value")
        if category == "ENGINE_SPEED":
            if isinstance(interpreted, bool) or not isinstance(interpreted, (int, float)) or interpreted < 0 or unit != "rpm":
                raise ValidationError(f"{location}: ENGINE_SPEED requires non-negative rpm")
        if category == "ENGINE_STATE" and (interpreted not in {"UNKNOWN", "STOPPED", "CRANKING", "RUNNING"} or unit is not None):
            raise ValidationError(f"{location}: invalid engine state")


def validate_engine_run_timeline(document: Mapping[str, Any], evidence_ids: set[str]) -> None:
    _required(
        document,
        {
            "schema_version",
            "timeline_id",
            "example_only",
            "profile_ref",
            "evidence_index",
            "evidence_ref",
            "source",
            "timestamp_basis",
            "candidate_thresholds",
            "samples",
            "qualification",
        },
        "engine_timeline",
    )
    _require_schema_v1(document, "engine_timeline")
    for field in ("timeline_id", "profile_ref", "evidence_index"):
        _require_nonempty_string(document[field], f"engine_timeline.{field}")
    _validate_evidence_ref(document["evidence_ref"], evidence_ids, "engine_timeline.evidence_ref")
    if document["timestamp_basis"] not in {"SESSION_RELATIVE", "UNAVAILABLE"}:
        raise ValidationError("engine_timeline.timestamp_basis: unsupported basis")
    if document["qualification"] not in {"OBSERVED", "CONFIRMED", "UNTRUSTED", "BLOCKED"}:
        raise ValidationError("engine_timeline.qualification: unsupported level")
    source = _require_object(document["source"], "engine_timeline.source")
    _required(source, {"tool", "sgbd_prg", "job", "session", "observed_at", "notes"}, "engine_timeline.source")
    for field in ("tool", "sgbd_prg", "job", "session"):
        _require_nonempty_string(source[field], f"engine_timeline.source.{field}")
    _require_timestamp(source["observed_at"], "engine_timeline.source.observed_at")

    thresholds = document["candidate_thresholds"]
    if thresholds is not None:
        thresholds = _require_object(thresholds, "engine_timeline.candidate_thresholds")
        _required(
            thresholds,
            {
                "candidate_only",
                "stopped_max_rpm",
                "running_band_min_rpm",
                "running_band_max_rpm",
                "running_exit_rpm",
                "consecutive_samples",
            },
            "engine_timeline.candidate_thresholds",
        )
        if thresholds["candidate_only"] is not True:
            raise ValidationError("engine_timeline.candidate_thresholds: must remain candidate_only")
        numeric = [thresholds[name] for name in ("stopped_max_rpm", "running_exit_rpm", "running_band_min_rpm", "running_band_max_rpm")]
        if any(isinstance(value, bool) or not isinstance(value, (int, float)) or value < 0 for value in numeric):
            raise ValidationError("engine_timeline.candidate_thresholds: values must be non-negative numbers")
        if not (numeric[0] < numeric[1] <= numeric[2] < numeric[3]):
            raise ValidationError("engine_timeline.candidate_thresholds: incoherent hysteresis ordering")
        if isinstance(thresholds["consecutive_samples"], bool) or not isinstance(thresholds["consecutive_samples"], int) or thresholds["consecutive_samples"] < 2:
            raise ValidationError("engine_timeline.candidate_thresholds.consecutive_samples: expected integer >= 2")

    previous_timestamp = -1
    previous_state: str | None = None
    for index, raw_sample in enumerate(_require_array(document["samples"], "engine_timeline.samples")):
        location = f"engine_timeline.samples[{index}]"
        sample = _require_object(raw_sample, location)
        _required(sample, {"sequence", "timestamp_us", "rpm", "observed_state", "transition", "source_row", "notes"}, location)
        if sample["sequence"] != index:
            raise ValidationError(f"{location}.sequence: expected {index}")
        timestamp = sample["timestamp_us"]
        if document["timestamp_basis"] == "UNAVAILABLE":
            if timestamp is not None:
                raise ValidationError(f"{location}.timestamp_us: must be null when unavailable")
        else:
            if isinstance(timestamp, bool) or not isinstance(timestamp, int) or timestamp < previous_timestamp:
                raise ValidationError(f"{location}.timestamp_us: expected monotonic non-negative integer")
            previous_timestamp = timestamp
        rpm = sample["rpm"]
        if isinstance(rpm, bool) or not isinstance(rpm, (int, float)) or rpm < 0:
            raise ValidationError(f"{location}.rpm: expected non-negative number")
        state = sample["observed_state"]
        if state not in {"UNKNOWN", "STOPPED", "CRANKING", "RUNNING"}:
            raise ValidationError(f"{location}.observed_state: unsupported state")
        allowed_transition = None
        if previous_state is not None and previous_state != state:
            allowed_transition = {
                ("STOPPED", "CRANKING"): "STOPPED_TO_CRANKING",
                ("CRANKING", "RUNNING"): "CRANKING_TO_RUNNING",
                ("RUNNING", "STOPPED"): "RUNNING_TO_STOPPED",
            }.get((previous_state, state), "TO_UNKNOWN")
        if sample["transition"] != allowed_transition:
            raise ValidationError(f"{location}.transition: inconsistent with state change")
        previous_state = state


def validate_signal_source_qualifications(document: Mapping[str, Any], evidence_ids: set[str]) -> None:
    _required(
        document,
        {"schema_version", "qualification_set_id", "profile_ref", "evidence_index", "updated_at", "entries"},
        "signal_sources",
    )
    _require_schema_v1(document, "signal_sources")
    for field in ("qualification_set_id", "profile_ref", "evidence_index"):
        _require_nonempty_string(document[field], f"signal_sources.{field}")
    _require_timestamp(document["updated_at"], "signal_sources.updated_at")
    identifiers: set[str] = set()
    suitability_values = {
        "CANDIDATE",
        "READ_ONLY_SIGNAL",
        "UNTRUSTED",
        "NOT_SUITABLE",
        "NOT_SUITABLE_FOR_PN",
        "NOT_FUNCTIONALLY_IDENTIFIED",
        "OBSERVED_NO_TRANSITION",
    }
    dangerous_msa_inferences = {"KL50", "STARTER_REQUEST", "START_AUTHORIZATION", "CAS_START_REQUEST"}
    kl50_forbidden_inferences = {
        "REMOTE_START_COMMAND_MECHANISM",
        "OEM_START_REQUEST_SOURCE",
        "START_AUTHORIZATION",
        "CAN_IDENTIFIER",
        "DIAGNOSTIC_COMMAND",
        "IMMOBILIZER_BYPASS",
    }
    for index, raw_entry in enumerate(_require_array(document["entries"], "signal_sources.entries")):
        location = f"signal_sources.entries[{index}]"
        entry = _require_object(raw_entry, location)
        _required(
            entry,
            {
                "qualification_id",
                "signal",
                "source",
                "qualification",
                "suitability",
                "precondition_eligibility",
                "allowed_interpretations",
                "forbidden_inferences",
                "evidence_refs",
                "notes",
            },
            location,
        )
        identifier = _require_nonempty_string(entry["qualification_id"], f"{location}.qualification_id")
        if identifier in identifiers:
            raise ValidationError(f"{location}.qualification_id: duplicate {identifier!r}")
        identifiers.add(identifier)
        _require_nonempty_string(entry["signal"], f"{location}.signal")
        if entry["qualification"] not in {"OBSERVED", "CONFIRMED", "UNTRUSTED", "BLOCKED"}:
            raise ValidationError(f"{location}.qualification: unsupported level")
        if entry["suitability"] not in suitability_values:
            raise ValidationError(f"{location}.suitability: unsupported value")
        eligibility = entry["precondition_eligibility"]
        if eligibility not in {"PROHIBITED", "CANDIDATE"}:
            raise ValidationError(f"{location}.precondition_eligibility: unsupported value")
        if eligibility == "CANDIDATE" and not (
            entry["qualification"] == "CONFIRMED" and entry["suitability"] == "CANDIDATE"
        ):
            raise ValidationError(f"{location}: only CONFIRMED/CANDIDATE sources may remain candidates")
        if entry["suitability"] != "CANDIDATE" and eligibility != "PROHIBITED":
            raise ValidationError(f"{location}: unsuitable sources must be PROHIBITED")
        if entry["qualification"] == "UNTRUSTED" and entry["suitability"] != "UNTRUSTED":
            raise ValidationError(f"{location}: UNTRUSTED qualification requires UNTRUSTED suitability")

        source = _require_object(entry["source"], f"{location}.source")
        _required(source, {"tool", "sgbd_prg", "job", "field"}, f"{location}.source")
        for field in ("tool", "sgbd_prg", "job", "field"):
            _require_nonempty_string(source[field], f"{location}.source.{field}")
        for list_name in ("allowed_interpretations", "forbidden_inferences", "evidence_refs"):
            values = _require_array(entry[list_name], f"{location}.{list_name}")
            if len(values) != len(set(values)):
                raise ValidationError(f"{location}.{list_name}: duplicate value")
            for value_index, value in enumerate(values):
                _require_nonempty_string(value, f"{location}.{list_name}[{value_index}]")
        if set(entry["allowed_interpretations"]) & set(entry["forbidden_inferences"]):
            raise ValidationError(f"{location}: an interpretation cannot be both allowed and forbidden")
        for reference_index, reference in enumerate(entry["evidence_refs"]):
            _validate_evidence_ref(reference, evidence_ids, f"{location}.evidence_refs[{reference_index}]")
        if not entry["evidence_refs"]:
            raise ValidationError(f"{location}.evidence_refs: provenance is required")
        _require_nonempty_string(entry["notes"], f"{location}.notes")
        if entry["suitability"] == "NOT_FUNCTIONALLY_IDENTIFIED":
            if entry["allowed_interpretations"]:
                raise ValidationError(f"{location}: unidentified signals cannot have allowed functional interpretations")
            if not dangerous_msa_inferences.issubset(set(entry["forbidden_inferences"])):
                raise ValidationError(f"{location}: MSA correlation must forbid functional start inferences")
        if entry["signal"] == "KLEMMENSTATUS.KL50":
            if entry["qualification"] != "CONFIRMED" or entry["suitability"] != "READ_ONLY_SIGNAL":
                raise ValidationError(f"{location}: KL50 observation must remain a confirmed read-only signal")
            if not kl50_forbidden_inferences.issubset(set(entry["forbidden_inferences"])):
                raise ValidationError(f"{location}: KL50 state must not imply an actuation or authorization mechanism")


def _validate_bitfield_value(value: Any, location: str) -> int:
    bitfield = _require_object(value, location)
    _required(bitfield, {"decimal", "hexadecimal", "binary"}, location)
    decimal = bitfield["decimal"]
    if isinstance(decimal, bool) or not isinstance(decimal, int) or decimal < 0:
        raise ValidationError(f"{location}.decimal: expected non-negative integer")
    if bitfield["hexadecimal"] != f"0x{decimal:X}":
        raise ValidationError(f"{location}.hexadecimal: inconsistent representation")
    if bitfield["binary"] != f"0b{decimal:b}":
        raise ValidationError(f"{location}.binary: inconsistent representation")
    return decimal


def _validate_raw_byte(value: Any, location: str) -> int:
    raw = _require_object(value, location)
    _required(raw, {"decimal", "hexadecimal"}, location)
    decimal = raw["decimal"]
    if isinstance(decimal, bool) or not isinstance(decimal, int) or not 0 <= decimal <= 0xFF:
        raise ValidationError(f"{location}.decimal: expected byte value")
    if raw["hexadecimal"] != f"0x{decimal:02X}":
        raise ValidationError(f"{location}.hexadecimal: inconsistent representation")
    return decimal


def validate_cas_kl50_observation(document: Mapping[str, Any], evidence_ids: set[str]) -> None:
    _required(
        document,
        {
            "schema_version",
            "artifact_type",
            "observation_id",
            "profile_ref",
            "evidence_index",
            "evidence_refs",
            "source",
            "signal",
            "observed_off_value",
            "observed_on_value",
            "trace_values",
            "on_value_repeated_consecutively",
            "observed_transition",
            "status",
            "timestamp_basis",
            "duration_us",
            "duration_status",
            "rpm_alignment_status",
            "raw_trace_available_in_repository",
            "forbidden_inferences",
            "notes",
        },
        "cas_kl50_observation",
    )
    _require_schema_v1(document, "cas_kl50_observation")
    if document["artifact_type"] != "CAS_KL50_OBSERVATION":
        raise ValidationError("cas_kl50_observation.artifact_type: unsupported type")
    for field in ("observation_id", "profile_ref", "evidence_index"):
        _require_nonempty_string(document[field], f"cas_kl50_observation.{field}")
    references = _require_array(document["evidence_refs"], "cas_kl50_observation.evidence_refs")
    if len(references) < 2 or len(references) != len(set(references)):
        raise ValidationError("cas_kl50_observation.evidence_refs: two distinct corroborating proofs required")
    for index, reference in enumerate(references):
        _validate_evidence_ref(reference, evidence_ids, f"cas_kl50_observation.evidence_refs[{index}]")

    source = _require_object(document["source"], "cas_kl50_observation.source")
    _required(
        source,
        {"tools", "artifact", "sgbd_prg", "job", "field", "trace_level", "recorded_at", "session"},
        "cas_kl50_observation.source",
    )
    if source["tools"] != ["Tool32", "EDIABAS IFH trace"]:
        raise ValidationError("cas_kl50_observation.source.tools: expected Tool32 and EDIABAS IFH trace")
    expected_source = {
        "artifact": "ifh.trc",
        "sgbd_prg": "CAS.PRG",
        "job": "status_fzg_zustand",
        "field": "KLEMMENSTATUS bits 4-5",
        "trace_level": 1,
    }
    for field, expected in expected_source.items():
        if source[field] != expected:
            raise ValidationError(f"cas_kl50_observation.source.{field}: expected {expected!r}")
    _require_timestamp(source["recorded_at"], "cas_kl50_observation.source.recorded_at")
    _require_nonempty_string(source["session"], "cas_kl50_observation.source.session")
    if document["signal"] != "CAS.KLEMMENSTATUS.KL50":
        raise ValidationError("cas_kl50_observation.signal: unexpected signal")

    off_value = _validate_raw_byte(document["observed_off_value"], "cas_kl50_observation.observed_off_value")
    on_value = _validate_raw_byte(document["observed_on_value"], "cas_kl50_observation.observed_on_value")
    if (off_value, on_value) != (0x45, 0x55):
        raise ValidationError("cas_kl50_observation: expected observed values 0x45 OFF and 0x55 ON")
    if ((off_value >> 4) & 0b11, (on_value >> 4) & 0b11) != (0b00, 0b01):
        raise ValidationError("cas_kl50_observation: KL50 two-bit decode is inconsistent")
    for value in (off_value, on_value):
        if ((value >> 2) & 0b11) != 0b01 or (value & 0b11) != 0b01 or ((value >> 6) & 0b11) != 0b01:
            raise ValidationError("cas_kl50_observation: KL R, KL15 and key-valid fields must remain ON/valid")

    trace_values = [
        _validate_raw_byte(value, f"cas_kl50_observation.trace_values[{index}]")
        for index, value in enumerate(_require_array(document["trace_values"], "cas_kl50_observation.trace_values"))
    ]
    if trace_values != [64, 65, 85, 69]:
        raise ValidationError("cas_kl50_observation.trace_values: expected reported Level 1 sequence")
    if document["on_value_repeated_consecutively"] is not True:
        raise ValidationError("cas_kl50_observation.on_value_repeated_consecutively: expected true")
    if document["observed_transition"] != ["OFF", "ON", "OFF"] or document["status"] != "CONFIRMED":
        raise ValidationError("cas_kl50_observation: expected confirmed OFF/ON/OFF transition")

    if document["timestamp_basis"] != "UNAVAILABLE" or document["duration_us"] is not None:
        raise ValidationError("cas_kl50_observation: Level 1 trace cannot claim timestamps or duration")
    if document["duration_status"] != "PENDING_LEVEL3_TRACE":
        raise ValidationError("cas_kl50_observation.duration_status: Level 3 trace must remain pending")
    if document["rpm_alignment_status"] != "PENDING":
        raise ValidationError("cas_kl50_observation.rpm_alignment_status: expected PENDING")
    if document["raw_trace_available_in_repository"] is not False:
        raise ValidationError("cas_kl50_observation.raw_trace_available_in_repository: expected false")

    required_forbidden = {
        "CAN_IDENTIFIER",
        "CAS_COMMAND",
        "KL50_COMMAND",
        "TRANSMISSION_PATH",
        "IMMOBILIZER_BYPASS",
        "REMOTE_START_ACTUATION",
    }
    forbidden = set(_require_array(document["forbidden_inferences"], "cas_kl50_observation.forbidden_inferences"))
    if not required_forbidden.issubset(forbidden):
        raise ValidationError("cas_kl50_observation: all command and bypass inferences must remain forbidden")
    _require_nonempty_string(document["notes"], "cas_kl50_observation.notes")


def validate_cas_kl50_timing_observation(
    document: Mapping[str, Any], evidence_ids: set[str]
) -> None:
    _required(
        document,
        {
            "schema_version",
            "artifact_type",
            "observation_id",
            "profile_ref",
            "evidence_index",
            "evidence_ref",
            "source",
            "signal",
            "timestamp_basis",
            "boundary_samples",
            "consecutive_on_samples",
            "sampling_period_approx_ms",
            "sampling_frequency_approx_hz",
            "duration_min_ms",
            "duration_max_ms",
            "duration_estimate_ms",
            "timestamp_resolution_approx_ms",
            "duration_status",
            "qualification",
            "rpm_alignment_status",
            "raw_trace_available_in_repository",
            "forbidden_inferences",
            "notes",
        },
        "cas_kl50_timing_observation",
    )
    _require_schema_v1(document, "cas_kl50_timing_observation")
    if document["artifact_type"] != "CAS_KL50_TIMING_OBSERVATION":
        raise ValidationError("cas_kl50_timing_observation.artifact_type: unsupported type")
    for field in ("observation_id", "profile_ref", "evidence_index"):
        _require_nonempty_string(document[field], f"cas_kl50_timing_observation.{field}")
    _validate_evidence_ref(
        document["evidence_ref"], evidence_ids, "cas_kl50_timing_observation.evidence_ref"
    )

    source = _require_object(document["source"], "cas_kl50_timing_observation.source")
    _required(
        source,
        {"tool", "artifact", "sgbd_prg", "job", "field", "trace_level", "recorded_at", "session"},
        "cas_kl50_timing_observation.source",
    )
    expected_source = {
        "tool": "EDIABAS IFH trace",
        "artifact": "ifh.trc",
        "sgbd_prg": "CAS.PRG",
        "job": "status_fzg_zustand",
        "field": "KLEMMENSTATUS bits 4-5",
        "trace_level": 3,
    }
    for field, expected in expected_source.items():
        if source[field] != expected:
            raise ValidationError(
                f"cas_kl50_timing_observation.source.{field}: expected {expected!r}"
            )
    _require_timestamp(source["recorded_at"], "cas_kl50_timing_observation.source.recorded_at")
    _require_nonempty_string(source["session"], "cas_kl50_timing_observation.source.session")
    if document["signal"] != "CAS.KLEMMENSTATUS.KL50":
        raise ValidationError("cas_kl50_timing_observation.signal: unexpected signal")
    if document["timestamp_basis"] != "SOURCE_ABSOLUTE_LOCAL":
        raise ValidationError("cas_kl50_timing_observation.timestamp_basis: expected Level 3 timestamps")

    samples = _require_array(
        document["boundary_samples"], "cas_kl50_timing_observation.boundary_samples"
    )
    if len(samples) != 4:
        raise ValidationError("cas_kl50_timing_observation.boundary_samples: exactly four required")
    expected_roles = [
        "LAST_OFF_BEFORE_ACTIVATION",
        "FIRST_ON",
        "LAST_ON",
        "FIRST_OFF_AFTER_ACTIVATION",
    ]
    expected_states = ["KL50_OFF", "KL50_ON", "KL50_ON", "KL50_OFF"]
    expected_values = [0x41, 0x55, 0x55, 0x45]
    timestamps: list[datetime] = []
    for index, raw_sample in enumerate(samples):
        location = f"cas_kl50_timing_observation.boundary_samples[{index}]"
        sample = _require_object(raw_sample, location)
        _required(sample, {"role", "timestamp", "raw_value", "interpreted_value"}, location)
        if sample["role"] != expected_roles[index] or sample["interpreted_value"] != expected_states[index]:
            raise ValidationError(f"{location}: unexpected boundary role or KL50 state")
        value = _validate_raw_byte(sample["raw_value"], f"{location}.raw_value")
        if value != expected_values[index]:
            raise ValidationError(f"{location}.raw_value: unexpected reported Level 3 value")
        decoded_on = ((value >> 4) & 0b11) == 0b01
        if decoded_on != (sample["interpreted_value"] == "KL50_ON"):
            raise ValidationError(f"{location}: raw KL50 bits disagree with interpreted state")
        _require_timestamp(sample["timestamp"], f"{location}.timestamp")
        timestamps.append(datetime.fromisoformat(sample["timestamp"].replace("Z", "+00:00")))
    if timestamps != sorted(timestamps) or len(set(timestamps)) != len(timestamps):
        raise ValidationError("cas_kl50_timing_observation.boundary_samples: timestamps must increase")

    on_samples = document["consecutive_on_samples"]
    if not isinstance(on_samples, int) or isinstance(on_samples, bool) or on_samples < 2:
        raise ValidationError("cas_kl50_timing_observation.consecutive_on_samples: expected at least two")
    sampling = _require_object(
        document["sampling_period_approx_ms"],
        "cas_kl50_timing_observation.sampling_period_approx_ms",
    )
    _required(sampling, {"minimum", "maximum", "nominal"}, "cas_kl50_timing_observation.sampling_period_approx_ms")
    period_values = [sampling[name] for name in ("minimum", "nominal", "maximum")]
    if any(not isinstance(value, int) or isinstance(value, bool) or value <= 0 for value in period_values):
        raise ValidationError("cas_kl50_timing_observation.sampling_period_approx_ms: positive integers required")
    if not sampling["minimum"] <= sampling["nominal"] <= sampling["maximum"]:
        raise ValidationError("cas_kl50_timing_observation.sampling_period_approx_ms: invalid bounds")
    frequency = document["sampling_frequency_approx_hz"]
    if not isinstance(frequency, (int, float)) or isinstance(frequency, bool) or frequency <= 0:
        raise ValidationError("cas_kl50_timing_observation.sampling_frequency_approx_hz: positive number required")
    if abs(frequency - 1000.0 / sampling["nominal"]) > 1.0:
        raise ValidationError("cas_kl50_timing_observation: sampling period and frequency disagree")

    observed_min_ms = round((timestamps[2] - timestamps[1]).total_seconds() * 1000)
    observed_max_ms = round((timestamps[3] - timestamps[0]).total_seconds() * 1000)
    observed_estimate_ms = round((observed_min_ms + observed_max_ms) / 2)
    if document["duration_min_ms"] != observed_min_ms:
        raise ValidationError("cas_kl50_timing_observation.duration_min_ms: inconsistent with timestamps")
    if document["duration_max_ms"] != observed_max_ms:
        raise ValidationError("cas_kl50_timing_observation.duration_max_ms: inconsistent with timestamps")
    if document["duration_estimate_ms"] != observed_estimate_ms:
        raise ValidationError("cas_kl50_timing_observation.duration_estimate_ms: expected rounded midpoint")
    resolution = document["timestamp_resolution_approx_ms"]
    if not isinstance(resolution, int) or isinstance(resolution, bool) or resolution <= 0:
        raise ValidationError("cas_kl50_timing_observation.timestamp_resolution_approx_ms: positive integer required")
    if document["duration_status"] != "OBSERVED_BOUNDED":
        raise ValidationError("cas_kl50_timing_observation.duration_status: expected OBSERVED_BOUNDED")
    if document["qualification"] != "CONFIRMED_FROM_LEVEL3_TRACE":
        raise ValidationError("cas_kl50_timing_observation.qualification: expected Level 3 qualification")
    if document["rpm_alignment_status"] != "PENDING":
        raise ValidationError("cas_kl50_timing_observation.rpm_alignment_status: expected PENDING")
    if document["raw_trace_available_in_repository"] is not False:
        raise ValidationError("cas_kl50_timing_observation.raw_trace_available_in_repository: expected false")
    if "duration_exact_ms" in document:
        raise ValidationError("cas_kl50_timing_observation: exact KL50 duration must not be claimed")

    required_forbidden = {
        "EXACT_DURATION",
        "CAN_IDENTIFIER",
        "CAS_COMMAND",
        "KL50_COMMAND",
        "TRANSMISSION_PATH",
        "IMMOBILIZER_BYPASS",
        "REMOTE_START_ACTUATION",
        "OEM_START_REQUEST",
        "OEM_START_AUTHORIZATION",
    }
    forbidden = set(
        _require_array(
            document["forbidden_inferences"],
            "cas_kl50_timing_observation.forbidden_inferences",
        )
    )
    if not required_forbidden.issubset(forbidden):
        raise ValidationError(
            "cas_kl50_timing_observation: exact timing, commands, transport and authorization must remain forbidden"
        )
    _require_nonempty_string(document["notes"], "cas_kl50_timing_observation.notes")


def validate_cas_dde_diagnostic_correlation(
    document: Mapping[str, Any], evidence_ids: set[str]
) -> None:
    location = "cas_dde_diagnostic_correlation"
    _required(
        document,
        {
            "schema_version",
            "artifact_type",
            "observation_id",
            "profile_ref",
            "evidence_index",
            "evidence_ref",
            "source",
            "source_payload_sha256",
            "recovery",
            "critical_samples",
            "transition_bounds",
            "observed_kl50_duration_bounds_ms",
            "diagnostic_correlation",
            "stabilized_rpm_summary",
            "qualification",
            "physical_transition_order",
            "rpm_alignment_status",
            "forbidden_inferences",
            "notes",
        },
        location,
    )
    _require_schema_v1(document, location)
    if document["artifact_type"] != "CAS_DDE_DIAGNOSTIC_CORRELATION_OBSERVATION":
        raise ValidationError(f"{location}.artifact_type: unsupported type")
    for field in ("observation_id", "profile_ref", "evidence_index"):
        _require_nonempty_string(document[field], f"{location}.{field}")
    _validate_evidence_ref(document["evidence_ref"], evidence_ids, f"{location}.evidence_ref")

    source = _require_object(document["source"], f"{location}.source")
    _required(
        source,
        {
            "tool",
            "process_count",
            "session_count",
            "clock",
            "acquisition_order",
            "cas",
            "dde",
            "captured_at",
            "read_only",
        },
        f"{location}.source",
    )
    expected_source = {
        "tool": "TestO 2.0",
        "process_count": 1,
        "session_count": 1,
        "clock": "Date.now() milliseconds",
        "acquisition_order": ["CAS", "DDE"],
        "read_only": True,
    }
    for field, expected in expected_source.items():
        if source[field] != expected:
            raise ValidationError(f"{location}.source.{field}: expected {expected!r}")
    expected_signals = {
        "cas": {
            "sgbd_prg": "CAS.PRG",
            "job": "STATUS_FZG_ZUSTAND",
            "field": "KLEMMENSTATUS",
        },
        "dde": {
            "sgbd_prg": "D71N47C0.PRG",
            "job": "STATUS_MOTORDREHZAHL",
            "field": "STAT_MOTORDREHZAHL_WERT",
        },
    }
    for ecu, expected in expected_signals.items():
        signal_source = _require_object(source[ecu], f"{location}.source.{ecu}")
        _required(signal_source, expected, f"{location}.source.{ecu}")
        if signal_source != expected:
            raise ValidationError(f"{location}.source.{ecu}: unexpected diagnostic source")
    _require_timestamp(source["captured_at"], f"{location}.source.captured_at")

    payload_digest = _require_nonempty_string(
        document["source_payload_sha256"], f"{location}.source_payload_sha256"
    )
    if re.fullmatch(r"[0-9a-f]{64}", payload_digest) is None:
        raise ValidationError(f"{location}.source_payload_sha256: expected lowercase SHA-256")

    recovery = _require_object(document["recovery"], f"{location}.recovery")
    _required(
        recovery,
        {
            "declared_samples",
            "recovered_samples",
            "missing_samples",
            "duplicate_samples",
            "all_recovered_reads_ok",
            "full_raw_file_in_repository",
            "raw_excerpt_in_repository",
        },
        f"{location}.recovery",
    )
    declared = recovery["declared_samples"]
    recovered = recovery["recovered_samples"]
    missing = _require_array(recovery["missing_samples"], f"{location}.recovery.missing_samples")
    duplicates = _require_array(recovery["duplicate_samples"], f"{location}.recovery.duplicate_samples")
    if any(not isinstance(value, int) or isinstance(value, bool) or value < 0 for value in (declared, recovered)):
        raise ValidationError(f"{location}.recovery: sample counts must be non-negative integers")
    if missing != sorted(set(missing)) or any(
        not isinstance(value, int) or isinstance(value, bool) or value < 0 for value in missing
    ):
        raise ValidationError(f"{location}.recovery.missing_samples: sorted unique integers required")
    if duplicates:
        raise ValidationError(f"{location}.recovery.duplicate_samples: duplicates are not accepted")
    if declared != recovered + len(missing):
        raise ValidationError(f"{location}.recovery: recovered plus missing must equal declared")
    if recovery["all_recovered_reads_ok"] is not True:
        raise ValidationError(f"{location}.recovery: every recovered CAS/DDE read must be OK")
    if recovery["full_raw_file_in_repository"] is not False or recovery["raw_excerpt_in_repository"] is not True:
        raise ValidationError(f"{location}.recovery: must distinguish excerpt from unavailable full raw file")

    samples = _require_array(document["critical_samples"], f"{location}.critical_samples")
    if [sample.get("sample") if isinstance(sample, dict) else None for sample in samples] != list(range(9, 21)):
        raise ValidationError(f"{location}.critical_samples: exact samples 9 through 20 required")
    by_sample: dict[int, Mapping[str, Any]] = {}
    previous_cas = -1
    previous_dde = -1
    for index, raw_sample in enumerate(samples):
        sample_location = f"{location}.critical_samples[{index}]"
        sample = _require_object(raw_sample, sample_location)
        _required(
            sample,
            {
                "sample",
                "cas_timestamp_ms",
                "klemmenstatus_decimal",
                "klemmenstatus_hex",
                "kl50_interpretation",
                "dde_timestamp_ms",
                "rpm",
                "end_timestamp_ms",
                "cas_ok",
                "dde_ok",
            },
            sample_location,
        )
        sample_number = sample["sample"]
        cas_timestamp = sample["cas_timestamp_ms"]
        dde_timestamp = sample["dde_timestamp_ms"]
        end_timestamp = sample["end_timestamp_ms"]
        if any(
            not isinstance(value, int) or isinstance(value, bool) or value < 0
            for value in (sample_number, cas_timestamp, dde_timestamp, end_timestamp)
        ):
            raise ValidationError(f"{sample_location}: sample and timestamps must be non-negative integers")
        if not previous_cas < cas_timestamp < dde_timestamp <= end_timestamp or dde_timestamp <= previous_dde:
            raise ValidationError(f"{sample_location}: sequential timestamps must remain strictly ordered")
        previous_cas = cas_timestamp
        previous_dde = dde_timestamp
        if sample["cas_ok"] is not True or sample["dde_ok"] is not True:
            raise ValidationError(f"{sample_location}: diagnostic reads must remain successful")
        klemmenstatus = sample["klemmenstatus_decimal"]
        if klemmenstatus not in {69, 85} or sample["klemmenstatus_hex"] != f"0x{klemmenstatus:02X}":
            raise ValidationError(f"{sample_location}: inconsistent KLEMMENSTATUS representations")
        expected_kl50 = "KL50_ON" if ((klemmenstatus >> 4) & 0b11) == 0b01 else "KL50_OFF"
        if sample["kl50_interpretation"] != expected_kl50:
            raise ValidationError(f"{sample_location}: KL50 interpretation disagrees with bits 4-5")
        rpm = sample["rpm"]
        if not isinstance(rpm, (int, float)) or isinstance(rpm, bool) or rpm < 0:
            raise ValidationError(f"{sample_location}.rpm: expected non-negative number")
        by_sample[sample_number] = sample

    expected_critical = {
        9: (1786303279519, 69, 1786303279572, 0.0),
        10: (1786303279776, 85, 1786303279829, 131.0),
        11: (1786303280089, 85, 1786303280142, 224.5),
        12: (1786303280347, 85, 1786303280399, 863.0),
        13: (1786303280653, 69, 1786303280706, 981.5),
    }
    for sample_number, expected in expected_critical.items():
        sample = by_sample[sample_number]
        actual = (
            sample["cas_timestamp_ms"],
            sample["klemmenstatus_decimal"],
            sample["dde_timestamp_ms"],
            float(sample["rpm"]),
        )
        if actual != expected:
            raise ValidationError(f"{location}.critical_samples: sample {sample_number} changed")

    transitions = _require_object(document["transition_bounds"], f"{location}.transition_bounds")
    _required(transitions, {"kl50_on", "rpm_nonzero", "kl50_off"}, f"{location}.transition_bounds")
    expected_bounds = {
        "kl50_on": (by_sample[9]["cas_timestamp_ms"], by_sample[10]["cas_timestamp_ms"]),
        "rpm_nonzero": (by_sample[9]["dde_timestamp_ms"], by_sample[10]["dde_timestamp_ms"]),
        "kl50_off": (by_sample[12]["cas_timestamp_ms"], by_sample[13]["cas_timestamp_ms"]),
    }
    for name, (lower, upper) in expected_bounds.items():
        bound = _require_object(transitions[name], f"{location}.transition_bounds.{name}")
        _required(
            bound,
            {"after_timestamp_ms_exclusive", "by_timestamp_ms_inclusive", "window_ms", "physical_instant_known"},
            f"{location}.transition_bounds.{name}",
        )
        if (
            bound["after_timestamp_ms_exclusive"],
            bound["by_timestamp_ms_inclusive"],
            bound["window_ms"],
            bound["physical_instant_known"],
        ) != (lower, upper, upper - lower, False):
            raise ValidationError(f"{location}.transition_bounds.{name}: inconsistent sampling bound")

    duration = _require_object(
        document["observed_kl50_duration_bounds_ms"],
        f"{location}.observed_kl50_duration_bounds_ms",
    )
    _required(duration, {"minimum", "maximum", "exact_physical_duration", "status"}, f"{location}.observed_kl50_duration_bounds_ms")
    expected_minimum = by_sample[12]["cas_timestamp_ms"] - by_sample[10]["cas_timestamp_ms"]
    expected_maximum = by_sample[13]["cas_timestamp_ms"] - by_sample[9]["cas_timestamp_ms"]
    if (duration["minimum"], duration["maximum"]) != (expected_minimum, expected_maximum):
        raise ValidationError(f"{location}.observed_kl50_duration_bounds_ms: inconsistent bounds")
    if duration["exact_physical_duration"] is not None or duration["status"] != "OBSERVED_BOUNDED_DIAGNOSTIC_SAMPLING":
        raise ValidationError(f"{location}.observed_kl50_duration_bounds_ms: exact duration must remain unknown")

    correlation = _require_object(document["diagnostic_correlation"], f"{location}.diagnostic_correlation")
    _required(
        correlation,
        {
            "same_clock",
            "same_process",
            "same_ediabas_session",
            "cas_then_dde_sequential",
            "first_on_to_first_nonzero_read_gap_ms",
            "last_on_to_863_rpm_read_gap_ms",
            "first_off_to_981_5_rpm_read_gap_ms",
        },
        f"{location}.diagnostic_correlation",
    )
    if any(correlation[field] is not True for field in ("same_clock", "same_process", "same_ediabas_session", "cas_then_dde_sequential")):
        raise ValidationError(f"{location}.diagnostic_correlation: acquisition context must remain explicit")
    expected_gaps = (
        by_sample[10]["dde_timestamp_ms"] - by_sample[10]["cas_timestamp_ms"],
        by_sample[12]["dde_timestamp_ms"] - by_sample[12]["cas_timestamp_ms"],
        by_sample[13]["dde_timestamp_ms"] - by_sample[13]["cas_timestamp_ms"],
    )
    actual_gaps = (
        correlation["first_on_to_first_nonzero_read_gap_ms"],
        correlation["last_on_to_863_rpm_read_gap_ms"],
        correlation["first_off_to_981_5_rpm_read_gap_ms"],
    )
    if actual_gaps != expected_gaps:
        raise ValidationError(f"{location}.diagnostic_correlation: read gaps must derive from timestamps")

    stable = _require_object(document["stabilized_rpm_summary"], f"{location}.stabilized_rpm_summary")
    _required(stable, {"sample_range", "recovered_sample_count", "minimum", "median", "mean", "maximum", "unit"}, f"{location}.stabilized_rpm_summary")
    if stable["sample_range"] != [19, 299] or stable["recovered_sample_count"] != 272 or stable["unit"] != "rpm":
        raise ValidationError(f"{location}.stabilized_rpm_summary: unexpected recovery scope")
    if (stable["minimum"], stable["median"], stable["mean"], stable["maximum"]) != (773, 781, 780.557, 786.5):
        raise ValidationError(f"{location}.stabilized_rpm_summary: reported statistics changed")

    if document["qualification"] != "CONFIRMED_DIAGNOSTIC_CORRELATION":
        raise ValidationError(f"{location}.qualification: expected diagnostic correlation only")
    if document["physical_transition_order"] != "UNKNOWN":
        raise ValidationError(f"{location}.physical_transition_order: sequential reads cannot prove physical order")
    if document["rpm_alignment_status"] != "OBSERVED_BOUNDED":
        raise ValidationError(f"{location}.rpm_alignment_status: expected OBSERVED_BOUNDED")
    required_forbidden = {
        "EXACT_PHYSICAL_KL50_ON_TIME",
        "EXACT_PHYSICAL_KL50_OFF_TIME",
        "PHYSICAL_KL50_VS_RPM_ORDER",
        "CAN_IDENTIFIER",
        "CAN_PAYLOAD",
        "CAS_COMMAND",
        "DDE_COMMAND",
        "KL50_COMMAND",
        "TRANSMISSION_PATH",
        "IMMOBILIZER_BYPASS",
        "REMOTE_START_ACTUATION",
        "OEM_START_REQUEST",
        "OEM_START_AUTHORIZATION",
        "FINAL_ENGINE_RUNNING_ALGORITHM",
    }
    forbidden = set(_require_array(document["forbidden_inferences"], f"{location}.forbidden_inferences"))
    if not required_forbidden.issubset(forbidden):
        raise ValidationError(f"{location}: physical timing, CAN, commands and actuation inferences must remain forbidden")
    _require_nonempty_string(document["notes"], f"{location}.notes")


def validate_synchronized_start_observation(document: Mapping[str, Any], evidence_ids: set[str]) -> None:
    _required(
        document,
        {
            "schema_version",
            "timeline_id",
            "profile_ref",
            "evidence_index",
            "evidence_ref",
            "source",
            "timestamp_basis",
            "events",
            "constant_bitfields",
            "subsequent_rpm_summary",
            "functional_identification",
        },
        "synchronized_start",
    )
    _require_schema_v1(document, "synchronized_start")
    for field in ("timeline_id", "profile_ref", "evidence_index"):
        _require_nonempty_string(document[field], f"synchronized_start.{field}")
    _validate_evidence_ref(document["evidence_ref"], evidence_ids, "synchronized_start.evidence_ref")
    if document["timestamp_basis"] != "SESSION_RELATIVE":
        raise ValidationError("synchronized_start.timestamp_basis: expected SESSION_RELATIVE")
    if document["functional_identification"] != "UNKNOWN":
        raise ValidationError("synchronized_start.functional_identification: correlation cannot assign meaning")
    source = _require_object(document["source"], "synchronized_start.source")
    _required(source, {"tool", "sgbd_prg", "job", "session", "observed_at", "physical_context"}, "synchronized_start.source")
    for field in ("tool", "sgbd_prg", "job", "session", "physical_context"):
        _require_nonempty_string(source[field], f"synchronized_start.source.{field}")
    _require_timestamp(source["observed_at"], "synchronized_start.source.observed_at")

    previous_timestamp = -1
    for index, raw_event in enumerate(_require_array(document["events"], "synchronized_start.events")):
        location = f"synchronized_start.events[{index}]"
        event = _require_object(raw_event, location)
        _required(
            event,
            {"sequence", "timestamp_us", "event_type", "signal", "qualification", "functional_meaning", "correlation_label", "notes"},
            location,
        )
        if event["sequence"] != index:
            raise ValidationError(f"{location}.sequence: expected {index}")
        timestamp = event["timestamp_us"]
        if isinstance(timestamp, bool) or not isinstance(timestamp, int) or timestamp < previous_timestamp:
            raise ValidationError(f"{location}.timestamp_us: expected monotonic non-negative integer")
        previous_timestamp = timestamp
        _require_nonempty_string(event["signal"], f"{location}.signal")
        if event["qualification"] not in {"OBSERVED", "CONFIRMED"}:
            raise ValidationError(f"{location}.qualification: unsupported level")
        if event["functional_meaning"] is not None:
            raise ValidationError(f"{location}.functional_meaning: temporal correlation cannot assign meaning")
        if event["correlation_label"] != "TEMPORALLY_CORRELATED_WITH_OEM_START":
            raise ValidationError(f"{location}.correlation_label: unsupported label")
        _require_nonempty_string(event["notes"], f"{location}.notes")
        if event["event_type"] == "BITFIELD_CHANGE":
            _required(event, {"previous_value", "value", "delta_decimal", "changed_bits"}, location)
            previous = _validate_bitfield_value(event["previous_value"], f"{location}.previous_value")
            current = _validate_bitfield_value(event["value"], f"{location}.value")
            if event["delta_decimal"] != current - previous:
                raise ValidationError(f"{location}.delta_decimal: inconsistent delta")
            expected_bits = [bit for bit in range(max(previous, current).bit_length() + 1) if ((previous ^ current) >> bit) & 1]
            if event["changed_bits"] != expected_bits:
                raise ValidationError(f"{location}.changed_bits: inconsistent XOR bit list")
        elif event["event_type"] == "RPM_SAMPLE":
            _required(event, {"rpm", "observed_state"}, location)
            rpm = event["rpm"]
            if isinstance(rpm, bool) or not isinstance(rpm, (int, float)) or rpm < 0:
                raise ValidationError(f"{location}.rpm: expected non-negative number")
            if event["observed_state"] not in {"STOPPED", "CRANKING", "RUNNING", "UNKNOWN"}:
                raise ValidationError(f"{location}.observed_state: unsupported state")
        else:
            raise ValidationError(f"{location}.event_type: unsupported event type")

    for index, raw_constant in enumerate(_require_array(document["constant_bitfields"], "synchronized_start.constant_bitfields")):
        location = f"synchronized_start.constant_bitfields[{index}]"
        constant = _require_object(raw_constant, location)
        _required(constant, {"signal", "value", "qualification", "suitability", "functional_meaning", "notes"}, location)
        _require_nonempty_string(constant["signal"], f"{location}.signal")
        _validate_bitfield_value(constant["value"], f"{location}.value")
        if constant["qualification"] != "OBSERVED" or constant["suitability"] != "OBSERVED_NO_TRANSITION":
            raise ValidationError(f"{location}: constant signals must remain OBSERVED_NO_TRANSITION")
        if constant["functional_meaning"] is not None:
            raise ValidationError(f"{location}.functional_meaning: constant observation cannot assign meaning")
        _require_nonempty_string(constant["notes"], f"{location}.notes")

    summary = _require_object(document["subsequent_rpm_summary"], "synchronized_start.subsequent_rpm_summary")
    _required(summary, {"peak_rpm_approx", "stabilized_rpm_approx", "timestamps_available", "notes"}, "synchronized_start.subsequent_rpm_summary")
    if summary["timestamps_available"] is not False:
        raise ValidationError("synchronized_start.subsequent_rpm_summary: unavailable timestamps must remain false")
    for field in ("peak_rpm_approx", "stabilized_rpm_approx"):
        if isinstance(summary[field], bool) or not isinstance(summary[field], (int, float)) or summary[field] < 0:
            raise ValidationError(f"synchronized_start.subsequent_rpm_summary.{field}: expected non-negative number")
    _require_nonempty_string(summary["notes"], "synchronized_start.subsequent_rpm_summary.notes")


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
        {
            "schema_version",
            "session_id",
            "example_only",
            "profile_ref",
            "evidence_index",
            "started_at",
            "timestamp_basis",
            "records",
        },
        "observation",
    )
    _require_schema_v1(document, "observation")
    _require_nonempty_string(document["session_id"], "observation.session_id")
    _require_nonempty_string(document["profile_ref"], "observation.profile_ref")
    _require_nonempty_string(document["evidence_index"], "observation.evidence_index")
    _require_timestamp(document["started_at"], "observation.started_at")
    timestamp_basis = document["timestamp_basis"]
    if timestamp_basis not in TIMESTAMP_BASES:
        raise ValidationError("observation.timestamp_basis: unsupported basis")
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
        if timestamp_basis == "UNAVAILABLE":
            if timestamp is not None:
                raise ValidationError(f"{location}.timestamp_us: must be null when timestamp basis is UNAVAILABLE")
        else:
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
        if status in {
            "OBSERVED",
            "OBSERVED_BOUNDED",
            "CONFIRMED",
            "UNTRUSTED",
            "NOT_YET_VALIDATED",
            "PENDING",
            "PENDING_LEVEL3_TRACE",
            "VALIDATED",
        } and not references:
            raise ValidationError(f"{location}: {status} requires evidence")
        if status == "UNKNOWN" and references:
            raise ValidationError(f"{location}: UNKNOWN cannot claim evidence")
        blocker = item["blocker_reason"]
        if status == "BLOCKED":
            _require_nonempty_string(blocker, f"{location}.blocker_reason")
        elif blocker is not None:
            raise ValidationError(f"{location}.blocker_reason: only valid for BLOCKED")
        if status in {
            "OBSERVED_BOUNDED",
            "UNTRUSTED",
            "NOT_YET_VALIDATED",
            "PENDING",
            "PENDING_LEVEL3_TRACE",
        }:
            _require_nonempty_string(item["notes"], f"{location}.notes")


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
    _required(
        session,
        {"session_id", "profile_ref", "evidence_index", "started_at", "timestamp_basis", "notes"},
        "mapping.session",
    )
    for field in ("session_id", "profile_ref", "evidence_index"):
        _require_nonempty_string(session[field], f"mapping.session.{field}")
    _require_timestamp(session["started_at"], "mapping.session.started_at")
    if session["timestamp_basis"] not in {"SESSION_RELATIVE", "SOURCE_ABSOLUTE"}:
        raise ValidationError("mapping.session.timestamp_basis: delimited imports require timestamps")
    _validate_evidence_ref(document["evidence_ref"], evidence_ids, "mapping.evidence_ref")


def _parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate Phase 3C/3D vehicle-data artifacts")
    parser.add_argument("--evidence", required=True, type=Path)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--observation", type=Path)
    parser.add_argument("--checklist", type=Path)
    parser.add_argument("--mapping", type=Path)
    parser.add_argument("--qualified-observations", action="append", default=[], type=Path)
    parser.add_argument("--engine-timeline", action="append", default=[], type=Path)
    parser.add_argument("--signal-sources", action="append", default=[], type=Path)
    parser.add_argument("--synchronized-start", action="append", default=[], type=Path)
    parser.add_argument("--kl50-observation", action="append", default=[], type=Path)
    parser.add_argument("--kl50-timing-observation", action="append", default=[], type=Path)
    parser.add_argument("--cas-dde-correlation", action="append", default=[], type=Path)
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
    for path in arguments.qualified_observations:
        validate_qualified_observations(load_json_object(path), evidence_ids)
    for path in arguments.engine_timeline:
        validate_engine_run_timeline(load_json_object(path), evidence_ids)
    for path in arguments.signal_sources:
        validate_signal_source_qualifications(load_json_object(path), evidence_ids)
    for path in arguments.synchronized_start:
        validate_synchronized_start_observation(load_json_object(path), evidence_ids)
    for path in arguments.kl50_observation:
        validate_cas_kl50_observation(load_json_object(path), evidence_ids)
    for path in arguments.kl50_timing_observation:
        validate_cas_kl50_timing_observation(load_json_object(path), evidence_ids)
    for path in arguments.cas_dde_correlation:
        validate_cas_dde_diagnostic_correlation(load_json_object(path), evidence_ids)
    print("Phase 3C/3D vehicle data: VALID")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ValidationError as error:
        print(f"Phase 3C/3D vehicle data: INVALID: {error}")
        raise SystemExit(2) from error
