package com.synaro.bmwe9xcontrol.frm;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public final class FrmDiscoveryResult {
    public final File prgFile;
    public final String sgbd;
    public final List<FrmCapability> capabilities;
    public final String status;

    public FrmDiscoveryResult(File prgFile, String sgbd, List<FrmCapability> capabilities, String status) {
        this.prgFile = prgFile;
        this.sgbd = sgbd;
        this.capabilities = Collections.unmodifiableList(new ArrayList<>(capabilities));
        this.status = status;
    }
}
