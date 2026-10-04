#!/usr/bin/env python3
"""Summarize a read-only Erisin ADB profile without ingesting proprietary binaries."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path

KEYWORDS = (
    "can", "canbus", "mcu", "car", "vehicle", "event", "eventcenter", "xrc",
    "bmw", "decoder", "serial", "uart", "spi", "canbox", "dashboard", "cluster",
)


def read_text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def matching_lines(text: str, limit: int = 500) -> list[str]:
    pattern = re.compile("|".join(re.escape(word) for word in KEYWORDS), re.IGNORECASE)
    result: list[str] = []
    for line in text.splitlines():
        if pattern.search(line):
            result.append(line[:2000])
            if len(result) == limit:
                break
    return result


def prop(properties: str, key: str) -> str | None:
    match = re.search(rf"^\[{re.escape(key)}\]: \[(.*)\]$", properties, re.MULTILINE)
    return match.group(1) if match else None


def analyze(root: Path) -> dict[str, object]:
    properties = read_text(root / "getprop.txt")
    files = sorted(path for path in root.glob("*.txt") if not path.name.endswith(".error.txt"))
    return {
        "schema_version": 1,
        "classification": "READ_ONLY_DEVICE_EVIDENCE",
        "device": {
            "manufacturer": prop(properties, "ro.product.manufacturer"),
            "model": prop(properties, "ro.product.model"),
            "android_release": prop(properties, "ro.build.version.release"),
            "api_level": prop(properties, "ro.build.version.sdk"),
            "fingerprint": prop(properties, "ro.build.fingerprint"),
            "board_platform": prop(properties, "ro.board.platform"),
        },
        "current_versions": {
            "xrc": "REQUIRES_REVIEW_OF_MATCHES",
            "mcu": "REQUIRES_REVIEW_OF_MATCHES",
        },
        "matches": {path.name: matching_lines(read_text(path)) for path in files},
        "text_evidence_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in files
        },
        "warnings": [
            "Keyword matches are discovery leads, not protocol confirmation.",
            "Do not publish dumps before redacting identifiers and proprietary content.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("profile_directory", type=Path)
    args = parser.parse_args()
    root = args.profile_directory.resolve()
    if not root.is_dir():
        parser.error(f"not a directory: {root}")
    result = analyze(root)
    output = root / "analysis.json"
    output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
