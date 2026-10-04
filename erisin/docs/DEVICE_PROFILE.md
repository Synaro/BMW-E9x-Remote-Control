# Target device profile

## Known before the fresh collection

| Field | Value | Status |
|---|---|---|
| Product | Erisin ES3360I for BMW | OWNER_REPORTED |
| Vehicle | BMW E90 LCI | CONFIRMED_BY_OWNER |
| SoC/platform property previously observed | `trinket`, ARM64 | HISTORICAL_OBSERVATION |
| Actual Android previously observed | Android 10 / API 29 | HISTORICAL_OBSERVATION |
| Root shell | `uid=0` previously obtained | HISTORICAL_OBSERVATION |
| ADB TCP | port 5555 previously used | HISTORICAL_OBSERVATION |
| Current XRC OS | unknown exact V8 string | MUST_RECOLLECT |
| Current MCU | unknown | MUST_RECOLLECT |

`XRC_BM_OS_P_7.07` and `XRC0BML241231A.Z665P` are historical values only.
They must not be displayed as current device facts.

## Fresh collection checklist

Run `erisin/tools/collect_erisin_profile.ps1` while the unit is reachable. It
collects read-only:

- complete `getprop`, build fingerprint, platform and Android API;
- kernel/CPU/modules/devices/TTY inventory;
- mount and filesystem capacity information;
- package paths and system/third-party classification;
- process, service and `dumpsys` snapshots;
- SELinux status and root identity probes;
- `/dev`, `/sys/class`, matching `/sys/devices` and network interfaces;
- keyword leads for XRC, MCU, CAN, EventCenter, vehicle, serial and CAN box.

Then run `pull_vendor_components_readonly.ps1` only after reviewing the package
matches. Pulled APKs and their SHA-256 inventory remain under the ignored
`erisin/vendor-apks/` directory.

## Fields to fill from evidence

```text
collection session:
XRC OS exact version:
XRC OS build/date:
MCU exact version:
Android release/API:
ro.board.platform:
build fingerprint:
manufacturer/model:
root result:
SELinux mode:
matching package names/versions:
important APK/library SHA-256:
candidate device nodes and permissions:
candidate Binder services:
candidate processes:
```

Do not add serial numbers, Wi-Fi credentials, Google account information or a
full unredacted dump to Git.
