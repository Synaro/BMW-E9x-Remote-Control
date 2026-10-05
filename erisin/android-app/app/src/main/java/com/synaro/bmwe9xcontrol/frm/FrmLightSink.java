package com.synaro.bmwe9xcontrol.frm;

import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticRequest;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticTransport;
import com.synaro.bmwe9xcontrol.lightshow.LightShowEngine;

import java.io.IOException;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

/** Real sink boundary. It rejects unknown and unarmed capabilities. */
public final class FrmLightSink implements LightShowEngine.OutputSink {
    private final DiagnosticTransport transport;
    private final Map<String, FrmCapability> capabilities;
    private final FrmCommandLogger logger;
    private final Set<String> active = new LinkedHashSet<>();
    private volatile String lastError = "";

    public FrmLightSink(DiagnosticTransport transport, Iterable<FrmCapability> capabilities,
                        FrmCommandLogger logger) {
        this.transport = transport;
        this.logger = logger;
        Map<String, FrmCapability> indexed = new LinkedHashMap<>();
        for (FrmCapability capability : capabilities) indexed.put(capability.id, capability);
        this.capabilities = Collections.unmodifiableMap(indexed);
    }

    @Override public synchronized void apply(String channel, int value) {
        FrmCapability capability = capabilities.get(channel);
        if (capability == null) throw reject("unknown FRM capability: " + channel);
        if (!capability.isArmed()) throw reject("FRM capability is not armed: " + channel);
        Map<String, String> args = new LinkedHashMap<>();
        args.put(capability.valueArgument, Integer.toString(capability.encodePercent(value)));
        DiagnosticRequest request = new DiagnosticRequest(capability.sgbd, capability.ediabasJob, args);
        DiagnosticResult result = transport.send(request);
        log(request, result);
        if (!result.isSuccess()) throw reject("EDIABAS job failed: " + result.getStatus() + " " + result.getDetail());
        active.add(channel);
        lastError = "";
    }

    @Override public synchronized void restoreControl() {
        for (String channel : new LinkedHashSet<>(active)) {
            FrmCapability capability = capabilities.get(channel);
            if (capability == null || !capability.canRestoreControl()) {
                lastError = "RESTORE JOB NOT QUALIFIED FOR " + channel;
                continue;
            }
            DiagnosticRequest request = new DiagnosticRequest(capability.sgbd,
                    capability.getRestoreJob(), Collections.emptyMap());
            DiagnosticResult result = transport.send(request);
            log(request, result);
            if (result.isSuccess()) active.remove(channel);
            else lastError = "restore failed for " + channel + ": " + result.getDetail();
        }
    }

    private void log(DiagnosticRequest request, DiagnosticResult result) {
        if (logger == null) return;
        try { logger.append(System.currentTimeMillis(), request, result); }
        catch (IOException ex) { lastError = "command log failed: " + ex.getMessage(); }
    }

    private IllegalStateException reject(String message) { lastError = message; return new IllegalStateException(message); }
    public String lastError() { return lastError; }
    public Map<String, FrmCapability> capabilities() { return capabilities; }
    @Override public boolean isSimulationOnly() { return false; }
}
