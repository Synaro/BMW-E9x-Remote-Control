# Research findings

Research date: 2026-10-04. Every external result remains unconfirmed on the
owner's ES3360I until direct evidence says otherwise.

## Head-unit integration projects

`chinesebmwheadunits/eventcenter` documents patching a
`com.szchoiceway.eventcenter` application on an older Chinese BMW head unit.
The associated `mcu` repository contains vendor MCU binaries, while
`asaril/ksw-mcu-tools` verifies KSW firmware checksums. These projects prove
that Android/MCU integration is often vendor-specific. They do not prove that
XRC uses Choiceway's service, KSW framing, the same serial device, or the same
transaction ordinals.

A newer public reverse-engineering project, `armeehn/device-reveng`, documents
an Android 13 QCM6125 Toyota unit with an RLC/HiWorld stack. On that specific
unit EventCenter owns `/dev/ttyHS1` at 115200 baud and exposes an AIDL method
named `sendCanbusData`. This is strong methodological guidance for what to look
for in an APK, but only a community report with respect to the XRC BMW unit.

`sygi1982/bmw-dash` demonstrates a clean Android transport abstraction around
external USB/Bluetooth/Wi-Fi CAN interfaces. `K-CAN App` likewise requires a
separate supported interface. Both support the external-interface fallback;
neither proves access to the ES3360I internal CAN decoder.

## BMW E8x/E9x data references

`bmwcd/opendbc-BMW-E8x-E9x` and community captures are useful candidate
decoders. They also show why the bus must be recorded: identical numeric IDs
can occur on different buses and a signal observed on one platform is not
automatically valid on this E90.

`llilakoblock/bmw-e87-e90-can-bt` describes an external ESP32 interface and
labels `0x23A` as remote-control/key-fob actions and `0x2B4` as door locking.
Both are stored only as **external, unvalidated hypotheses**. They are not
input mappings, safety conditions or command definitions in this project.

MorGuux's `0x6F1` example targets KOMBI. It must never be reused as an FRM
command merely because it is BMW diagnostic traffic.

## FRM and OdazhiuLS

`SerhiiOdazhiu1/OdazhiuLS` is a Windows/Python application using `pydiabas` and
EDIABAS. Its E90 path invokes the diagnostic job:

```text
ECU/variant: FRM_87 (project mapping)
job: STEUERN_LAMPEN_PWM
argument shape: AUSGANG_* ; PWM value
```

This proves only that the project's EDIABAS environment can ask a named FRM
job to control an output. `pydiabas` delegates to installed proprietary
EDIABAS; it does not disclose the complete raw bus exchange. The chain is:

```text
OdazhiuLS → pydiabas → EDIABAS job/PRG interpreter → configured IFH/interface
           → BMW diagnostic transport → selected FRM → response/timeout
```

To reproduce this safely without EDIABAS we would need target-vehicle evidence
for ECU addressing, transport, session/preconditions, request segmentation,
timers, response handling, job-specific payload and termination. Normal CAN
broadcasts, functional diagnostic addressing and physical ECU diagnostics are
separate concepts. None of those missing details is inferred here.

The output names found in OdazhiuLS are leads, not the detected channels of the
owner's FRM. Detection through the target FRM/SP-Daten and controlled evidence
is required before any light control.

## XRC V7 versus V8

Public searches in English, French, German, Russian and Chinese found scattered
XRC OS/MCU version strings and forum reports, but no trustworthy, legally
reusable pair of exact ES3360I V7/V8 images, extracted APK sets or authoritative
changelog. Random firmware archives were deliberately not downloaded.

Therefore the comparison is evidence-gated:

1. collect the exact current V8 string, MCU string, package versions and hashes;
2. preserve any owner-supplied historical V7 inventory locally, if legally
   available;
3. compare package manifests, DEX/API surface and native-library symbols without
   committing vendor binaries;
4. record only textual findings and hashes.

## TikTok project

No TikTok URL, account name or stable creator identifier is present in the
available task material. Public searches found light-show applications and
videos, including OdazhiuLS and proprietary BimmerLight, but no defensible link
to the owner's referenced video. Attribution remains `UNKNOWN`; the original
public URL is needed for a targeted search.
