# FRM light control implementation boundary

The APK now contains the transport-independent control path, manual controls,
Ghost controller, presets and light-show engine. The checked-in APK still
defaults to simulation because the EdiabasLib .NET bridge and target-device
qualification are not yet present.

## Capability discovery

1. The user imports `.PRG` files with Android's document picker.
2. Files are copied to app-private `files/ediabas/ecu/`.
3. The Ediabas bridge probes each imported SGBD rather than hardcoding
   `FRM_70` or `FRM_87`.
4. Only real metadata whose job/description contains `STEUERN_`, `LAMPE`,
   `PWM` or `AUSGANG` becomes a candidate capability.
5. The capability records exact SGBD, job, argument and documented range.
6. Every candidate starts **unarmed** and is labelled
   `DOCUMENTED_SGBD_METADATA`; discovery is not vehicle validation.

No lamp name, job argument or restore job is invented. A real output is not
selected until its candidate has been reviewed and explicitly armed.

## Temporary control and restore

`FrmLightSink` executes a diagnostic job only for an armed capability. It logs
timestamp, SGBD, job, arguments, result, elapsed time and status to NDJSON.
Ghost mode uses those same primitives and never writes FSW/PSW coding.

Stopping a show or pressing **RESTORE FRM CONTROL** calls an explicitly
qualified restore job. It does not assume that value zero releases an
override. If no restore job was qualified, restoration is reported as
unavailable rather than guessed.

## What OdazhiuLS establishes

The public OdazhiuLS project calls EDIABAS through `pydiabas`, selects an FRM
variant in its own mapping and executes `STEUERN_LAMPEN_PWM` with `AUSGANG_*`
arguments. That is useful evidence for a diagnostic control *concept*.

It does not establish for the owner's E90:

- the detected FRM variant (`FRM_70`, `FRM_87`, or another data variant);
- the available/allowed output names;
- raw request arbitration ID or transport frames;
- session, security, timing or response requirements;
- whether XRC can originate the same diagnostic exchange;
- safe PWM/frequency limits for the installed lamps and drivers.

## Protocol layers to keep separate

```text
normal broadcast CAN
functional diagnostic request
physical ECU diagnostic request
transport segmentation/reassembly
diagnostic service/session
EDIABAS PRG job semantics
FRM output behavior
```

The KOMBI-related `0x6F1` example is ECU-specific community evidence and is not
an FRM recipe.

## Vehicle qualification gate

Before a channel appears in a real `Lights` screen it needs:

- detected target FRM and matching diagnostic data;
- exact output name from that target data;
- repeatable read-only or controlled bench evidence;
- electrical-load type and safe minimum interval/duty/cooldown;
- a qualified restore job and reversible timeout behavior;
- explicit allowlisting.

Xenon/HID ballasts and other unsuitable loads must never be rapidly strobed.
