package com.synaro.bmwe9xcontrol.lightshow;

import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Presets are templates. Semantic roles must be bound to discovered capability ids. */
public final class PresetLibrary {
    public static final List<String> NAMES = Collections.unmodifiableList(Arrays.asList(
            "Ghost", "Wig-Wag", "Alternating DRL", "Welcome", "Goodbye", "Showroom", "Custom 1", "Custom 2"));
    private PresetLibrary() {}

    public static LightShow create(String name, Map<String, String> bindings) {
        LightShow show = new LightShow();
        show.name = name;
        show.version = 2;
        if ("Wig-Wag".equals(name)) {
            show.loop = true;
            add(show, 0, bindings.get("left"), 100);
            add(show, 250, bindings.get("left"), 0);
            add(show, 250, bindings.get("right"), 100);
            add(show, 500, bindings.get("right"), 0);
        } else if ("Alternating DRL".equals(name)) {
            show.repeatCount = 4;
            add(show, 0, bindings.get("drl_left"), 100);
            add(show, 400, bindings.get("drl_left"), 0);
            add(show, 400, bindings.get("drl_right"), 100);
            add(show, 800, bindings.get("drl_right"), 0);
        } else {
            int value = "Ghost".equals(name) || "Goodbye".equals(name) ? 0 : 100;
            long time = 0;
            for (String channel : bindings.values()) { add(show, time, channel, value); time += 100; }
        }
        return show;
    }

    public static Map<String, String> simulatedBindings() {
        Map<String, String> result = new LinkedHashMap<>();
        result.put("left", "SIMULATED_LEFT");
        result.put("right", "SIMULATED_RIGHT");
        result.put("drl_left", "SIMULATED_DRL_LEFT");
        result.put("drl_right", "SIMULATED_DRL_RIGHT");
        return result;
    }

    private static void add(LightShow show, long time, String channel, int value) {
        if (channel == null || channel.trim().isEmpty()) return;
        LightShow.Step step = new LightShow.Step();
        step.timeMs = time;
        step.channel = channel;
        step.value = value;
        show.timeline.add(step);
    }
}
