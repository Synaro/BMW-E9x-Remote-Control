package com.synaro.bmwe9xcontrol.diagnostic;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;

public final class DiagnosticResult {
    public enum Status { SUCCESS, UNAVAILABLE, REJECTED, ERROR }

    private final Status status;
    private final Map<String, String> values;
    private final String detail;
    private final long elapsedMs;
    private final boolean simulated;

    public DiagnosticResult(Status status, Map<String, String> values, String detail,
                            long elapsedMs, boolean simulated) {
        this.status = status;
        this.values = Collections.unmodifiableMap(new LinkedHashMap<>(values));
        this.detail = detail == null ? "" : detail;
        this.elapsedMs = elapsedMs;
        this.simulated = simulated;
    }

    public static DiagnosticResult unavailable(String detail) {
        return new DiagnosticResult(Status.UNAVAILABLE, Collections.emptyMap(), detail, 0, false);
    }

    public Status getStatus() { return status; }
    public Map<String, String> getValues() { return values; }
    public String getDetail() { return detail; }
    public long getElapsedMs() { return elapsedMs; }
    public boolean isSimulated() { return simulated; }
    public boolean isSuccess() { return status == Status.SUCCESS; }
}
