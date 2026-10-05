package com.synaro.bmwe9xcontrol.lightshow;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class LightShow {
    public String name = "";
    public int version = 2;
    public boolean loop;
    public int repeatCount = 1;
    public long durationMs;
    public List<Step> timeline = new ArrayList<>();

    public List<String> validate() {
        List<String> errors = new ArrayList<>();
        if (name == null || name.trim().isEmpty()) errors.add("name is required");
        if (version != 1 && version != 2) errors.add("unsupported version");
        if (repeatCount < 1) errors.add("repeatCount must be at least 1");
        if (durationMs < 0) errors.add("durationMs must not be negative");
        if (timeline == null) return Collections.singletonList("timeline is required");
        long previous = -1;
        for (int i = 0; i < timeline.size(); i++) {
            Step step = timeline.get(i);
            if (step.timeMs < 0 || step.timeMs < previous) errors.add("timeline must be monotonic at " + i);
            if (!step.pause && blank(step.channel) && blank(step.group) && blank(step.scene)) {
                errors.add("channel, group or scene is required at " + i);
            }
            if (step.value < 0 || step.value > 100) errors.add("value must be 0..100 at " + i);
            if (step.durationMs < 0) errors.add("durationMs must not be negative at " + i);
            if (!"STEP".equals(step.transition) && !"FADE".equals(step.transition)) {
                errors.add("transition must be STEP or FADE at " + i);
            }
            previous = step.timeMs;
        }
        return errors;
    }

    private static boolean blank(String value) { return value == null || value.trim().isEmpty(); }

    public static final class Step {
        public long timeMs;
        public String channel = "";
        public int value;
        public String transition = "STEP";
        public long durationMs;
        public boolean fade;
        public String group = "";
        public String scene = "";
        public boolean pause;
    }
}
