#!/usr/bin/env python3
"""Capture a bounded classic-CAN trace through a documented listen-only backend."""

from __future__ import annotations

import argparse
import math
import os
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Protocol

from canonical_trace import CanonicalFrame, write_canonical_csv
from import_can_trace import CanMessage, canonicalize_messages


SUPPORTED_INTERFACES = ("pcan", "slcan")


class ReceiveBus(Protocol):
    channel_info: str

    def recv(self, timeout: float | None = None) -> CanMessage | None: ...

    def shutdown(self) -> None: ...


@dataclass(frozen=True)
class CaptureSummary:
    frame_count: int
    unique_identifier_count: int
    timestamp_span_ms: int
    passive_evidence: str


def _positive_integer(text: str) -> int:
    value = int(text, 10)
    if value <= 0:
        raise argparse.ArgumentTypeError("value must be greater than zero")
    return value


def _positive_float(text: str) -> float:
    value = float(text)
    if not math.isfinite(value) or value <= 0.0:
        raise argparse.ArgumentTypeError("value must be finite and greater than zero")
    return value


def passive_bus_options(interface: str, can_module: Any) -> tuple[dict[str, Any], str]:
    if interface == "pcan":
        return (
            {"state": can_module.BusState.PASSIVE},
            "pcan BusState.PASSIVE confirmed",
        )
    if interface == "slcan":
        return (
            {"listen_only": True},
            "slcan listen_only requested",
        )
    raise ValueError(
        f"interface {interface!r} has no qualified listen-only configuration; "
        f"supported interfaces: {', '.join(SUPPORTED_INTERFACES)}"
    )


def open_passive_bus(
    can_module: Any,
    interface: str,
    channel: str,
    bitrate: int,
) -> tuple[ReceiveBus, str]:
    options, evidence = passive_bus_options(interface, can_module)
    try:
        bus = can_module.Bus(
            interface=interface,
            channel=channel,
            bitrate=bitrate,
            **options,
        )
    except Exception as error:
        raise RuntimeError(f"unable to open CAN interface in listen-only mode: {error}") from error

    if interface == "pcan":
        try:
            passive = bus.state == can_module.BusState.PASSIVE
        except Exception as error:
            bus.shutdown()
            raise RuntimeError("PCAN driver did not expose its bus state") from error
        if not passive:
            bus.shutdown()
            raise RuntimeError("PCAN driver did not confirm passive bus state")

    return bus, evidence


def capture_frames(
    bus: ReceiveBus,
    duration_seconds: float,
    maximum_frames: int,
    clock: Callable[[], float] = time.monotonic,
) -> list[CanonicalFrame]:
    if not math.isfinite(duration_seconds) or duration_seconds <= 0.0:
        raise ValueError("capture duration must be finite and greater than zero")
    if maximum_frames <= 0:
        raise ValueError("maximum frame count must be greater than zero")

    messages: list[CanMessage] = []
    started_at = clock()
    previous_now = started_at

    while True:
        now = clock()
        if now < previous_now:
            raise RuntimeError("monotonic capture clock moved backwards")
        previous_now = now
        remaining = duration_seconds - (now - started_at)
        if remaining <= 0.0:
            break
        if len(messages) >= maximum_frames:
            raise ValueError("maximum frame count reached before capture duration elapsed")

        message = bus.recv(timeout=min(0.25, remaining))
        if message is not None:
            messages.append(message)

    return canonicalize_messages(messages, maximum_frames)


def summarize_capture(
    frames: list[CanonicalFrame], passive_evidence: str
) -> CaptureSummary:
    if not frames:
        raise ValueError("trace contains no CAN frames")
    return CaptureSummary(
        frame_count=len(frames),
        unique_identifier_count=len(
            {(frame.identifier, frame.extended) for frame in frames}
        ),
        timestamp_span_ms=frames[-1].timestamp_ms,
        passive_evidence=passive_evidence,
    )


def write_trace_atomically(
    output_path: Path,
    frames: list[CanonicalFrame],
    overwrite: bool = False,
) -> None:
    if output_path.exists() and not overwrite:
        raise FileExistsError(f"output already exists: {output_path}")
    output_path.parent.mkdir(parents=True, exist_ok=True)

    temporary_path: Path | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w",
            encoding="utf-8",
            newline="",
            dir=output_path.parent,
            prefix=f".{output_path.name}.",
            suffix=".tmp",
            delete=False,
        ) as output:
            temporary_path = Path(output.name)
            write_canonical_csv(frames, output)
        if output_path.exists() and not overwrite:
            raise FileExistsError(f"output already exists: {output_path}")
        os.replace(temporary_path, output_path)
        temporary_path = None
    finally:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)


def capture_to_file(
    can_module: Any,
    interface: str,
    channel: str,
    bitrate: int,
    duration_seconds: float,
    maximum_frames: int,
    output_path: Path,
    overwrite: bool = False,
    clock: Callable[[], float] = time.monotonic,
) -> CaptureSummary:
    bus, evidence = open_passive_bus(can_module, interface, channel, bitrate)
    try:
        frames = capture_frames(bus, duration_seconds, maximum_frames, clock)
    finally:
        bus.shutdown()

    write_trace_atomically(output_path, frames, overwrite)
    return summarize_capture(frames, evidence)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description=(
            "Capture classic CAN frames with a documented listen-only python-can "
            "backend and write the project's canonical CSV format."
        )
    )
    parser.add_argument("--interface", choices=SUPPORTED_INTERFACES, required=True)
    parser.add_argument("--channel", required=True, help="backend-specific channel")
    parser.add_argument("--bitrate", type=_positive_integer, required=True)
    parser.add_argument("--duration", type=_positive_float, default=10.0)
    parser.add_argument("--max-frames", type=_positive_integer, default=250_000)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="replace an existing output file after a successful capture",
    )
    return parser


def main() -> int:
    arguments = build_parser().parse_args()
    try:
        import can  # type: ignore[import-not-found]

        summary = capture_to_file(
            can,
            arguments.interface,
            arguments.channel,
            arguments.bitrate,
            arguments.duration,
            arguments.max_frames,
            arguments.output,
            arguments.overwrite,
        )
    except ImportError:
        print(
            "error: python-can is required; run: "
            "python -m pip install -r requirements-tools.txt"
        )
        return 1
    except (OSError, RuntimeError, ValueError) as error:
        print(f"error: {error}")
        return 1

    print(f"Listen-only evidence: {summary.passive_evidence}")
    print(
        f"Captured {summary.frame_count} classic CAN frame(s), "
        f"{summary.unique_identifier_count} identifier(s), "
        f"timestamp span {summary.timestamp_span_ms} ms"
    )
    print(f"Canonical trace: {arguments.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
