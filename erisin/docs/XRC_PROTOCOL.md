# XRC protocol notebook

Status: `UNKNOWN_ON_TARGET_DEVICE`.

No public KSW, Choiceway, HiWorld, Raise or RLC protocol is treated as XRC.
This document records the evidence gates for discovering the ES3360I path.

## Read-only discovery order

1. Identify the existing dashboard package/process while its live data is on
   screen.
2. Record its APK path, version, signer metadata and SHA-256 locally.
3. Inspect manifest components: services, providers, receivers, permissions,
   native libraries and exported Binder endpoints.
4. Search DEX/native strings for `System.loadLibrary`, JNI symbols, AIDL,
   `Binder`, `LocalSocket`, `SerialPort`, UART/SPI/CAN devices and XRC terms.
5. Correlate logcat and process/service activity with controlled vehicle events.
6. Attribute reads to a concrete device node or IPC endpoint.
7. Only then document framing/checksums and whether the endpoint accepts writes.

## Candidate mechanisms — all unselected

| Mechanism | Evidence needed |
|---|---|
| Binder/AIDL service | service descriptor, transaction surface, caller permissions, live calls |
| Android broadcast/content provider | exact action/key/provider and live value correlation |
| UART/serial | exact device node, owner process, baud/framing and captured receive bytes |
| JNI/native library | Java entry point, exported/internal native path and data flow |
| CAN box protocol | identified hardware/module and framed traffic correlated with events |
| SocketCAN | `can*` interface, kernel support, permissions and passive frames |

## TX capability classification

- `LEVEL_A_RAW_CAN_TX`: arbitrary frame interface proven end to end.
- `LEVEL_B_VENDOR_COMMANDS`: outbound path exists but only for vendor commands.
- `LEVEL_C_RX_ONLY`: serious inspection finds no outbound vehicle path.
- current value: `UNCLASSIFIED`.

An outbound MCU function may control the radio or display without ever reaching
BMW CAN. A method name containing “CAN” may wrap CAN-box commands rather than
arbitrary bus frames. End-to-end proof is mandatory.

## Protocol evidence record

For every byte sequence eventually documented, record:

```text
capture/session id:
device and firmware hashes:
direction:
physical/IPC endpoint:
raw bytes:
framing/checksum:
trigger:
observed response:
repetitions:
confidence:
alternative explanations:
```

No write experiment is authorized by this notebook.
