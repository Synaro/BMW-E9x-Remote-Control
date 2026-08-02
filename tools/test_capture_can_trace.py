from __future__ import annotations

import tempfile
import unittest
from dataclasses import dataclass
from pathlib import Path
from types import SimpleNamespace

from canonical_trace import load_canonical_trace
from capture_can_trace import (
    capture_frames,
    capture_to_file,
    open_passive_bus,
    passive_bus_options,
    write_trace_atomically,
)


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


class FakeClock:
    def __init__(self) -> None:
        self.now = 0.0

    def __call__(self) -> float:
        return self.now

    def advance(self, seconds: float) -> None:
        self.now += seconds


class FakeBus:
    def __init__(
        self,
        clock: FakeClock,
        messages: list[FakeMessage | None],
        state: object = "passive",
    ) -> None:
        self.clock = clock
        self.messages = list(messages)
        self.state = state
        self.channel_info = "fake channel"
        self.shutdown_count = 0

    def recv(self, timeout: float | None = None) -> FakeMessage | None:
        assert timeout is not None
        self.clock.advance(timeout)
        if self.messages:
            return self.messages.pop(0)
        return None

    def shutdown(self) -> None:
        self.shutdown_count += 1


class FakeCanModule:
    BusState = SimpleNamespace(PASSIVE="passive")

    def __init__(self, bus: FakeBus) -> None:
        self.bus = bus
        self.options: dict[str, object] | None = None

    def Bus(self, **options: object) -> FakeBus:
        self.options = options
        return self.bus


class CaptureCanTraceTests(unittest.TestCase):
    def test_selects_only_documented_listen_only_options(self) -> None:
        can_module = SimpleNamespace(BusState=SimpleNamespace(PASSIVE="passive"))
        self.assertEqual(
            passive_bus_options("pcan", can_module),
            ({"state": "passive"}, "pcan BusState.PASSIVE confirmed"),
        )
        self.assertEqual(
            passive_bus_options("slcan", can_module),
            ({"listen_only": True}, "slcan listen_only requested"),
        )
        with self.assertRaisesRegex(ValueError, "no qualified listen-only"):
            passive_bus_options("virtual", can_module)

    def test_opens_pcan_in_confirmed_passive_state(self) -> None:
        clock = FakeClock()
        bus = FakeBus(clock, [])
        can_module = FakeCanModule(bus)

        opened, evidence = open_passive_bus(
            can_module, "pcan", "PCAN_USBBUS1", 500_000
        )

        self.assertIs(opened, bus)
        self.assertEqual(evidence, "pcan BusState.PASSIVE confirmed")
        self.assertEqual(
            can_module.options,
            {
                "interface": "pcan",
                "channel": "PCAN_USBBUS1",
                "bitrate": 500_000,
                "state": "passive",
            },
        )

    def test_opens_slcan_with_driver_listen_only_option(self) -> None:
        clock = FakeClock()
        bus = FakeBus(clock, [])
        can_module = FakeCanModule(bus)

        opened, evidence = open_passive_bus(
            can_module, "slcan", "COM8", 500_000
        )

        self.assertIs(opened, bus)
        self.assertEqual(evidence, "slcan listen_only requested")
        self.assertEqual(
            can_module.options,
            {
                "interface": "slcan",
                "channel": "COM8",
                "bitrate": 500_000,
                "listen_only": True,
            },
        )

    def test_rejects_pcan_when_driver_does_not_confirm_passive_state(self) -> None:
        clock = FakeClock()
        bus = FakeBus(clock, [], state="active")
        with self.assertRaisesRegex(RuntimeError, "did not confirm"):
            open_passive_bus(
                FakeCanModule(bus), "pcan", "PCAN_USBBUS1", 500_000
            )
        self.assertEqual(bus.shutdown_count, 1)

    def test_captures_bounded_messages_and_normalizes_timestamps(self) -> None:
        clock = FakeClock()
        bus = FakeBus(
            clock,
            [FakeMessage(42.0), FakeMessage(42.125, arbitration_id=0x456)],
        )

        frames = capture_frames(bus, 0.5, 10, clock)

        self.assertEqual([frame.timestamp_ms for frame in frames], [0, 125])
        self.assertEqual([frame.identifier for frame in frames], [0x123, 0x456])

    def test_rejects_empty_capture(self) -> None:
        clock = FakeClock()
        with self.assertRaisesRegex(ValueError, "no CAN frames"):
            capture_frames(FakeBus(clock, []), 0.25, 10, clock)

    def test_rejects_frame_limit_before_duration(self) -> None:
        clock = FakeClock()
        bus = FakeBus(clock, [FakeMessage(1.0), FakeMessage(1.1)])
        with self.assertRaisesRegex(ValueError, "before capture duration"):
            capture_frames(bus, 1.0, 1, clock)

    def test_capture_closes_bus_and_writes_strict_trace(self) -> None:
        clock = FakeClock()
        bus = FakeBus(clock, [FakeMessage(10.0), FakeMessage(10.1)])
        can_module = FakeCanModule(bus)

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "capture.cantrace.csv"
            summary = capture_to_file(
                can_module,
                "pcan",
                "PCAN_USBBUS1",
                500_000,
                0.5,
                10,
                output,
                clock=clock,
            )
            frames = load_canonical_trace(output)

        self.assertEqual(bus.shutdown_count, 1)
        self.assertEqual(summary.frame_count, 2)
        self.assertEqual(summary.unique_identifier_count, 1)
        self.assertEqual(summary.timestamp_span_ms, 100)
        self.assertEqual(len(frames), 2)

    def test_capture_closes_bus_when_no_frames_are_received(self) -> None:
        clock = FakeClock()
        bus = FakeBus(clock, [])

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "empty.cantrace.csv"
            with self.assertRaisesRegex(ValueError, "no CAN frames"):
                capture_to_file(
                    FakeCanModule(bus),
                    "pcan",
                    "PCAN_USBBUS1",
                    500_000,
                    0.25,
                    10,
                    output,
                    clock=clock,
                )
            self.assertFalse(output.exists())

        self.assertEqual(bus.shutdown_count, 1)

    def test_atomic_writer_refuses_existing_output_by_default(self) -> None:
        clock = FakeClock()
        frames = capture_frames(FakeBus(clock, [FakeMessage(1.0)]), 0.25, 10, clock)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "existing.cantrace.csv"
            output.write_text("keep", encoding="utf-8")
            with self.assertRaises(FileExistsError):
                write_trace_atomically(output, frames)
            self.assertEqual(output.read_text(encoding="utf-8"), "keep")


if __name__ == "__main__":
    unittest.main()
