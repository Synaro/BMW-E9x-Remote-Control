#!/usr/bin/env python3
"""Convert a python-can supported log into the project's canonical CSV format."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
from typing import Iterable, Protocol, Sequence

from canonical_trace import CanonicalFrame, write_canonical_csv


class CanMessage(Protocol):
    timestamp: float | None
    arbitration_id: int
    is_extended_id: bool
    is_error_frame: bool
    is_remote_frame: bool
    is_fd: bool
    dlc: int
    data: Sequence[int]


def canonicalize_messages(
    messages: Iterable[CanMessage], maximum_frames: int
) -> list[CanonicalFrame]:
    if maximum_frames <= 0:
        raise ValueError("maximum frame count must be greater than zero")

    frames: list[CanonicalFrame] = []
    first_timestamp: float | None = None
    previous_timestamp: float | None = None

    for message in messages:
        if len(frames) >= maximum_frames:
            raise ValueError("maximum frame count exceeded")
        if message.timestamp is None or not math.isfinite(message.timestamp):
            raise ValueError("message timestamp is missing or invalid")
        if previous_timestamp is not None and message.timestamp < previous_timestamp:
            raise ValueError("message timestamps are not monotonic")
        if message.is_error_frame or message.is_remote_frame or message.is_fd:
            raise ValueError("only classic CAN data frames are supported")

        maximum_identifier = 0x1FFFFFFF if message.is_extended_id else 0x7FF
        if message.arbitration_id < 0 or message.arbitration_id > maximum_identifier:
            raise ValueError("CAN identifier is outside its declared range")

        data = bytes(message.data)
        if message.dlc != len(data) or len(data) > 8:
            raise ValueError("CAN data length is inconsistent")

        if first_timestamp is None:
            first_timestamp = message.timestamp
        relative_ms = round((message.timestamp - first_timestamp) * 1000.0)
        if relative_ms < 0 or relative_ms > 0xFFFFFFFF:
            raise ValueError("relative timestamp is outside uint32 range")

        frames.append(
            CanonicalFrame(
                timestamp_ms=relative_ms,
                identifier=message.arbitration_id,
                extended=message.is_extended_id,
                data=data,
            )
        )
        previous_timestamp = message.timestamp

    if not frames:
        raise ValueError("trace contains no CAN frames")
    return frames

def import_trace(input_path: Path, output_path: Path, maximum_frames: int) -> int:
    try:
        import can  # type: ignore[import-not-found]
    except ImportError as error:
        raise RuntimeError(
            "python-can is required; run: python -m pip install -r requirements-tools.txt"
        ) from error

    if input_path.resolve() == output_path.resolve():
        raise ValueError("input and output paths must be different")

    with can.LogReader(input_path) as reader:
        frames = canonicalize_messages(reader, maximum_frames)

    with output_path.open("w", encoding="utf-8", newline="") as output:
        write_canonical_csv(frames, output)
    return len(frames)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Convert ASC, BLF, CSV, LOG, MF4 or TRC CAN logs to canonical CSV."
    )
    parser.add_argument("input", type=Path, help="source CAN log")
    parser.add_argument("output", type=Path, help="destination .cantrace.csv file")
    parser.add_argument(
        "--max-frames", type=int, default=1_000_000, help="hard frame-count limit"
    )
    return parser


def main() -> int:
    arguments = build_parser().parse_args()
    try:
        count = import_trace(arguments.input, arguments.output, arguments.max_frames)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}")
        return 1
    print(f"Imported {count} classic CAN frame(s) into {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
