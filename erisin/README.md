# BMW E9x Control for Erisin ES3360I

This directory is the isolated, read-only-first investigation and Android MVP
for the Erisin ES3360I installed in the test BMW E90 LCI.

The current scope is deliberately limited to:

- collecting a fresh device profile through read-only ADB commands;
- identifying the proven CAN/MCU/Android data path;
- displaying simulated or replayed vehicle data in a compilable Android app;
- preserving raw evidence and provenance;
- preparing, but **not enabling**, an XRC MCU backend.

There is no vehicle transmit API in the application. `XrcMcuTransport` is a
read-only discovery placeholder and reports itself unavailable until the exact
device protocol is established from the owner's unit.

## Quick start without the vehicle

```powershell
cd erisin/android-app
.\gradlew.bat testDebugUnitTest assembleDebug
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
log, writes a serial device, sends CAN, or invokes a vehicle diagnostic job.
See [SAFETY.md](docs/SAFETY.md).
