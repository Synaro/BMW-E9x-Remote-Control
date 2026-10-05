package com.synaro.bmwe9xcontrol.diagnostic;

public final class DiagnosticMessage {
    public final long timestampMs;
    public final String source;
    public final String payload;

    public DiagnosticMessage(long timestampMs, String source, String payload) {
        this.timestampMs = timestampMs;
        this.source = source;
        this.payload = payload;
    }
}
