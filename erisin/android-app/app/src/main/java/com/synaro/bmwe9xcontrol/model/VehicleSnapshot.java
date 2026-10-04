package com.synaro.bmwe9xcontrol.model;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;

public final class VehicleSnapshot {
    private final long timestampMs;
    private final DataOrigin origin;
    private final Map<String, Object> values;

    public VehicleSnapshot(long timestampMs, DataOrigin origin, Map<String, Object> values) {
        this.timestampMs = timestampMs;
        this.origin = Objects.requireNonNull(origin, "origin");
        this.values = Collections.unmodifiableMap(new LinkedHashMap<>(values));
    }

    public long getTimestampMs() { return timestampMs; }
    public DataOrigin getOrigin() { return origin; }
    public Map<String, Object> getValues() { return values; }
    public Object get(String key) { return values.get(key); }

    public static Builder builder(long timestampMs, DataOrigin origin) {
        return new Builder(timestampMs, origin);
    }

    public static final class Builder {
        private final long timestampMs;
        private final DataOrigin origin;
        private final Map<String, Object> values = new LinkedHashMap<>();

        private Builder(long timestampMs, DataOrigin origin) {
            this.timestampMs = timestampMs;
            this.origin = origin;
        }

        public Builder put(String key, Object value) {
            values.put(Objects.requireNonNull(key, "key"), value);
            return this;
        }

        public VehicleSnapshot build() {
            return new VehicleSnapshot(timestampMs, origin, values);
        }
    }
}
