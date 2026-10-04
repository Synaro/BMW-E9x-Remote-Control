package com.synaro.bmwe9xcontrol.model;

import java.util.Objects;

public final class RawEvent {
    private final long timestampMs;
    private final DataOrigin origin;
    private final String source;
    private final String type;
    private final String id;
    private final String payload;
    private final String decodedValue;
    private final String confidence;

    public RawEvent(long timestampMs, DataOrigin origin, String source, String type,
                    String id, String payload, String decodedValue, String confidence) {
        this.timestampMs = timestampMs;
        this.origin = Objects.requireNonNull(origin, "origin");
        this.source = safe(source);
        this.type = safe(type);
        this.id = safe(id);
        this.payload = safe(payload);
        this.decodedValue = safe(decodedValue);
        this.confidence = safe(confidence);
    }

    private static String safe(String value) { return value == null ? "" : value; }
    public long getTimestampMs() { return timestampMs; }
    public DataOrigin getOrigin() { return origin; }
    public String getSource() { return source; }
    public String getType() { return type; }
    public String getId() { return id; }
    public String getPayload() { return payload; }
    public String getDecodedValue() { return decodedValue; }
    public String getConfidence() { return confidence; }
}
