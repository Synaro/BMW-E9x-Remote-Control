# FRM light control research boundary

The final product may control individually qualified FRM lighting outputs. The
present code controls only names such as `simulated_left` in memory.

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

## Future qualification gate

Before a channel appears in a real `Lights` screen it needs:

- detected target FRM and matching diagnostic data;
- exact output name from that target data;
- repeatable read-only or controlled bench evidence;
- electrical-load type and safe minimum interval/duty/cooldown;
- a reversible timeout behavior;
- explicit allowlisting.

Xenon/HID ballasts and other unsuitable loads must never be rapidly strobed.
