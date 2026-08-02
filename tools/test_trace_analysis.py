from __future__ import annotations

import io
import unittest

from analyze_trace_change import FrameKey, compare_traces, render_comparison
from canonical_trace import CanonicalFrame, parse_canonical_trace


def frame(identifier: int, data: bytes, timestamp_ms: int = 0) -> CanonicalFrame:
    return CanonicalFrame(timestamp_ms, identifier, identifier > 0x7FF, data)


class CanonicalTraceTests(unittest.TestCase):
    def test_reads_strict_canonical_trace(self) -> None:
        frames = parse_canonical_trace(
            io.StringIO(
                "timestamp_ms,identifier,extended,dlc,data_hex\n"
                "0,0x123,0,2,0AFF\n"
                "50,0x1FFFFF00,1,1,A5\n"
            )
        )

        self.assertEqual(len(frames), 2)
        self.assertEqual(frames[0].data, b"\x0A\xFF")
        self.assertEqual(frames[1].timestamp_ms, 50)
        self.assertTrue(frames[1].extended)

    def test_rejects_nonzero_first_timestamp(self) -> None:
        with self.assertRaisesRegex(ValueError, "first timestamp"):
            parse_canonical_trace(
                io.StringIO(
                    "timestamp_ms,identifier,extended,dlc,data_hex\n"
                    "1,0x123,0,1,00\n"
                )
            )

    def test_rejects_malformed_payload_and_identifier(self) -> None:
        with self.assertRaisesRegex(ValueError, "data length"):
            parse_canonical_trace(
                io.StringIO(
                    "timestamp_ms,identifier,extended,dlc,data_hex\n"
                    "0,0x123,0,2,AA\n"
                )
            )
        with self.assertRaisesRegex(ValueError, "identifier"):
            parse_canonical_trace(
                io.StringIO(
                    "timestamp_ms,identifier,extended,dlc,data_hex\n"
                    "0,0x800,0,1,AA\n"
                )
            )


class TraceAnalysisTests(unittest.TestCase):
    def test_finds_stable_changed_byte_and_bit(self) -> None:
        baseline = [frame(0x100, b"\x10\xAA"), frame(0x100, b"\x10\xAA", 10)]
        event = [frame(0x100, b"\x11\xAA"), frame(0x100, b"\x11\xAA", 10)]

        comparison = compare_traces(baseline, event)

        self.assertEqual(len(comparison.candidates), 1)
        candidate = comparison.candidates[0]
        self.assertEqual(candidate.key, FrameKey(0x100, False, 2))
        self.assertEqual(candidate.byte_index, 0)
        self.assertEqual(candidate.changed_bit_mask, 0x01)
        self.assertEqual(candidate.score, 1.0)

    def test_ignores_unchanged_distribution_and_reports_new_identifier(self) -> None:
        baseline = [frame(0x100, b"\x10"), frame(0x100, b"\x11", 10)]
        event = [
            frame(0x100, b"\x11"),
            frame(0x100, b"\x10", 10),
            frame(0x200, b"\xFF", 20),
        ]

        comparison = compare_traces(baseline, event)

        self.assertEqual(comparison.candidates, ())
        self.assertEqual(
            comparison.identifiers_only_in_event, (FrameKey(0x200, False, 1),)
        )

    def test_render_warns_that_candidates_are_hypotheses(self) -> None:
        comparison = compare_traces([frame(0x100, b"\x00")], [frame(0x100, b"\x08")])
        rendered = render_comparison(comparison)

        self.assertIn("0x08", rendered)
        self.assertIn("hypotheses only", rendered)


if __name__ == "__main__":
    unittest.main()
