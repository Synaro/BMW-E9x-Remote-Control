package com.synaro.bmwe9xcontrol.lightshow;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class LightShow {
    public String name = "";
    public int version = 1;
    public boolean loop;
    public List<Step> timeline = new ArrayList<>();

    public List<String> validate() {
        List<String> errors = new ArrayList<>();
        if (name == null || name.trim().isEmpty()) errors.add("name is required");
        if (version != 1) errors.add("unsupported version");
        if (timeline == null) return Collections.singletonList("timeline is required");
        long previous = -1;
        for (int i = 0; i < timeline.size(); i++) {
            Step step = timeline.get(i);
            if (step.timeMs < 0 || step.timeMs < previous) errors.add("timeline must be monotonic at " + i);
            if (step.channel == null || step.channel.trim().isEmpty()) errors.add("channel is required at " + i);
            if (step.value < 0 || step.value > 100) errors.add("value must be 0..100 at " + i);
            previous = step.timeMs;
        }
        return errors;
    }

    public static final class Step {
        public long timeMs;
        public String channel = "";
        public int value;
        public String transition = "STEP";
        public long durationMs;
    }
}
