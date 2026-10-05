# Light-show JSON format v2

```json
{
  "name": "WIG_WAG",
  "version": 2,
  "loop": true,
  "repeatCount": 1,
  "durationMs": 500,
  "timeline": [
    {
      "timeMs": 0,
      "channel": "discovered_capability_id",
      "value": 100,
      "transition": "STEP",
      "durationMs": 250,
      "group": "",
      "scene": "",
      "pause": false
    }
  ]
}
```

Rules:

- versions `1` and `2` are accepted; new files use `2`;
- `timeMs` is monotonic and non-negative;
- `value` is an integer percentage from 0 through 100;
- `transition` is `STEP` or `FADE`;
- `durationMs` is the step hold/fade duration; show-level `durationMs` can
  define a longer cycle;
- a step targets a `channel`, `group`, `scene`, or is an explicit `pause`;
- `repeatCount` applies to finite shows; `loop` repeats until STOP;
- playback speed must be positive;
- STOP and natural completion request `restoreControl()` on the output sink.

The editor stores shows in app-private `files/lightshows/`. Import/export does
not embed BMW target data.

`SimulatedLightSink` is immediately available. `FrmLightSink` accepts only
capability IDs discovered from imported SGBD metadata and explicitly armed by
the user. Preset semantic roles must be bound to those IDs before real
execution; the simulated bindings are never presented as BMW output names.
