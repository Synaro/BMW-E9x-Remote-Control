package com.synaro.bmwe9xcontrol.ediabas;

import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticRequest;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;
import com.synaro.bmwe9xcontrol.diagnostic.UsbDeviceDescriptor;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Locale;

/** Test-only bridge. Every result is explicitly marked simulated. */
public final class MockEdiabasBridge implements EdiabasBridge {
    private final List<EdiabasJobDefinition> jobs = new ArrayList<>();
    private final List<DiagnosticRequest> requests = new ArrayList<>();
    private boolean connected;

    public MockEdiabasBridge addJob(EdiabasJobDefinition job) { jobs.add(job); return this; }
    public List<DiagnosticRequest> requests() { return Collections.unmodifiableList(requests); }
    @Override public boolean isInstalled() { return true; }
    @Override public EdiabasBridgeStatus status() {
        return new EdiabasBridgeStatus(true, "SIMULATED", "SIMULATED", Collections.emptyList(),
                false, connected, "SIMULATED", "", connected ? "SIMULATED_CONNECTED" : "SIMULATED",
                requests.isEmpty() ? "" : requests.get(requests.size() - 1).getJob(), 1, 0, 1, "");
    }
    @Override public String implementationVersion() { return "SIMULATED"; }
    @Override public DiagnosticResult connect(UsbDeviceDescriptor device, File ecuDirectory) {
        connected = true;
        return result("SIMULATED_CONNECTION");
    }
    @Override public void disconnect() { connected = false; }
    @Override public String identifySgbd(File prgFile) {
        String name = prgFile.getName();
        return name.toUpperCase(Locale.ROOT).endsWith(".PRG") ? name.substring(0, name.length() - 4) : "";
    }
    @Override public DiagnosticResult probe(File prgFile) {
        return connected ? result("SIMULATED_PROBE") : DiagnosticResult.unavailable("SIMULATED_BRIDGE_DISCONNECTED");
    }
    @Override public List<EdiabasJobDefinition> listJobs(File prgFile) {
        return Collections.unmodifiableList(jobs);
    }
    @Override public DiagnosticResult execute(DiagnosticRequest request) {
        if (!connected) return DiagnosticResult.unavailable("SIMULATED_BRIDGE_DISCONNECTED");
        requests.add(request);
        return result("SIMULATED_JOB_RESULT");
    }
    @Override public DiagnosticResult abortJob() { return result("SIMULATED_ABORT"); }
    @Override public String getTrace() { return "SIMULATED_TRACE"; }
    private static DiagnosticResult result(String detail) {
        Map<String, String> values = new LinkedHashMap<>();
        values.put("origin", "SIMULATED");
        return new DiagnosticResult(DiagnosticResult.Status.SUCCESS, values, detail, 1, true);
    }
}
