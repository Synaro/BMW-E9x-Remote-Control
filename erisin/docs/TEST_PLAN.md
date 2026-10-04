# Test plan

## Automated host tests

- replay NDJSON parsing and chronological playback;
- rejection of missing fields and non-monotonic timestamps;
- preservation of `REPLAY` provenance;
- mock values labelled `SIMULATED`;
- transport boundary contains no transmit/send method;
- XRC placeholder opens nothing and reports unavailable;
- light-show JSON validation, monotonic timeline and 0–100 bounds;
- simulated playback and all-off behavior;
- watchdog expiry;
- profile analyzer extraction and keyword leads.

Build gate:

```powershell
cd erisin/android-app
.\gradlew.bat testDebugUnitTest assembleDebug
```

## Target-device read-only tests

1. install the debug APK only after the owner chooses to do so;
2. verify Diagnostics on Android 10/API 29;
3. verify Mock and Replay are visibly labelled;
4. run the ADB profile collector and manually redact its output;
5. identify the stock dashboard process while live values are shown;
6. execute at least three correlation captures per vehicle action;
7. compare timestamps, Binder/service logs and candidate device activity.

## Not yet authorized

- opening a candidate serial node for writing;
- invoking unknown Binder transactions;
- sending CAN/CAN-box frames;
- replaying captured data to the MCU;
- issuing EDIABAS/FRM jobs from the Erisin.
