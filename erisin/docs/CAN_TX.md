# Vehicle transmit investigation

Status: **disabled and not implemented**.

The Android interface intentionally has no transmit method. RX and TX counters
are separate; every existing transport reports TX = 0.

## Evidence required before any implementation

1. exact XRC OS/MCU/package/library versions and hashes;
2. proven Android endpoint and owner process;
3. proven distinction between radio/MCU commands and vehicle-bus commands;
4. framing, checksum, destination, timeout and response behavior;
5. a bench-observable end-to-end path with no vehicle attached;
6. narrowly scoped allowlist for a non-critical body/light function;
7. default-off developer mode and persistent audit log;
8. watchdog/expiry that returns control to the normal BMW system;
9. stationary vehicle procedure and battery-voltage supervision.

## Forbidden initial targets

DME, DSC, ABS, MRS/airbag, steering, transmission, brakes, accelerator,
immobilizer/EWS and sensitive CAS behavior remain outside the active scope.

## Interpretation guardrails

- Choiceway `sendCanbusData` on a different unit is not an XRC capability.
- A writable serial port is not proof of a CAN transmit path.
- A vendor predefined command path would be Level B, not raw CAN Level A.
- Diagnostic FRM jobs are not equivalent to ordinary CAN broadcasts.
- No frame, address, checksum or diagnostic session is guessed.
