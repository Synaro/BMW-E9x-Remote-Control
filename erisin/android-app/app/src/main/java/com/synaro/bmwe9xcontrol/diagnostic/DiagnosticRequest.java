package com.synaro.bmwe9xcontrol.diagnostic;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.Objects;

public final class DiagnosticRequest {
    private final String sgbd;
    private final String job;
    private final Map<String, String> arguments;

    public DiagnosticRequest(String sgbd, String job, Map<String, String> arguments) {
        this.sgbd = requireText(sgbd, "sgbd");
        this.job = requireText(job, "job");
        this.arguments = Collections.unmodifiableMap(new LinkedHashMap<>(
                Objects.requireNonNull(arguments, "arguments")));
    }

    private static String requireText(String value, String name) {
        if (value == null || value.trim().isEmpty()) throw new IllegalArgumentException(name + " is required");
        return value.trim();
    }

    public String getSgbd() { return sgbd; }
    public String getJob() { return job; }
    public Map<String, String> getArguments() { return arguments; }
}
