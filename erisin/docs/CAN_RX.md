# Vehicle receive path

## Confirmed observation

The target Erisin UI displays live RPM, vehicle speed, brake and lighting
states. This confirms received vehicle-derived information, but not raw CAN
availability to third-party Android applications.

The dashboard could receive:

- decoded values through broadcasts or Binder;
- a proprietary MCU/CAN-box digest;
- raw frames;
- shared-memory/native callbacks;
- some combination of these.

## Correlation procedure

Run `capture_vehicle_events.ps1` and perform each prompted action repeatedly:

1. vehicle rest;
2. ignition on;
3. engine start and small RPM variation;
4. brake press/release;
5. left/right indicators;
6. position lights, low beam and high beam;
7. reverse;
8. driver/other door;
9. iDrive input if applicable;
10. engine stop and ignition off.

The script records host UTC/Unix timestamps next to unmodified epoch logcat.
Because the marker is manual and clocks/process pipelines have latency, it
establishes a correlation window, not an exact physical transition time.

Repeat each state transition. A signal is promoted only when its raw source and
decoded value reproduce consistently and competing explanations are excluded.

## Community CAN hypotheses

The following are intentionally not implemented:

| Candidate | External source | Status |
|---|---|---|
| `0x23A` key-fob actions | llilakoblock project | EXTERNAL_HYPOTHESIS_NOT_VALIDATED |
| `0x2B4` door locking | llilakoblock project | EXTERNAL_HYPOTHESIS_NOT_VALIDATED |

No decoder, trigger or safety rule in the APK uses either ID.
