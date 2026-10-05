# Erisin investigation architecture

## Scope

This subproject starts from observable evidence, not from the assumption that
Android is electrically attached to BMW K-CAN. The only architecture presently
safe to draw is:

```text
BMW E90 vehicle network
        |
        | vehicle data reaches the Erisin display (owner observation: CONFIRMED)
        v
[CAN transceiver / CAN decoder / MCU: exact topology UNKNOWN]
        |
        | UART / SPI / USB / Binder / native IPC: UNKNOWN
        v
[Android service or native library: UNKNOWN]
        |
        v
[existing dashboard APK: package UNKNOWN]
```

The next ADB collection must replace every `UNKNOWN` edge with direct evidence
or leave it unknown.

## Evidence classification

| Link or fact | Classification | Basis |
|---|---|---|
| The ES3360I shows RPM, speed, brake and lighting data | CONFIRMED | Direct owner observation on the target unit |
| Some vehicle-to-head-unit path exists | CONFIRMED | Those live values cannot be rendered without a data source |
| The path is BMW K-CAN → CAN box → XRC MCU → Android | HYPOTHESIS | Plausible topology, not yet inspected on this unit |
| XRC exposes a Binder service or serial device | HYPOTHESIS | Must be established from the fresh profile/vendor APKs |
| Choiceway EventCenter can own `/dev/ttyHS1` and expose AIDL | COMMUNITY REPORT | Proven on a different QCM6125/HiWorld/RLC unit, not XRC |
| KSW MCU tools apply to XRC | REJECTED ASSUMPTION | Different vendor/protocol until binary evidence proves otherwise |
| Android can transmit arbitrary raw CAN through XRC | UNKNOWN | Central question; no target-device evidence yet |

## Android layers

```text
UI (Home / Lights / Ghost / Shows / Music / Diagnostics / Settings)
                   |
VehicleRepository + evidence log
                   |
VehicleTransport (read-only interface; no transmit method)
     +-------------+------------------+
     |             |                  |
MockTransport  ReplayTransport  XrcMcuTransport
SIMULATED      REPLAY           DISCOVERY_ONLY / unavailable
```

Vehicle telemetry remains read-only. Active diagnostic work uses a separate
interface so a diagnostic job can never be confused with a raw CAN frame:

```text
LightShowEngine / GhostController / Manual Controls
                     |
                 FrmLightSink
                     |
              DiagnosticTransport
                     |
              UsbKdcanTransport
                     |
                EdiabasBridge
                     |
       EdiabasLib .NET Android runtime (not yet packaged)
                     |
             FTDI USB K+DCAN cable
                     |
               BMW D-CAN / FRM
```

`UsbKdcanTransport` detects FTDI devices and owns Android USB permission and
connection state. `UnavailableEdiabasBridge` is the production default in this
branch and fails every connection/job explicitly. This is intentional: current
EdiabasLib is a .NET for Android codebase and cannot be linked as a Java Maven
dependency. A GPL-compatible bridge build is still required before vehicle I/O.

The light-show parser and scheduler remain transport-independent.
`SimulatedLightSink` is immediately usable; `FrmLightSink` exists but accepts
only discovered, explicitly armed capabilities and only after the diagnostic
transport is connected. It never guesses a job or output name.

An eventual `ObdLinkCxBleTransport` can implement `DiagnosticTransport`
without changing the show engine and without consuming Wi-Fi needed by
Wireless CarPlay.

## Decision gate for the internal interface

- **Level A:** target-device evidence proves arbitrary raw CAN TX.
- **Level B:** target-device evidence proves only a constrained vendor command
  protocol.
- **Level C:** serious inspection establishes a receive-only internal path.

No level has been assigned. A method named `sendCanbusData` on another vendor's
unit is neither Level A evidence nor proof that XRC provides the same API.
