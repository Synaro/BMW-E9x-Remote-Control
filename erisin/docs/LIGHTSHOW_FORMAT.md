# Light-show format v1

The v1 parser and scheduler are implemented, but only
`SimulatedLightSink` exists. This format does not authorize a real output.

```json
{
  "name": "EXAMPLE_ONLY",
  "version": 1,
  "loop": false,
  "timeline": [
    {
      "timeMs": 0,
      "channel": "simulated_left",
      "value": 100,
      "transition": "STEP",
      "durationMs": 0
    }
  ]
}
```

Rules currently enforced:

- version is exactly 1;
- name and channel are non-empty;
- timeline is monotonic and timestamps are non-negative;
- values are integer percentages from 0 through 100;
- playback speed is positive;
- `stop()` cancels scheduled work and calls `allOff()` on the sink.

`transition` and `durationMs` are reserved for future fades/editor work; only
step scheduling is implemented. Loop, pause, visual editor, BPM and music
analysis are not MVP functionality.

Real channels must later come from a detected FRM and verified capabilities,
not from the conceptual list in the product vision.
