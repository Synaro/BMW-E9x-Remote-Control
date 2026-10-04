import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


MODULE_PATH = Path(__file__).parents[1] / "tools" / "analyze_erisin_profile.py"
SPEC = importlib.util.spec_from_file_location("analyze_erisin_profile", MODULE_PATH)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class ProfileAnalyzerTest(unittest.TestCase):
    def test_extracts_profile_and_keeps_matches_as_leads(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "getprop.txt").write_text(
                "[ro.product.manufacturer]: [Erisin]\n"
                "[ro.product.model]: [ES3360I]\n"
                "[ro.build.version.release]: [10]\n"
                "[ro.build.version.sdk]: [29]\n"
                "[ro.build.fingerprint]: [vendor/product/trinket:10/test]\n"
                "[ro.board.platform]: [trinket]\n",
                encoding="utf-8",
            )
            (root / "services.txt").write_text("eventcenter: candidate binder\n", encoding="utf-8")
            result = MODULE.analyze(root)
            self.assertEqual("ES3360I", result["device"]["model"])
            self.assertEqual("29", result["device"]["api_level"])
            self.assertEqual("REQUIRES_REVIEW_OF_MATCHES", result["current_versions"]["xrc"])
            self.assertIn("eventcenter: candidate binder", result["matches"]["services.txt"])

    def test_hashes_text_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "getprop.txt").write_text("", encoding="utf-8")
            result = MODULE.analyze(root)
            self.assertEqual(64, len(result["text_evidence_sha256"]["getprop.txt"]))


if __name__ == "__main__":
    unittest.main()
