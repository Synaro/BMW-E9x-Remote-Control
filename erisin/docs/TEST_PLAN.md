# Test plan

## Automated host tests

- replay NDJSON parsing and chronological playback;
- rejection of missing fields and non-monotonic timestamps;
- preservation of `REPLAY` provenance;
- mock values labelled `SIMULATED`;
- telemetry transport remains read-only; diagnostic commands use a separate boundary;
- XRC placeholder opens nothing and reports unavailable;
- light-show v1/v2 parsing, timing, finite repeats, loops, scenes/groups/pause and 0–100 bounds;
- presets use supplied bindings rather than invented BMW channels;
- capability discovery uses only SGBD job metadata;
- unarmed/unknown FRM capabilities are rejected;
- mocked EDIABAS job mapping and explicit restore behavior;
- Ghost uses the same sink and restore primitive;
- FTDI attach/detach detection by USB descriptor;
- music envelope, bass energy, beat detection and minimum interval;
- watchdog expiry;
- profile analyzer extraction and keyword leads.

Build gate:

```powershell
cd erisin/android-app
.\gradlew.bat testDebugUnitTest lintDebug assembleDebug assembleRelease
```

## Target-device staging tests

1. install the debug APK only after the owner chooses to do so;
2. verify Diagnostics on Android 10/API 29;
3. verify Mock and Replay are visibly labelled;
4. run the ADB profile collector and manually redact its output;
5. identify the stock dashboard process while live values are shown;
6. execute at least three correlation captures per vehicle action;
7. connect the FTDI K+DCAN cable and confirm Android USB permission/device identity;
8. import owner-supplied PRGs and confirm they remain in app-private storage;
9. after the GPL EdiabasLib bridge exists, probe FRM without executing a control job;
10. compare timestamps, Binder/service logs and candidate device activity.

## Not yet authorized

- opening a candidate serial node for writing;
- invoking unknown Binder transactions;
- sending CAN/CAN-box frames;
- replaying captured data to the MCU;
- calling a light-control job whose SGBD, arguments, range and restore job have
  not been reviewed on the target FRM;
- using engine, DSC, ABS, MRS, CAS/EWS, gearbox or immobilizer diagnostic jobs.
