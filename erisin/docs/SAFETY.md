# Safety and data-handling policy

## Current build: simulation plus fail-closed diagnostic architecture

Collectors remain read-only. The Android application now has a distinct
diagnostic command abstraction, but the checked-in production backend is
`UnavailableEdiabasBridge`; it cannot open EDIABAS or send a vehicle job.
It does not:

- remount or modify a system partition;
- set an Android property/setting;
- install/uninstall/disable a package;
- clear logcat;
- change permissions or SELinux;
- open serial/CAN devices for writing;
- report a diagnostic job as successful without a real bridge response;
- select a real FRM sink automatically;
- invent a job, lamp channel or restore mechanism.

## Proprietary and personal data

These ignored directories must remain local:

```text
erisin/private-dumps/
erisin/device-backups/
erisin/vendor-apks/
erisin/decompiled-vendor/
erisin/firmware-images/
erisin/captures-private/
```

Before sharing a textual result, redact serials, VIN, account names, tokens,
Wi-Fi data, Bluetooth identities and unrelated personal logs. Publish hashes,
package metadata and derived findings rather than vendor binaries.

## Diagnostic actuation gate

Before enabling the Ediabas bridge on a vehicle, require an imported matching
SGBD, successful FRM probe, reviewed capability, explicit arming, NDJSON log,
qualified restore path and watchdog. Initial active scope is limited to
qualified non-critical body lighting.
Vehicle stationary, parking brake, battery voltage monitoring and a suitable
charger are mandatory for eventual tests.

No FRM job has been validated on the owner's car by this branch. Engine,
braking, DSC, MRS, gearbox, CAS/EWS and immobilizer work remain out of scope.
