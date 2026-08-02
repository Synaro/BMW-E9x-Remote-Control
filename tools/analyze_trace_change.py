#!/usr/bin/env python3
"""Rank byte-level candidates that differ between two canonical CAN traces."""

from __future__ import annotations

import argparse
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

from canonical_trace import CanonicalFrame, load_canonical_trace


@dataclass(frozen=True, order=True)
class FrameKey:
    identifier: int
    extended: bool
    data_length: int


@dataclass(frozen=True)
class ByteCandidate:
    key: FrameKey
    byte_index: int
    baseline_mode: int
    event_mode: int
    changed_bit_mask: int
    distribution_distance: float
    baseline_stability: float
    event_stability: float
    score: float


@dataclass(frozen=True)
class TraceComparison:
    baseline_frame_count: int
    event_frame_count: int
    candidates: tuple[ByteCandidate, ...]
    identifiers_only_in_baseline: tuple[FrameKey, ...]
    identifiers_only_in_event: tuple[FrameKey, ...]


@dataclass
class _KeySummary:
    frame_count: int
    byte_histograms: list[Counter[int]]


def _summarize(frames: Sequence[CanonicalFrame]) -> dict[FrameKey, _KeySummary]:
    summaries: dict[FrameKey, _KeySummary] = {}
    for frame in frames:
        key = FrameKey(frame.identifier, frame.extended, len(frame.data))
        summary = summaries.get(key)
        if summary is None:
            summary = _KeySummary(0, [Counter() for _ in frame.data])
            summaries[key] = summary
        summary.frame_count += 1
        for byte_index, value in enumerate(frame.data):
            summary.byte_histograms[byte_index][value] += 1
    return summaries


def _mode(histogram: Counter[int]) -> tuple[int, float]:
    maximum_count = max(histogram.values())
    value = min(key for key, count in histogram.items() if count == maximum_count)
    return value, maximum_count / histogram.total()


def _distribution_distance(
    baseline: Counter[int], event: Counter[int]
) -> float:
    baseline_total = baseline.total()
    event_total = event.total()
    values = baseline.keys() | event.keys()
    return 0.5 * sum(
        abs(baseline[value] / baseline_total - event[value] / event_total)
        for value in values
    )


def compare_traces(
    baseline_frames: Sequence[CanonicalFrame],
    event_frames: Sequence[CanonicalFrame],
    minimum_distance: float = 0.25,
) -> TraceComparison:
    if not 0.0 <= minimum_distance <= 1.0:
        raise ValueError("minimum distance must be between zero and one")

    baseline = _summarize(baseline_frames)
    event = _summarize(event_frames)
    baseline_keys = set(baseline)
    event_keys = set(event)
    candidates: list[ByteCandidate] = []

    for key in sorted(baseline_keys & event_keys):
        baseline_summary = baseline[key]
        event_summary = event[key]
        for byte_index in range(key.data_length):
            baseline_histogram = baseline_summary.byte_histograms[byte_index]
            event_histogram = event_summary.byte_histograms[byte_index]
            baseline_mode, baseline_stability = _mode(baseline_histogram)
            event_mode, event_stability = _mode(event_histogram)
            if baseline_mode == event_mode:
                continue

            distance = _distribution_distance(baseline_histogram, event_histogram)
            if distance < minimum_distance:
                continue
            stability = (baseline_stability + event_stability) / 2.0
            candidates.append(
                ByteCandidate(
                    key=key,
                    byte_index=byte_index,
                    baseline_mode=baseline_mode,
                    event_mode=event_mode,
                    changed_bit_mask=baseline_mode ^ event_mode,
                    distribution_distance=distance,
                    baseline_stability=baseline_stability,
                    event_stability=event_stability,
                    score=distance * stability,
                )
            )

    candidates.sort(
        key=lambda candidate: (
            -candidate.score,
            -candidate.distribution_distance,
            candidate.key,
            candidate.byte_index,
        )
    )
    return TraceComparison(
        baseline_frame_count=len(baseline_frames),
        event_frame_count=len(event_frames),
        candidates=tuple(candidates),
        identifiers_only_in_baseline=tuple(sorted(baseline_keys - event_keys)),
        identifiers_only_in_event=tuple(sorted(event_keys - baseline_keys)),
    )


def _format_identifier(key: FrameKey) -> str:
    width = 8 if key.extended else 3
    frame_type = "ext" if key.extended else "std"
    return f"0x{key.identifier:0{width}X}/{frame_type}/dlc{key.data_length}"


def _format_keys(keys: Sequence[FrameKey]) -> str:
    return ", ".join(_format_identifier(key) for key in keys) or "none"


def render_comparison(comparison: TraceComparison, limit: int = 20) -> str:
    if limit <= 0:
        raise ValueError("candidate display limit must be greater than zero")

    lines = [
        f"baseline_frames={comparison.baseline_frame_count} event_frames={comparison.event_frame_count}",
        f"only_in_baseline={_format_keys(comparison.identifiers_only_in_baseline)}",
        f"only_in_event={_format_keys(comparison.identifiers_only_in_event)}",
        f"byte_candidates={len(comparison.candidates)}",
    ]
    if comparison.candidates:
        lines.append(
            "score  identifier              byte  baseline  event  xor   stable(base/event)"
        )
        for candidate in comparison.candidates[:limit]:
            lines.append(
                f"{candidate.score:0.3f}  {_format_identifier(candidate.key):<22} "
                f"{candidate.byte_index:>4}  0x{candidate.baseline_mode:02X}      "
                f"0x{candidate.event_mode:02X}   0x{candidate.changed_bit_mask:02X}  "
                f"{candidate.baseline_stability:>5.1%}/{candidate.event_stability:<5.1%}"
            )
    else:
        lines.append("No byte candidate crossed the configured threshold.")
    lines.append("Candidates are hypotheses only; repeat observations before decoding.")
    return "\n".join(lines)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Compare a baseline and one-event canonical CAN trace."
    )
    parser.add_argument("baseline", type=Path, help="baseline .cantrace.csv")
    parser.add_argument("event", type=Path, help="single-event .cantrace.csv")
    parser.add_argument(
        "--min-distance",
        type=float,
        default=0.25,
        help="minimum total-variation distance from 0 to 1",
    )
    parser.add_argument("--limit", type=int, default=20, help="maximum candidates shown")
    parser.add_argument(
        "--max-frames", type=int, default=1_000_000, help="per-trace frame limit"
    )
    return parser


def main() -> int:
    arguments = build_parser().parse_args()
    try:
        baseline = load_canonical_trace(arguments.baseline, arguments.max_frames)
        event = load_canonical_trace(arguments.event, arguments.max_frames)
        comparison = compare_traces(baseline, event, arguments.min_distance)
        print(render_comparison(comparison, arguments.limit))
    except (OSError, ValueError) as error:
        print(f"error: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
