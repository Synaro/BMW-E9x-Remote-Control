from __future__ import annotations

import io
import unittest
from dataclasses import dataclass

from import_can_trace import canonicalize_messages, write_canonical_csv


@dataclass
class FakeMessage:
    timestamp: float | None
    arbitration_id: int = 0x123
    is_extended_id: bool = False
    is_error_frame: bool = False
    is_remote_frame: bool = False
    is_fd: bool = False
    dlc: int = 2
    data: bytes = b"\x0A\xFF"


class ImportCanTraceTests(unittest.TestCase):
    def test_normalizes_timestamps_and_writes_canonical_csv(self) -> None:
        frames = canonicalize_messages(
            [FakeMessage(42.0), FakeMessage(42.125, arbitration_id=0x456)], 10
        )
        output = io.StringIO()
        write_canonical_csv(frames, output)

        self.assertEqual(frames[0].timestamp_ms, 0)
        self.assertEqual(frames[1].timestamp_ms, 125)
        self.assertEqual(
            output.getvalue(),
            "timestamp_ms,identifier,extended,dlc,data_hex\n"
            "0,0x123,0,2,0AFF\n"
            "125,0x456,0,2,0AFF\n",
        )

    def test_rejects_non_monotonic_timestamps(self) -> None:
        with self.assertRaisesRegex(ValueError, "not monotonic"):
            canonicalize_messages([FakeMessage(2.0), FakeMessage(1.0)], 10)

    def test_rejects_non_classic_frames(self) -> None:
        with self.assertRaisesRegex(ValueError, "classic CAN"):
            canonicalize_messages([FakeMessage(0.0, is_fd=True)], 10)

    def test_enforces_frame_limit(self) -> None:
        with self.assertRaisesRegex(ValueError, "maximum frame count"):
            canonicalize_messages([FakeMessage(0.0), FakeMessage(1.0)], 1)

    def test_rejects_empty_trace(self) -> None:
        with self.assertRaisesRegex(ValueError, "no CAN frames"):
            canonicalize_messages([], 10)


if __name__ == "__main__":
    unittest.main()
