package com.synaro.bmwe9xcontrol.lightshow;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

public final class SimulatedLightSink implements LightShowEngine.OutputSink {
    private final Map<String, Integer> values = new LinkedHashMap<>();
    @Override public synchronized void apply(String channel, int value) { values.put(channel, value); }
    @Override public synchronized void restoreControl() { values.replaceAll((key, value) -> 0); }
    @Override public boolean isSimulationOnly() { return true; }
    public synchronized Map<String, Integer> values() {
        return Collections.unmodifiableMap(new LinkedHashMap<>(values));
    }
}
