package com.synaro.bmwe9xcontrol.ediabas;

import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticRequest;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;
import com.synaro.bmwe9xcontrol.diagnostic.UsbDeviceDescriptor;

import java.io.File;
import java.util.Collections;
import java.util.List;

/** Production-safe placeholder: it never reports a vehicle command as successful. */
public final class UnavailableEdiabasBridge implements EdiabasBridge {
    public static final String REASON = "EDIABAS_DOTNET_BRIDGE_NOT_INSTALLED";
    @Override public boolean isInstalled() { return false; }
    @Override public String implementationVersion() { return "none"; }
    @Override public DiagnosticResult connect(UsbDeviceDescriptor device, File ecuDirectory) {
        return DiagnosticResult.unavailable(REASON);
    }
    @Override public void disconnect() {}
    @Override public String identifySgbd(File prgFile) { return ""; }
    @Override public DiagnosticResult probe(File prgFile) { return DiagnosticResult.unavailable(REASON); }
    @Override public List<EdiabasJobDefinition> listJobs(File prgFile) { return Collections.emptyList(); }
    @Override public DiagnosticResult execute(DiagnosticRequest request) {
        return DiagnosticResult.unavailable(REASON);
    }
    @Override public DiagnosticResult abortJob() { return DiagnosticResult.unavailable(REASON); }
    @Override public String getTrace() { return ""; }
}
