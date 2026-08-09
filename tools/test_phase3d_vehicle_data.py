import copy
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from import_engine_speed_log import build_timeline
from vehicle_data_validation import (
    ValidationError,
    is_precondition_candidate,
    load_json_object,
    validate_engine_run_timeline,
    validate_evidence_index,
    validate_prerequisites,
    validate_qualified_observations,
    validate_vehicle_profile,
)


DATA = ROOT / "vehicle-data"
EVIDENCE_PATH = DATA / "evidence" / "current-test-vehicle.evidence-index.json"
PROFILE_PATH = DATA / "profiles" / "current-test-vehicle.vehicle-profile.json"
CHECKLIST_PATH = DATA / "profiles" / "current-test-vehicle.remote-start-prerequisites.json"
IDENTIFICATION_PATH = DATA / "observations" / "current-test-vehicle-phase3d-identification-2026-08-09.json"
STATUS_PATH = DATA / "observations" / "current-test-vehicle-phase3d-status-observations-2026-08-09.json"
REAL_TIMELINE_PATH = DATA / "observations" / "current-test-vehicle-testo-oem-start-sequence-2026-08-09.json"
EXAMPLE_EVIDENCE_PATH = DATA / "evidence" / "EXAMPLE_ONLY.evidence-index.json"
EXAMPLE_ENGINE_INPUT = DATA / "imports" / "EXAMPLE_ONLY.engine-speed.csv"
EXAMPLE_ENGINE_MAPPING = DATA / "imports" / "EXAMPLE_ONLY.engine-speed.mapping.json"


class Phase3DVehicleDataTests(unittest.TestCase):
    def setUp(self):
        self.evidence = load_json_object(EVIDENCE_PATH)
        self.evidence_ids = validate_evidence_index(
            self.evidence, evidence_directory=EVIDENCE_PATH.parent
        )
        self.profile = load_json_object(PROFILE_PATH)
        self.checklist = load_json_object(CHECKLIST_PATH)
        self.identification = load_json_object(IDENTIFICATION_PATH)
        self.status = load_json_object(STATUS_PATH)
        self.timeline = load_json_object(REAL_TIMELINE_PATH)
        self.status_by_id = {
            observation["observation_id"]: observation
            for observation in self.status["observations"]
        }
        self.identification_by_id = {
            observation["observation_id"]: observation
            for observation in self.identification["observations"]
        }

    def test_all_real_phase3d_artifacts_validate(self):
        validate_vehicle_profile(self.profile, self.evidence_ids)
        validate_prerequisites(self.checklist, self.evidence_ids)
        validate_qualified_observations(self.identification, self.evidence_ids)
        validate_qualified_observations(self.status, self.evidence_ids)
        validate_engine_run_timeline(self.timeline, self.evidence_ids)

    def test_identification_values_are_preserved_as_reported(self):
        expected = {
            "cas.bmw_part_number": "9147226",
            "egs.bmw_part_number": "7591971",
            "egs.programmed_part_number": "000007603531",
            "dde.bmw_part_number": "7823420",
            "dde.software_version": "1037396565",
        }
        for observation_id, raw_value in expected.items():
            observation = self.identification_by_id[observation_id]
            self.assertEqual(raw_value, observation["raw_value"])
            self.assertEqual("CONFIRMED", observation["qualification"])

    def test_every_qualified_observation_has_explicit_provenance(self):
        for bundle in (self.identification, self.status):
            for observation in bundle["observations"]:
                source = observation["source"]
                self.assertTrue(source["tool"])
                self.assertIn("sgbd_prg", source)
                self.assertIn("job", source)
                self.assertTrue(source["field"])
                self.assertTrue(source["physical_context"])
                self.assertTrue(source["observed_at"])
                self.assertTrue(source["session"])
                self.assertIn(source["evidence_ref"], self.evidence_ids)

    def test_klemmenstatus_mapping_and_engine_running_exclusion(self):
        expected = {
            "cas.klemmenstatus.awake_off": 64,
            "cas.klemmenstatus.key_inserted": 65,
            "cas.klemmenstatus.kl15_engine_stopped": 69,
            "cas.klemmenstatus.kl15_engine_running": 69,
            "cas.klemmenstatus.after_stop_off": 64,
        }
        for observation_id, raw_value in expected.items():
            self.assertEqual(raw_value, self.status_by_id[observation_id]["raw_value"])
        self.assertEqual(
            self.status_by_id["cas.klemmenstatus.kl15_engine_stopped"]["raw_value"],
            self.status_by_id["cas.klemmenstatus.kl15_engine_running"]["raw_value"],
        )
        self.assertIn(
            "standalone engine-running",
            self.status_by_id["cas.klemmenstatus.kl15_engine_running"]["notes"],
        )

    def test_tool32_egs_prnd_mapping_is_confirmed(self):
        for position in ("p", "r", "n", "d"):
            observation = self.status_by_id[f"egs.tool32.position.{position}"]
            self.assertEqual(position.upper(), observation["raw_value"])
            self.assertEqual(position.upper(), observation["interpreted_value"])
            self.assertEqual("CONFIRMED", observation["qualification"])
            self.assertTrue(
                is_precondition_candidate(
                    observation["qualification"], observation["precondition_eligibility"]
                )
            )

    def test_ista_transmission_position_is_untrusted_and_prohibited(self):
        expected = {"p": "R", "r": "N", "n": "D", "d": "D"}
        for physical, reported in expected.items():
            observation = self.status_by_id[f"egs.ista.transmission_position.{physical}"]
            self.assertEqual(reported, observation["raw_value"])
            self.assertEqual("UNTRUSTED", observation["qualification"])
            self.assertEqual("PROHIBITED", observation["precondition_eligibility"])
            self.assertFalse(
                is_precondition_candidate(
                    observation["qualification"], observation["precondition_eligibility"]
                )
            )

    def test_ista_actual_gear_cannot_distinguish_p_and_n(self):
        self.assertEqual(
            self.status_by_id["egs.ista.actual_gear.p"]["raw_value"],
            self.status_by_id["egs.ista.actual_gear.n"]["raw_value"],
        )
        for physical in ("p", "r", "n", "d"):
            observation = self.status_by_id[f"egs.ista.actual_gear.{physical}"]
            self.assertEqual("PROHIBITED", observation["precondition_eligibility"])

    def test_brake_operated_mapping_is_observed_but_not_candidate(self):
        released = self.status_by_id["egs.ista.brake.released"]
        pressed = self.status_by_id["egs.ista.brake.pressed"]
        self.assertEqual(("not operated", False), (released["raw_value"], released["interpreted_value"]))
        self.assertEqual(("operated", True), (pressed["raw_value"], pressed["interpreted_value"]))
        self.assertEqual("OBSERVED", pressed["qualification"])
        self.assertEqual("PROHIBITED", pressed["precondition_eligibility"])

    def test_dde_and_testo_engine_speed_observations(self):
        self.assertEqual(0, self.status_by_id["dde.tool32.rpm.stopped"]["interpreted_value"])
        self.assertEqual(780, self.status_by_id["dde.tool32.rpm.idle"]["interpreted_value"])
        self.assertEqual(786.5, self.status_by_id["dde.testo.rpm.idle"]["interpreted_value"])
        for observation_id in ("dde.tool32.rpm.stopped", "dde.tool32.rpm.idle", "dde.testo.rpm.idle"):
            self.assertEqual("CONFIRMED", self.status_by_id[observation_id]["qualification"])

    def test_real_testo_sequence_is_ordered_without_invented_timestamps(self):
        expected_rpm = [0, 103, 125, 129.5, 182.5, 193, 272, 383.5, 638.5, 890, 1022.5, 780]
        self.assertEqual(expected_rpm, [sample["rpm"] for sample in self.timeline["samples"]])
        self.assertEqual("UNAVAILABLE", self.timeline["timestamp_basis"])
        self.assertTrue(all(sample["timestamp_us"] is None for sample in self.timeline["samples"]))
        self.assertEqual("CRANKING", self.timeline["samples"][9]["observed_state"])
        self.assertEqual("CRANKING", self.timeline["samples"][10]["observed_state"])
        self.assertEqual("RUNNING", self.timeline["samples"][11]["observed_state"])

    def test_untrusted_or_blocked_data_cannot_become_candidate(self):
        self.assertFalse(is_precondition_candidate("UNTRUSTED", "CANDIDATE"))
        self.assertFalse(is_precondition_candidate("BLOCKED", "CANDIDATE"))
        blocked = self.status_by_id["testo.raw_csv.timeline"]
        self.assertEqual("BLOCKED", blocked["qualification"])
        self.assertEqual("PROHIBITED", blocked["precondition_eligibility"])
        modified = copy.deepcopy(self.status)
        observation = next(
            item for item in modified["observations"]
            if item["observation_id"] == "egs.ista.transmission_position.p"
        )
        observation["precondition_eligibility"] = "CANDIDATE"
        with self.assertRaisesRegex(ValidationError, "UNTRUSTED data must be PROHIBITED"):
            validate_qualified_observations(modified, self.evidence_ids)

    def test_checklist_reflects_phase3d_maturity(self):
        expected = {
            "exact_cas_identification": "CONFIRMED",
            "exact_dde_identification": "CONFIRMED",
            "exact_egs_identification": "CONFIRMED",
            "transmission_prnd_tool32": "CONFIRMED",
            "brake": "OBSERVED",
            "kl15": "CONFIRMED",
            "engine_speed": "CONFIRMED",
            "engine_running_detection_algorithm": "NOT_YET_VALIDATED",
            "terminal_50_start_request": "UNKNOWN",
            "oem_start_authorization": "UNKNOWN",
            "stop_strategy": "UNKNOWN",
            "can_identifiers": "UNKNOWN",
        }
        for item, status in expected.items():
            self.assertEqual(status, self.checklist["items"][item]["status"])

    def test_generic_engine_csv_import_uses_consecutive_samples_and_hysteresis(self):
        evidence = load_json_object(EXAMPLE_EVIDENCE_PATH)
        evidence_ids = validate_evidence_index(
            evidence, evidence_directory=EXAMPLE_EVIDENCE_PATH.parent
        )
        timeline = build_timeline(
            EXAMPLE_ENGINE_INPUT,
            load_json_object(EXAMPLE_ENGINE_MAPPING),
            evidence_ids,
        )
        self.assertEqual("CRANKING", timeline["samples"][4]["observed_state"])
        self.assertEqual("CRANKING", timeline["samples"][5]["observed_state"])
        self.assertEqual("RUNNING", timeline["samples"][6]["observed_state"])
        self.assertEqual("CRANKING_TO_RUNNING", timeline["samples"][6]["transition"])
        self.assertTrue(timeline["candidate_thresholds"]["candidate_only"])

    def test_single_sample_running_candidate_is_rejected(self):
        evidence = load_json_object(EXAMPLE_EVIDENCE_PATH)
        evidence_ids = validate_evidence_index(
            evidence, evidence_directory=EXAMPLE_EVIDENCE_PATH.parent
        )
        mapping = load_json_object(EXAMPLE_ENGINE_MAPPING)
        mapping["candidate_thresholds"]["consecutive_samples"] = 1
        with self.assertRaisesRegex(ValidationError, "expected integer >= 2"):
            build_timeline(EXAMPLE_ENGINE_INPUT, mapping, evidence_ids)


if __name__ == "__main__":
    unittest.main()
