"""Strict reader and writer for the host-side canonical classic-CAN format."""

from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, TextIO


HEADER = "timestamp_ms,identifier,extended,dlc,data_hex"
UINT32_MAXIMUM = 0xFFFFFFFF
STANDARD_IDENTIFIER_MAXIMUM = 0x7FF
EXTENDED_IDENTIFIER_MAXIMUM = 0x1FFFFFFF


@dataclass(frozen=True)
class CanonicalFrame:
    timestamp_ms: int
    identifier: int
    extended: bool
    data: bytes


def _parse_decimal(text: str) -> int:
    if not text or not text.isascii() or not text.isdigit():
        raise ValueError("invalid decimal integer")
    return int(text, 10)


def _parse_identifier(text: str) -> int:
    if len(text) <= 2 or text[:2].lower() != "0x":
        raise ValueError("identifier must use hexadecimal 0x notation")
    digits = text[2:]
    if not digits.isascii() or any(
        character not in "0123456789abcdefABCDEF" for character in digits
    ):
        raise ValueError("invalid hexadecimal identifier")
    return int(digits, 16)


def _parse_extended(text: str) -> bool:
    if text in ("1", "true"):
        return True
    if text in ("0", "false"):
        return False
    raise ValueError("extended must be 0 or 1")


def _parse_data(text: str, data_length: int) -> bytes:
    if data_length > 8 or len(text) != data_length * 2:
        raise ValueError("CAN data length is inconsistent")
    if any(character not in "0123456789abcdefABCDEF" for character in text):
        raise ValueError("CAN data is not hexadecimal")
    return bytes.fromhex(text)


def parse_canonical_trace(
    source: TextIO, maximum_frames: int = 1_000_000
) -> list[CanonicalFrame]:
    if maximum_frames <= 0:
        raise ValueError("maximum frame count must be greater than zero")

    header = source.readline()
    if header == "":
        raise ValueError("trace is empty")
    if header.rstrip("\r\n").removeprefix("\ufeff") != HEADER:
        raise ValueError("line 1: invalid header")

    frames: list[CanonicalFrame] = []
    for line_number, raw_line in enumerate(source, start=2):
        line = raw_line.rstrip("\r\n")
        if not line:
            continue
        if len(frames) >= maximum_frames:
            raise ValueError(f"line {line_number}: maximum frame count exceeded")

        fields = line.split(",")
        if len(fields) != 5:
            raise ValueError(f"line {line_number}: invalid field count")

        try:
            timestamp_ms = _parse_decimal(fields[0])
            identifier = _parse_identifier(fields[1])
            extended = _parse_extended(fields[2])
            data_length = _parse_decimal(fields[3])
            data = _parse_data(fields[4], data_length)
        except ValueError as error:
            raise ValueError(f"line {line_number}: {error}") from error

        if timestamp_ms > UINT32_MAXIMUM:
            raise ValueError(f"line {line_number}: timestamp is outside uint32 range")
        maximum_identifier = (
            EXTENDED_IDENTIFIER_MAXIMUM if extended else STANDARD_IDENTIFIER_MAXIMUM
        )
        if identifier > maximum_identifier:
            raise ValueError(
                f"line {line_number}: CAN identifier is outside its declared range"
            )
        if not frames and timestamp_ms != 0:
            raise ValueError(f"line {line_number}: first timestamp must be zero")
        if frames and timestamp_ms < frames[-1].timestamp_ms:
            raise ValueError(f"line {line_number}: timestamps must be monotonic")

        frames.append(CanonicalFrame(timestamp_ms, identifier, extended, data))

    if not frames:
        raise ValueError("trace contains no CAN frames")
    return frames


def load_canonical_trace(
    path: Path, maximum_frames: int = 1_000_000
) -> list[CanonicalFrame]:
    with path.open("r", encoding="utf-8", newline="") as source:
        return parse_canonical_trace(source, maximum_frames)


def write_canonical_csv(frames: Iterable[CanonicalFrame], output: TextIO) -> None:
    writer = csv.writer(output, lineterminator="\n")
    writer.writerow(HEADER.split(","))
    for frame in frames:
        writer.writerow(
            [
                frame.timestamp_ms,
                f"0x{frame.identifier:X}",
                1 if frame.extended else 0,
                len(frame.data),
                frame.data.hex().upper(),
            ]
        )
