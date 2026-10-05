package com.synaro.bmwe9xcontrol.ediabas;

import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;
import com.synaro.bmwe9xcontrol.frm.FrmCapability;
import com.synaro.bmwe9xcontrol.frm.FrmCapabilityDiscovery;
import com.synaro.bmwe9xcontrol.frm.FrmDiscoveryResult;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public final class EdiabasSession {
    private final EdiabasBridge bridge;
    private final EdiabasFileStore store;

    public EdiabasSession(EdiabasBridge bridge, EdiabasFileStore store) {
        this.bridge = bridge;
        this.store = store;
    }

    public FrmDiscoveryResult discoverFrm() {
        if (!bridge.isInstalled()) return new FrmDiscoveryResult(null, "", new ArrayList<>(),
                UnavailableEdiabasBridge.REASON);
        for (File prg : store.importedPrgFiles()) {
            DiagnosticResult probe = bridge.probe(prg);
            if (!probe.isSuccess()) continue;
            String sgbd = bridge.identifySgbd(prg);
            if (sgbd == null || sgbd.trim().isEmpty()) continue;
            List<EdiabasJobDefinition> jobs = bridge.listJobs(prg);
            try { store.writeJobDump(sgbd, jobs); }
            catch (java.io.IOException ex) {
                return new FrmDiscoveryResult(prg, sgbd, new ArrayList<>(),
                        "SGBD responded but metadata dump failed: " + ex.getMessage());
            }
            List<FrmCapability> capabilities = FrmCapabilityDiscovery.fromJobs(sgbd, jobs);
            if (!capabilities.isEmpty()) {
                return new FrmDiscoveryResult(prg, sgbd, capabilities,
                        "CONNECTED; candidates require target-vehicle review");
            }
        }
        return new FrmDiscoveryResult(null, "", new ArrayList<>(),
                "No compatible imported SGBD responded with documented light-control jobs");
    }
}
