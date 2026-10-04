# Safety and data-handling policy

## Current phase: read only

The collectors may read system state, copy selected installed APK files from
the device and record logcat. They do not:

- remount or modify a system partition;
- set an Android property/setting;
- install/uninstall/disable a package;
- clear logcat;
- change permissions or SELinux;
- open serial/CAN devices for writing;
- invoke vehicle diagnostic jobs;
- transmit a vehicle or MCU command.

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

## Future TX gate

Any future TX must be a separate, explicit phase after bench proof, with
default-off developer mode, allowlisting, watchdog and a safe-expiry path.
Initial active scope is limited to qualified non-critical body lighting.
Vehicle stationary, parking brake, battery voltage monitoring and a suitable
charger are mandatory for eventual tests.

No part of the present branch authorizes vehicle TX.
