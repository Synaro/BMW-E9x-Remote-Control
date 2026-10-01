import copy
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOLS = ROOT / "tools"
if str(TOOLS) not in sys.path:
    sys.path.insert(0, str(TOOLS))

from vehicle_data_validation import (
    ValidationError,
    load_json_object,
    validate_external_can_hypotheses,
)


CATALOG_PATH = ROOT / "vehicle-data" / "catalog" / "external-kcan-hypotheses.json"


class Phase3EKcanHypothesisTests(unittest.TestCase):
    def setUp(self):
        self.catalog = load_json_object(CATALOG_PATH)

    def test_external_hypotheses_remain_unvalidated_and_non_actionable(self):
        validate_external_can_hypotheses(self.catalog)
        self.assertEqual({"0x23A", "0x2B4"}, {item["can_id_hex"] for item in self.catalog["hypotheses"]})
        self.assertTrue(all(item["qualification"] == "EXTERNAL_UNVALIDATED" for item in self.catalog["hypotheses"]))
        self.assertTrue(all(item["precondition_eligibility"] == "PROHIBITED" for item in self.catalog["hypotheses"]))
        self.assertTrue(all(item["functional_meaning"] is None for item in self.catalog["hypotheses"]))

    def test_external_hypothesis_cannot_be_promoted_to_vehicle_fact(self):
        modified = copy.deepcopy(self.catalog)
        modified["hypotheses"][0]["qualification"] = "CONFIRMED"
        with self.assertRaisesRegex(ValidationError, "community claim cannot be promoted"):
            validate_external_can_hypotheses(modified)

        modified = copy.deepcopy(self.catalog)
        modified["hypotheses"][0]["observed_on_current_vehicle"] = True
        modified["hypotheses"][0]["functional_meaning"] = "REMOTE_LOCK_PRESS"
        with self.assertRaisesRegex(ValidationError, "no current-vehicle meaning"):
            validate_external_can_hypotheses(modified)

    def test_external_hypothesis_cannot_become_runtime_input_or_capture_filter(self):
        modified = copy.deepcopy(self.catalog)
        modified["hypotheses"][1]["runtime_use"] = "REMOTE_START_TRIGGER"
        with self.assertRaisesRegex(ValidationError, "runtime or acquisition use is forbidden"):
            validate_external_can_hypotheses(modified)

        modified = copy.deepcopy(self.catalog)
        modified["capture_policy"] = "CAPTURE_ONLY_LISTED_IDS"
        with self.assertRaisesRegex(ValidationError, "must not filter acquisition"):
            validate_external_can_hypotheses(modified)

    def test_external_hypothesis_requires_immutable_provenance(self):
        modified = copy.deepcopy(self.catalog)
        modified["source"]["commit_sha"] = "main"
        with self.assertRaisesRegex(ValidationError, "expected immutable Git SHA"):
            validate_external_can_hypotheses(modified)


if __name__ == "__main__":
    unittest.main()
