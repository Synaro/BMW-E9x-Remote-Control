# BMW E9x Control for Erisin ES3360I

This directory contains the Android 10/API 29 BMW E9x Control application and
the evidence-driven investigation for the Erisin ES3360I.

The current application provides:

- collecting a fresh device profile through read-only ADB commands;
- identifying the proven CAN/MCU/Android data path;
- displaying simulated or replayed vehicle data;
- manual light, Ghost, presets, light-show editor and music-analysis screens;
- FTDI USB K+DCAN detection and Android permission handling;
- private user import of BMW `.PRG` files;
- an explicit EDIABAS/FRM diagnostic backend boundary and NDJSON job logging;
- preserving raw evidence and provenance;
- preserving, but not conflating, the separate XRC MCU research backend.

`VehicleTransport` remains read-only. Active FRM work uses
`DiagnosticTransport`; the committed production implementation fails closed
because the required EdiabasLib .NET Android bridge is not yet packaged. Thus
the APK is functional in Mock/Replay but does not claim real vehicle control.
See [EDIABAS_FTDI.md](docs/EDIABAS_FTDI.md).

## Quick start without the vehicle

```powershell
cd erisin/android-app
.\gradlew.bat testDebugUnitTest lintDebug assembleDebug assembleRelease
```

The APK is generated at:

`app/build/outputs/apk/debug/app-debug.apk`

The application starts with `MockTransport`; every value is labelled
`SIMULATED`. A bundled `EXAMPLE_ONLY` NDJSON capture can be selected through
the replay code path during development.

## First read-only device session

With the Erisin on the same network and ADB TCP already authorized:

```powershell
.\erisin\tools\collect_erisin_profile.ps1 -DeviceAddress 192.0.2.10:5555
```

The example address is intentionally non-routable. Replace it with the unit's
actual address. Output goes to `erisin/private-dumps/`, which is ignored by
Git. See [DEVICE_PROFILE.md](docs/DEVICE_PROFILE.md) before collecting.

## Safety boundary

No script remounts a partition, changes a property, installs an APK, clears a
log or writes an XRC serial device. The checked-in EDIABAS backend rejects all
jobs until the real bridge is explicitly integrated; discovered FRM
capabilities also start unarmed. See [SAFETY.md](docs/SAFETY.md).
