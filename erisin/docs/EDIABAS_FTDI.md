# EdiabasLib / FTDI K+DCAN integration

## Pinned upstream

- project: `uholeschak/ediabaslib`
- license: GPL-3.0
- reference release: `binaries_20260607` (2026-06-07)
- supported path documented upstream: Android USB host → FTDI →
  BMW-DS2/BMW-FAST/D-CAN → imported SGBD/PRG

No upstream binary, BMW PRG, firmware or other proprietary file is committed.

## Why a bridge is required

The existing BMW E9x Control MVP is a Java Android Gradle application.
Current EdiabasLib Android projects are C#/.NET for Android; there is no
official Java Maven artifact that can simply be added to `app/build.gradle`.

The source tree therefore defines `EdiabasBridge` as the exact boundary for a
separately built GPL-compatible Android component. The Java APK already owns:

- FTDI discovery and Android USB permission;
- app-private PRG import (`files/ediabas/ecu/`);
- connection/error states;
- SGBD probe and capability metadata contract;
- job execution request/result models;
- raw NDJSON command logging;
- fail-closed behavior through `UnavailableEdiabasBridge`.

The missing bridge must implement, using the pinned EdiabasLib API:

1. connect to the Android-granted FTDI device using `STD:OBD`;
2. point `EcuPath` at the app-private ECU directory;
3. probe an imported PRG against the connected car;
4. enumerate exact job and argument metadata;
5. execute a requested SGBD/job/argument set;
6. return raw results, elapsed time and errors;
7. disconnect and release USB deterministically.

## Auto-detection behavior

The app never assumes `FRM_70.PRG` or `FRM_87.PRG`. It probes all user-imported
PRGs through the bridge and selects the first responding SGBD that exposes
documented lighting-control candidate jobs. The UI then displays detected SGBD,
file, interface and status.

Until the bridge is installed on the ES3360I, the expected status is:

```text
USB FTDI: detected or not detected
EDIABAS: BRIDGE NOT INSTALLED
FRM: NOT DETECTED
vehicle jobs: unavailable
```

That is a truthful incomplete integration, not a simulated vehicle pass.
