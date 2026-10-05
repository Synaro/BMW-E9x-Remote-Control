# EdiabasLib / FTDI K+DCAN integration

## Implemented integration boundary

`LocalEdiabasBridgeClient` talks to the separately installed
`BMW E9x EDIABAS Bridge` service. The bridge, not the Java application, is the sole
owner of `UsbManager`, USB permission, the upstream FTDI port and the EdiabasNet
instance. Java-side USB enumeration is informational only.

The bridge configures `EdInterfaceObd` with `EdFtdiInterface.PortId + "0"`, supplies
the upstream Android `EdFtdiInterface.ConnectParameterType(UsbManager)`, and sets the
private ECU path. This is the Android `STD:OBD` path; no Windows COM port is simulated.

The exact pinned upstream is `binaries_20260607` / commit
`947dfbbc686290625e3fc9cad044e69af64d8fe9`.

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

The source tree therefore defines `EdiabasBridge` as the exact Java boundary and
builds a separate GPL-compatible Android service. The Java APK owns:

- informational FTDI discovery (never the USB handle or permission request);
- app-private PRG import (`files/ediabas/ecu/`);
- connection/error states;
- SGBD probe and capability metadata contract;
- job execution request/result models;
- raw NDJSON command logging;
- fail-closed behavior when the companion package or authenticated endpoint is absent.

The implemented companion bridge uses the pinned EdiabasLib API to:

1. request Android USB permission and configure FTDI using `STD:OBD`;
2. receive user-imported PRGs over authenticated loopback, verify SHA-256 and store
   them in the bridge package's private ECU directory;
3. point `EcuPath` at that private directory;
4. probe an imported PRG against the connected car;
5. enumerate exact job, argument and result metadata;
6. execute a requested SGBD/job/argument set;
7. return raw result sets, EDIABAS errors and separated timing fields;
8. abort jobs, disconnect and release USB deterministically.

## Auto-detection behavior

The app never assumes `FRM_70.PRG` or `FRM_87.PRG`. It probes all user-imported
PRGs through the bridge and selects the first responding SGBD that exposes
documented lighting-control candidate jobs. The UI then displays detected SGBD,
file, interface and status.

Until both APKs are installed and tested on the ES3360I, truthful statuses remain
independent. A desktop build establishes only `BRIDGE BUILDS`; it does not promote any
of the following runtime gates:

```text
FTDI DETECTED: NOT TESTED ON TARGET
EDIABAS CONNECTED: NOT TESTED ON TARGET
SGBD RESPONDED: NOT TESTED ON VEHICLE
FRM DETECTED: NOT TESTED ON VEHICLE
LIGHT JOB DISCOVERED: NONE WITHOUT USER PRG AND VEHICLE
REAL LIGHT COMMAND VERIFIED: NO
```

That is a truthful incomplete integration, not a simulated vehicle pass.
