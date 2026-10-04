import re
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[2]
ERISIN = ROOT / "erisin"


class ReadOnlyContractTest(unittest.TestCase):
    def test_transport_interface_exposes_no_write_surface(self):
        source = (ERISIN / "android-app/app/src/main/java/com/synaro/bmwe9xcontrol/transport/VehicleTransport.java").read_text(encoding="utf-8")
        self.assertNotRegex(source, r"\b(?:send|write|transmit)\s*\(")

    def test_collectors_contain_no_device_mutation_commands(self):
        forbidden = (
            "adb remount", " shell setprop ", " settings put ", " pm install ",
            " pm uninstall ", " pm disable", " chmod ", " chown ", " logcat -c",
            " adb push ", " twai_transmit", "twai_node_transmit",
        )
        for path in (ERISIN / "tools").glob("*.ps1"):
            text = " " + path.read_text(encoding="utf-8").lower().replace("`n", " ") + " "
            for token in forbidden:
                self.assertNotIn(token, text, f"{token!r} found in {path.name}")

    def test_external_can_hypotheses_do_not_enter_app_sources(self):
        app = ERISIN / "android-app/app/src/main"
        combined = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in app.rglob("*") if path.is_file())
        self.assertNotIn("0x23A", combined)
        self.assertNotIn("0x2B4", combined)

    def test_proprietary_directories_are_ignored(self):
        ignore = (ROOT / ".gitignore").read_text(encoding="utf-8")
        for name in ("private-dumps", "device-backups", "vendor-apks", "decompiled-vendor", "firmware-images", "captures-private"):
            self.assertIn(f"erisin/{name}/", ignore)


if __name__ == "__main__":
    unittest.main()
