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
    validate_cas_kl50_observation,
    validate_cas_kl50_timing_observation,
    validate_evidence_index,
    validate_prerequisites,
    validate_qualified_observations,
    validate_signal_source_qualifications,
    validate_synchronized_start_observation,
    validate_vehicle_profile,
)


DATA = ROOT / "vehicle-data"
EVIDENCE_PATH = DATA / "evidence" / "current-test-vehicle.evidence-index.json"
PROFILE_PATH = DATA / "profiles" / "current-test-vehicle.vehicle-profile.json"
CHECKLIST_PATH = DATA / "profiles" / "current-test-vehicle.remote-start-prerequisites.json"
IDENTIFICATION_PATH = DATA / "observations" / "current-test-vehicle-phase3d-identification-2026-08-09.json"
STATUS_PATH = DATA / "observations" / "current-test-vehicle-phase3d-status-observations-2026-08-09.json"
REAL_TIMELINE_PATH = DATA / "observations" / "current-test-vehicle-testo-oem-start-sequence-2026-08-09.json"
SIGNAL_SOURCES_PATH = DATA / "observations" / "current-test-vehicle-phase3d-signal-source-qualification-2026-08-09.json"
SYNCHRONIZED_START_PATH = DATA / "observations" / "current-test-vehicle-phase3d-synchronized-rpm-msa-start-2026-08-09.json"
KL50_IFH_PATH = DATA / "observations" / "current-test-vehicle-phase3d-cas-kl50-ifh-level1-2026-08-09.json"
KL50_IFH_LEVEL3_PATH = DATA / "observations" / "current-test-vehicle-phase3d-cas-kl50-ifh-level3-2026-08-09.json"
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
        self.signal_sources = load_json_object(SIGNAL_SOURCES_PATH)
        self.synchronized_start = load_json_object(SYNCHRONIZED_START_PATH)
        self.kl50_ifh = load_json_object(KL50_IFH_PATH)
        self.kl50_ifh_level3 = load_json_object(KL50_IFH_LEVEL3_PATH)
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
        validate_signal_source_qualifications(self.signal_sources, self.evidence_ids)
        validate_synchronized_start_observation(self.synchronized_start, self.evidence_ids)
        validate_cas_kl50_observation(self.kl50_ifh, self.evidence_ids)
        validate_cas_kl50_timing_observation(self.kl50_ifh_level3, self.evidence_ids)

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

    def test_klemmenstatus_kl50_sequence_is_confirmed_read_only_state(self):
        sequence = [
            self.status_by_id[f"cas.klemmenstatus.kl50.sequence.{index}.{phase}"]
            for index, phase in enumerate(("before", "active", "after"))
        ]
        self.assertEqual([69, 85, 69], [observation["raw_value"] for observation in sequence])
        self.assertEqual(
            ["KL50_OFF", "KL50_ON", "KL50_OFF"],
            [observation["interpreted_value"] for observation in sequence],
        )
        self.assertEqual([0, 1, 0], [(observation["raw_value"] >> 4) & 0b11 for observation in sequence])
        self.assertEqual([1, 1, 1], [(observation["raw_value"] >> 2) & 0b11 for observation in sequence])
        self.assertTrue(all(observation["qualification"] == "CONFIRMED" for observation in sequence))
        self.assertTrue(all(observation["precondition_eligibility"] == "PROHIBITED" for observation in sequence))

    def test_confirmed_kl50_state_cannot_be_promoted_to_actuation_meaning(self):
        entry = next(
            item for item in self.signal_sources["entries"]
            if item["qualification_id"] == "cas.tool32.klemmenstatus_kl50"
        )
        self.assertEqual("READ_ONLY_SIGNAL", entry["suitability"])
        self.assertEqual("PROHIBITED", entry["precondition_eligibility"])
        self.assertEqual({"KL50_OFF", "KL50_ON"}, set(entry["allowed_interpretations"]))
        self.assertTrue(
            {
                "REMOTE_START_COMMAND_MECHANISM",
                "OEM_START_REQUEST_SOURCE",
                "START_AUTHORIZATION",
                "CAN_IDENTIFIER",
                "DIAGNOSTIC_COMMAND",
                "IMMOBILIZER_BYPASS",
            }.issubset(set(entry["forbidden_inferences"]))
        )
        modified = copy.deepcopy(self.signal_sources)
        modified_entry = next(
            item for item in modified["entries"]
            if item["qualification_id"] == "cas.tool32.klemmenstatus_kl50"
        )
        modified_entry["allowed_interpretations"].append("REMOTE_START_COMMAND_MECHANISM")
        with self.assertRaisesRegex(ValidationError, "both allowed and forbidden"):
            validate_signal_source_qualifications(modified, self.evidence_ids)

    def test_ifh_level1_corroborates_kl50_without_timing_claim(self):
        self.assertEqual("CAS_KL50_OBSERVATION", self.kl50_ifh["artifact_type"])
        self.assertEqual("CAS.KLEMMENSTATUS.KL50", self.kl50_ifh["signal"])
        self.assertEqual({"decimal": 69, "hexadecimal": "0x45"}, self.kl50_ifh["observed_off_value"])
        self.assertEqual({"decimal": 85, "hexadecimal": "0x55"}, self.kl50_ifh["observed_on_value"])
        self.assertEqual([64, 65, 85, 69], [value["decimal"] for value in self.kl50_ifh["trace_values"]])
        self.assertTrue(self.kl50_ifh["on_value_repeated_consecutively"])
        self.assertEqual(["OFF", "ON", "OFF"], self.kl50_ifh["observed_transition"])
        self.assertEqual("UNAVAILABLE", self.kl50_ifh["timestamp_basis"])
        self.assertIsNone(self.kl50_ifh["duration_us"])
        self.assertFalse(self.kl50_ifh["raw_trace_available_in_repository"])

    def test_ifh_level1_cannot_claim_kl50_duration(self):
        modified = copy.deepcopy(self.kl50_ifh)
        modified["duration_us"] = 750000
        with self.assertRaisesRegex(ValidationError, "cannot claim timestamps or duration"):
            validate_cas_kl50_observation(modified, self.evidence_ids)

    def test_ifh_evidence_cannot_drop_command_and_bypass_prohibitions(self):
        modified = copy.deepcopy(self.kl50_ifh)
        modified["forbidden_inferences"].remove("CAS_COMMAND")
        with self.assertRaisesRegex(ValidationError, "command and bypass inferences"):
            validate_cas_kl50_observation(modified, self.evidence_ids)

    def test_ifh_level3_preserves_bounded_kl50_duration(self):
        observation = self.kl50_ifh_level3
        self.assertEqual("CAS_KL50_TIMING_OBSERVATION", observation["artifact_type"])
        self.assertEqual(3, observation["source"]["trace_level"])
        self.assertEqual(
            [
                "2026-08-09T20:47:20.578+02:00",
                "2026-08-09T20:47:20.632+02:00",
                "2026-08-09T20:47:21.352+02:00",
                "2026-08-09T20:47:21.405+02:00",
            ],
            [sample["timestamp"] for sample in observation["boundary_samples"]],
        )
        self.assertEqual([65, 85, 85, 69], [sample["raw_value"]["decimal"] for sample in observation["boundary_samples"]])
        self.assertEqual(15, observation["consecutive_on_samples"])
        self.assertEqual({"minimum": 46, "maximum": 58, "nominal": 50}, observation["sampling_period_approx_ms"])
        self.assertEqual(20, observation["sampling_frequency_approx_hz"])
        self.assertEqual(720, observation["duration_min_ms"])
        self.assertEqual(827, observation["duration_max_ms"])
        self.assertEqual(774, observation["duration_estimate_ms"])
        self.assertEqual(50, observation["timestamp_resolution_approx_ms"])
        self.assertEqual("OBSERVED_BOUNDED", observation["duration_status"])
        self.assertEqual("CONFIRMED_FROM_LEVEL3_TRACE", observation["qualification"])
        self.assertEqual("PENDING", observation["rpm_alignment_status"])

    def test_ifh_level3_duration_bounds_are_recomputed_from_timestamps(self):
        for field, value in (
            ("duration_min_ms", 721),
            ("duration_max_ms", 826),
            ("duration_estimate_ms", 773),
        ):
            modified = copy.deepcopy(self.kl50_ifh_level3)
            modified[field] = value
            with self.assertRaisesRegex(ValidationError, field):
                validate_cas_kl50_timing_observation(modified, self.evidence_ids)

    def test_ifh_level3_estimate_cannot_be_promoted_to_exact_duration(self):
        modified = copy.deepcopy(self.kl50_ifh_level3)
        modified["duration_exact_ms"] = 774
        with self.assertRaisesRegex(ValidationError, "exact KL50 duration"):
            validate_cas_kl50_timing_observation(modified, self.evidence_ids)

    def test_ifh_level3_cannot_drop_non_actuation_inferences(self):
        modified = copy.deepcopy(self.kl50_ifh_level3)
        modified["forbidden_inferences"].remove("OEM_START_REQUEST")
        with self.assertRaisesRegex(ValidationError, "authorization must remain forbidden"):
            validate_cas_kl50_timing_observation(modified, self.evidence_ids)

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

        p_numeric = self.status_by_id["egs.tool32.position.p.numeric"]
        self.assertEqual(8, p_numeric["raw_value"])
        self.assertEqual("P", p_numeric["interpreted_value"])
        self.assertEqual("OKAY", self.status_by_id["egs.tool32.position.job_status"]["raw_value"])

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
            "kl50": "CONFIRMED",
            "kl50_oem_start_transition": "CONFIRMED",
            "kl50_duration": "OBSERVED_BOUNDED",
            "kl50_rpm_temporal_alignment": "PENDING",
            "engine_speed": "CONFIRMED",
            "stopped_cranking_running_observations": "CONFIRMED",
            "engine_running_detection_algorithm": "NOT_YET_VALIDATED",
            "msa_start_correlated_bits": "OBSERVED",
            "msa_bit_functional_meaning": "UNKNOWN",
            "terminal_50_start_request": "UNKNOWN",
            "cas_start_button_request": "UNKNOWN",
            "oem_start_request_source": "UNKNOWN",
            "remote_start_actuation_mechanism": "UNKNOWN",
            "oem_start_authorization": "UNKNOWN",
            "stop_strategy": "UNKNOWN",
            "can_identifiers": "UNKNOWN",
        }
        for item, status in expected.items():
            self.assertEqual(status, self.checklist["items"][item]["status"])

    def test_signal_source_suitability_is_usage_specific(self):
        entries = {
            entry["qualification_id"]: entry for entry in self.signal_sources["entries"]
        }
        self.assertEqual("CANDIDATE", entries["egs.tool32.selector_position"]["suitability"])
        self.assertEqual("UNTRUSTED", entries["egs.ista.transmission_position"]["suitability"])
        self.assertEqual("NOT_SUITABLE_FOR_PN", entries["egs.ista.actual_gear_for_pn"]["suitability"])
        self.assertEqual("CANDIDATE", entries["cas.tool32.klemmenstatus_kl15"]["suitability"])
        self.assertEqual("NOT_SUITABLE", entries["cas.tool32.klemmenstatus_engine_running"]["suitability"])
        self.assertEqual("NOT_FUNCTIONALLY_IDENTIFIED", entries["dde.status_msa.correlated_bitfields"]["suitability"])

    def test_kl15_abschaltung_fields_are_not_live_signal_sources(self):
        for observation_id in (
            "cas.kl15_abschaltung.brake_active",
            "cas.kl15_abschaltung.selector_not_p_active",
        ):
            observation = self.status_by_id[observation_id]
            self.assertEqual(0, observation["raw_value"])
            self.assertEqual("NOT_SUITABLE_AS_LIVE_SIGNAL", observation["interpreted_value"])
            self.assertEqual("PROHIBITED", observation["precondition_eligibility"])

    def test_synchronized_bitfields_preserve_decimal_hex_binary_and_changed_bits(self):
        bit_events = {
            (event["signal"], event["timestamp_us"]): event
            for event in self.synchronized_start["events"]
            if event["event_type"] == "BITFIELD_CHANGE"
        }
        msaav = bit_events[("STAT_STAT_MSAAV", 9365000)]
        self.assertEqual(
            {"decimal": 1537, "hexadecimal": "0x601", "binary": "0b11000000001"},
            msaav["previous_value"],
        )
        self.assertEqual(
            {"decimal": 1569, "hexadecimal": "0x621", "binary": "0b11000100001"},
            msaav["value"],
        )
        self.assertEqual(32, msaav["delta_decimal"])
        self.assertEqual([5], msaav["changed_bits"])
        self.assertEqual([2], bit_events[("STAT_STAT_MSAAA", 9365000)]["changed_bits"])
        self.assertEqual([2], bit_events[("STAT_STAT_MSAEV", 9507000)]["changed_bits"])

    def test_first_msa_change_precedes_first_nonzero_rpm_by_62_ms(self):
        first_bit = min(
            event["timestamp_us"] for event in self.synchronized_start["events"]
            if event["event_type"] == "BITFIELD_CHANGE"
        )
        first_rpm = min(
            event["timestamp_us"] for event in self.synchronized_start["events"]
            if event["event_type"] == "RPM_SAMPLE" and event["rpm"] > 0
        )
        self.assertEqual(62000, first_rpm - first_bit)

    def test_temporal_correlation_cannot_assign_functional_meaning(self):
        self.assertEqual("UNKNOWN", self.synchronized_start["functional_identification"])
        bit_events = [
            event for event in self.synchronized_start["events"]
            if event["event_type"] == "BITFIELD_CHANGE"
        ]
        self.assertTrue(all(event["functional_meaning"] is None for event in bit_events))
        modified = copy.deepcopy(self.synchronized_start)
        modified["events"][0]["functional_meaning"] = "STARTER_REQUEST"
        with self.assertRaisesRegex(ValidationError, "temporal correlation cannot assign meaning"):
            validate_synchronized_start_observation(modified, self.evidence_ids)

    def test_invalid_bitfield_representation_or_changed_bit_is_rejected(self):
        modified_hex = copy.deepcopy(self.synchronized_start)
        modified_hex["events"][0]["value"]["hexadecimal"] = "0x000"
        with self.assertRaisesRegex(ValidationError, "inconsistent representation"):
            validate_synchronized_start_observation(modified_hex, self.evidence_ids)
        modified_bit = copy.deepcopy(self.synchronized_start)
        modified_bit["events"][0]["changed_bits"] = [4]
        with self.assertRaisesRegex(ValidationError, "inconsistent XOR bit list"):
            validate_synchronized_start_observation(modified_bit, self.evidence_ids)

    def test_msaea_constant_does_not_confirm_start_request(self):
        msaea = self.synchronized_start["constant_bitfields"][0]
        self.assertEqual("STAT_STAT_MSAEA", msaea["signal"])
        self.assertEqual(11, msaea["value"]["decimal"])
        self.assertEqual("OBSERVED_NO_TRANSITION", msaea["suitability"])
        self.assertIsNone(msaea["functional_meaning"])

    def test_unidentified_msa_source_forbids_start_semantics(self):
        entry = next(
            item for item in self.signal_sources["entries"]
            if item["qualification_id"] == "dde.status_msa.correlated_bitfields"
        )
        self.assertEqual([], entry["allowed_interpretations"])
        self.assertEqual(
            {"KL50", "STARTER_REQUEST", "START_AUTHORIZATION", "CAS_START_REQUEST"},
            set(entry["forbidden_inferences"]),
        )

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
