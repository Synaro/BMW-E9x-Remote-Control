package com.synaro.bmwe9xcontrol.frm;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

public final class GhostProfile {
    public String name = "Ghost";
    public final Map<String, Integer> channelValues = new LinkedHashMap<>();
    public boolean interiorOptional = true;

    public Map<String, Integer> values() {
        return Collections.unmodifiableMap(channelValues);
    }
}
