package com.synaro.bmwe9xcontrol.frm;

import com.synaro.bmwe9xcontrol.lightshow.LightShowEngine;

import java.util.Map;

/** Ghost is temporary diagnostic override only; it never changes FSW/PSW coding. */
public final class GhostController {
    private final LightShowEngine.OutputSink sink;

    public GhostController(LightShowEngine.OutputSink sink) { this.sink = sink; }

    public void activate(GhostProfile profile) {
        for (Map.Entry<String, Integer> entry : profile.values().entrySet()) {
            sink.apply(entry.getKey(), entry.getValue());
        }
    }

    public void restoreFrmControl() { sink.restoreControl(); }
}
