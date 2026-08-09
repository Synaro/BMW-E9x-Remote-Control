import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from import_vehicle_data import import_delimited
from vehicle_data_validation import (
    REQUIRED_CHECKLIST_ITEMS,
    ValidationError,
    load_json_object,
    validate_evidence_index,
    validate_import_mapping,
    validate_observation_session,
    validate_prerequisites,
    validate_vehicle_profile,
)


DATA = ROOT / "vehicle-data"
EVIDENCE_PATH = DATA / "evidence" / "EXAMPLE_ONLY.evidence-index.json"
PROFILE_PATH = DATA / "profiles" / "EXAMPLE_ONLY.vehicle-profile.json"
OBSERVATION_PATH = DATA / "observations" / "EXAMPLE_ONLY.oem-start-observation.json"
CHECKLIST_PATH = DATA / "profiles" / "EXAMPLE_ONLY.remote-start-prerequisites.json"
MAPPING_PATH = DATA / "imports" / "EXAMPLE_ONLY.mapping.json"
INPUT_PATH = DATA / "imports" / "EXAMPLE_ONLY.input.csv"
REAL_EVIDENCE_PATH = DATA / "evidence" / "current-test-vehicle.evidence-index.json"
REAL_PROFILE_PATH = DATA / "profiles" / "current-test-vehicle.vehicle-profile.json"
REAL_OBSERVATION_PATH = (
    DATA / "observations" / "current-test-vehicle-tool32-cas-klemmenstatus-2026-08-09.json"
)
REAL_CHECKLIST_PATH = (
    DATA / "profiles" / "current-test-vehicle.remote-start-prerequisites.json"
)


class VehicleDataTests(unittest.TestCase):
    def setUp(self):
        self.evidence = load_json_object(EVIDENCE_PATH)
        self.evidence_ids = validate_evidence_index(
            self.evidence, evidence_directory=EVIDENCE_PATH.parent
        )
        self.profile = load_json_object(PROFILE_PATH)
        self.observation = load_json_object(OBSERVATION_PATH)
        self.checklist = load_json_object(CHECKLIST_PATH)
        self.mapping = load_json_object(MAPPING_PATH)

    def test_all_versioned_schemas_are_valid_json_schema_documents(self):
        schema_paths = sorted((DATA / "schema").glob("*.schema.json"))
        self.assertEqual(17, len(schema_paths))
        for path in schema_paths:
            schema = json.loads(path.read_text(encoding="utf-8"))
            self.assertEqual("https://json-schema.org/draft/2020-12/schema", schema["$schema"])
            self.assertIn("$id", schema)
            self.assertIn("title", schema)

    def test_example_artifacts_validate_together(self):
        validate_vehicle_profile(self.profile, self.evidence_ids)
        validate_observation_session(self.observation, self.evidence_ids)
        validate_prerequisites(self.checklist, self.evidence_ids)
        validate_import_mapping(self.mapping, self.evidence_ids)

    def test_profile_rejects_missing_required_field(self):
        del self.profile["vehicle"]["engine"]
        with self.assertRaisesRegex(ValidationError, "missing required fields: engine"):
            validate_vehicle_profile(self.profile, self.evidence_ids)

    def test_profile_rejects_observed_value_without_provenance(self):
        self.profile["vehicle"]["engine"] = {
            "value": "OBSERVED_VALUE",
            "observed_at": "2000-01-01T00:00:00Z",
            "confidence": "HIGH",
            "evidence_refs": [],
            "notes": None,
        }
        with self.assertRaisesRegex(ValidationError, "requires provenance"):
            validate_vehicle_profile(self.profile, self.evidence_ids)

    def test_profile_rejects_unknown_with_claimed_value(self):
        self.profile["vehicle"]["model"]["value"] = "CLAIMED_VALUE"
        with self.assertRaisesRegex(ValidationError, "UNKNOWN must not claim"):
            validate_vehicle_profile(self.profile, self.evidence_ids)

    def test_profile_rejects_invalid_confidence(self):
        self.profile["dde"]["type_version"]["confidence"] = "CERTAIN"
        with self.assertRaisesRegex(ValidationError, "unsupported level"):
            validate_vehicle_profile(self.profile, self.evidence_ids)

    def test_evidence_rejects_unknown_kind(self):
        evidence = copy.deepcopy(self.evidence)
        evidence["entries"][0]["kind"] = "SCREENSHOT"
        with self.assertRaisesRegex(ValidationError, "unsupported evidence kind"):
            validate_evidence_index(evidence)

    def test_evidence_rejects_path_traversal(self):
        evidence = copy.deepcopy(self.evidence)
        evidence["entries"][0]["relative_path"] = "../outside.txt"
        with self.assertRaisesRegex(ValidationError, "must stay inside"):
            validate_evidence_index(evidence)

    def test_evidence_rejects_missing_referenced_file(self):
        evidence = copy.deepcopy(self.evidence)
        evidence["entries"][0]["relative_path"] = "missing.txt"
        with self.assertRaisesRegex(ValidationError, "does not exist"):
            validate_evidence_index(evidence, evidence_directory=EVIDENCE_PATH.parent)

    def test_evidence_rejects_mismatched_digest(self):
        evidence = copy.deepcopy(self.evidence)
        evidence["entries"][0]["sha256"] = "0" * 64
        with self.assertRaisesRegex(ValidationError, "digest does not match"):
            validate_evidence_index(evidence, evidence_directory=EVIDENCE_PATH.parent)

    def test_observation_rejects_non_monotonic_timestamps(self):
        self.observation["records"][2]["timestamp_us"] = 1
        with self.assertRaisesRegex(ValidationError, "timestamps must be monotonic"):
            validate_observation_session(self.observation, self.evidence_ids)

    def test_observation_rejects_sequence_gap(self):
        self.observation["records"][1]["sequence"] = 8
        with self.assertRaisesRegex(ValidationError, "expected 1"):
            validate_observation_session(self.observation, self.evidence_ids)

    def test_observation_rejects_unknown_provenance(self):
        self.observation["records"][0]["source"]["evidence_ref"] = "MISSING"
        with self.assertRaisesRegex(ValidationError, "unknown evidence reference"):
            validate_observation_session(self.observation, self.evidence_ids)

    def test_observation_rejects_incoherent_engine_speed_unit(self):
        record = self.observation["records"][0]
        record["signal"] = "ENGINE_SPEED"
        record["interpreted_value"] = 500
        record["unit"] = "V"
        with self.assertRaisesRegex(ValidationError, "numeric value in rpm"):
            validate_observation_session(self.observation, self.evidence_ids)

    def test_observation_rejects_empty_raw_and_interpreted_values(self):
        record = self.observation["records"][0]
        record["raw_value"] = None
        record["interpreted_value"] = None
        with self.assertRaisesRegex(ValidationError, "cannot both be null"):
            validate_observation_session(self.observation, self.evidence_ids)

    def test_checklist_has_exact_required_items(self):
        self.assertEqual(REQUIRED_CHECKLIST_ITEMS, set(self.checklist["items"]))
        del self.checklist["items"]["kl50"]
        with self.assertRaisesRegex(ValidationError, "exact item set required"):
            validate_prerequisites(self.checklist, self.evidence_ids)

    def test_checklist_rejects_observed_without_evidence(self):
        self.checklist["items"]["kl15"]["status"] = "OBSERVED"
        with self.assertRaisesRegex(ValidationError, "OBSERVED requires evidence"):
            validate_prerequisites(self.checklist, self.evidence_ids)

    def test_checklist_rejects_blocked_without_reason(self):
        self.checklist["items"]["kl15"]["status"] = "BLOCKED"
        with self.assertRaisesRegex(ValidationError, "must be a non-empty string"):
            validate_prerequisites(self.checklist, self.evidence_ids)

    def test_generic_import_produces_valid_ordered_session(self):
        session = import_delimited(INPUT_PATH, self.mapping, self.evidence_ids)
        self.assertEqual([0, 1, 2], [record["sequence"] for record in session["records"]])
        self.assertEqual([0, 1000000, 2000000], [record["timestamp_us"] for record in session["records"]])
        validate_observation_session(session, self.evidence_ids)

    def test_generic_import_rejects_missing_mapped_column(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "incomplete.csv"
            path.write_text("time_us,stage\n0,PRECHECK\n", encoding="utf-8")
            with self.assertRaisesRegex(ValidationError, "missing mapped columns"):
                import_delimited(path, self.mapping, self.evidence_ids)

    def test_generic_import_rejects_incoherent_row(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.csv"
            path.write_text(
                "time_us,stage,measurement,raw,meaning,engineering_unit,confidence,source_row\n"
                "0,PRECHECK,KL15,raw,not-a-boolean,,LOW,row 1\n",
                encoding="utf-8",
            )
            with self.assertRaisesRegex(ValidationError, "requires a boolean"):
                import_delimited(path, self.mapping, self.evidence_ids)

    def test_confirmed_tool32_kl15_observation_preserves_engine_running_exclusion(self):
        evidence = load_json_object(REAL_EVIDENCE_PATH)
        evidence_ids = validate_evidence_index(
            evidence, evidence_directory=REAL_EVIDENCE_PATH.parent
        )
        profile = load_json_object(REAL_PROFILE_PATH)
        observation = load_json_object(REAL_OBSERVATION_PATH)
        checklist = load_json_object(REAL_CHECKLIST_PATH)
        validate_vehicle_profile(profile, evidence_ids)
        validate_observation_session(observation, evidence_ids)
        validate_prerequisites(checklist, evidence_ids)

        value_69_records = [
            record for record in observation["records"] if record["raw_value"] == 69
        ]
        self.assertEqual({"KL15_ACTIVE", "ENGINE_RUNNING"}, {record["phase"] for record in value_69_records})
        self.assertTrue(all(record["signal"] == "KL15" for record in value_69_records))
        self.assertTrue(all(record["interpreted_value"] is True for record in value_69_records))
        self.assertEqual("UNAVAILABLE", observation["timestamp_basis"])
        self.assertTrue(all(record["timestamp_us"] is None for record in observation["records"]))
        self.assertEqual("CONFIRMED", checklist["items"]["kl15"]["status"])
        self.assertEqual("CONFIRMED", checklist["items"]["engine_running_state"]["status"])
        self.assertIn(
            "disqualified",
            checklist["items"]["engine_running_state"]["notes"],
        )


if __name__ == "__main__":
    unittest.main()
