# BMW E9x EDIABAS Bridge

This directory contains the local Android companion backend for **BMW E9x Control**.
It avoids loading a .NET assembly in the Java VM:

```text
Java/Gradle application
  -> authenticated NDJSON on 127.0.0.1:39721
  -> .NET for Android foreground service
  -> EdiabasLib / STD:OBD
  -> Android UsbManager / FTDI K+DCAN
  -> vehicle diagnostic bus
```

## Reproducible upstream integration

EdiabasLib is a Git submodule at `vendor/ediabaslib`, pinned to the commit behind the
official `binaries_20260607` release:

- tag: `binaries_20260607`
- annotated tag object: `fcdbe32743b7077677129efa278de2ad58bc4ddc`
- source commit: `947dfbbc686290625e3fc9cad044e69af64d8fe9`
- license: GPL-3.0; see `LICENSE-NOTICE.md` and `vendor/ediabaslib/license.md`

Clone with `git submodule update --init --recursive`. No proprietary BMW PRG, IPO,
firmware or device dump belongs in Git.

## Projects

- `EdiabasBridge.Core`: protocol, dispatcher, loopback-only server and engine boundary.
- `EdiabasBridgeService`: Android 10/API 29 service and actual EdiabasLib adapter.
- `tests/EdiabasBridge.Tests`: host tests with a mock Ediabas engine.

The Android project targets `net10.0-android36.1`, matching the pinned upstream release,
while declaring Android 10/API 29 as the supported minimum and target device baseline.

## RPC protocol v1

Each request and response is one UTF-8 JSON line. The server binds only to
`IPAddress.Loopback`; it never listens on Wi-Fi or another network interface. Every
request includes `version`, a monotonic client `id`, and the random session `token`
returned through an explicit Android `PendingIntent` callback.

Methods: `status`, `listUsbDevices`, `connect`, `disconnect`, `setEcuPath`,
`uploadPrg`, `identifySgbd`, `probeSgbd`, `listJobs`, `executeJob`, `abortJob`,
and `getTrace`.

Only one EDIABAS command is accepted at a time. A second command returns
`COMMAND_BUSY`; `abortJob` remains available. Requests larger than 32 MiB are refused.

## PRG storage

The Java app imports user-owned PRGs through Android Storage Access Framework into its
private storage. When needed, it sends the exact PRG bytes and SHA-256 over the
authenticated loopback channel. The bridge validates the filename, extension and hash,
then stores the file in its own private directory:

```text
/data/user/0/com.synaro.bmwe9xediabasbridge/files/ediabas/ecu/
```

This avoids shared storage, broad filesystem permissions, and cross-package access to a
private directory.

## Safety/status semantics

Compilation is not vehicle validation. Keep these statuses independent:

- `BRIDGE BUILDS`
- `FTDI DETECTED`
- `EDIABAS CONNECTED`
- `SGBD RESPONDED`
- `FRM DETECTED`
- `LIGHT JOB DISCOVERED`
- `REAL LIGHT COMMAND VERIFIED`

The bridge does not invent an FRM job. `listJobs` uses the same SGBD metadata jobs used
by upstream Ediabas Tool (`_JOBS`, `_JOBCOMMENTS`, `_ARGUMENTS`, `_RESULTS`). A generic
`probeSgbd` is read-only and runs `IDENT` only when that job is actually present.

The Java app stores every successful metadata inventory as
`files/ediabas/jobs/frm_jobs_<sgbd>.json`, including documented job comments,
arguments and results. This file is generated only from the user-supplied PRG.

Every `executeJob` response separates `jobExecutionMs` from bridge dispatch time.
The Java client also measures the complete socket `roundTripMs` and derives
`ipcOverheadMs`. These fields feed the existing diagnostic command log so the first
real read-only job can be benchmarked without pretending that desktop/mock timing is
vehicle timing. No real K+DCAN benchmark exists until the two APKs run on the Erisin
with a granted FTDI device and a responding ECU.

The bridge also enables EdiabasLib IFH error tracing in its private
`files/ediabas/trace/` directory. `getTrace` returns a bounded tail together with the
bridge operation log; the trace is never exposed on a network interface or committed.

## Build

```powershell
cd erisin/ediabas-bridge
dotnet workload restore EdiabasBridgeService/EdiabasBridgeService.csproj
dotnet test tests/EdiabasBridge.Tests/EdiabasBridge.Tests.csproj -c Release
dotnet publish EdiabasBridgeService/EdiabasBridgeService.csproj `
  -c Debug -f net10.0-android36.1 -r android-arm64 `
  -p:EnableAndroidTargets=true
```

The Java app and bridge must both be installed. Start the bridge from the Diagnostics
screen, approve Android USB permission, and only then connect the imported SGBD.
