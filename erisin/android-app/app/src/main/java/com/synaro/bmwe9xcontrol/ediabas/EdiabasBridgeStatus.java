package com.synaro.bmwe9xcontrol.ediabas;

import com.synaro.bmwe9xcontrol.diagnostic.UsbDeviceDescriptor;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Truthful snapshot of bridge/USB/session state; it never implies vehicle validation. */
public final class EdiabasBridgeStatus {
    public final boolean bridgeConnected;
    public final String bridgeVersion;
    public final String ediabasVersion;
    public final List<UsbDeviceDescriptor> usbDevices;
    public final boolean usbPermissionGranted;
    public final boolean ediabasConfigured;
    public final String ecuPath;
    public final String activeSgbd;
    public final String state;
    public final String lastJob;
    public final long jobExecutionMs;
    public final long ipcMs;
    public final long roundTripMs;
    public final String lastError;

    public EdiabasBridgeStatus(boolean bridgeConnected, String bridgeVersion, String ediabasVersion,
                               List<UsbDeviceDescriptor> usbDevices, boolean usbPermissionGranted,
                               boolean ediabasConfigured, String ecuPath, String activeSgbd,
                               String state, String lastJob, long jobExecutionMs, long ipcMs,
                               long roundTripMs, String lastError) {
        this.bridgeConnected = bridgeConnected;
        this.bridgeVersion = safe(bridgeVersion);
        this.ediabasVersion = safe(ediabasVersion);
        this.usbDevices = Collections.unmodifiableList(new ArrayList<>(usbDevices));
        this.usbPermissionGranted = usbPermissionGranted;
        this.ediabasConfigured = ediabasConfigured;
        this.ecuPath = safe(ecuPath);
        this.activeSgbd = safe(activeSgbd);
        this.state = safe(state);
        this.lastJob = safe(lastJob);
        this.jobExecutionMs = Math.max(0L, jobExecutionMs);
        this.ipcMs = Math.max(0L, ipcMs);
        this.roundTripMs = Math.max(0L, roundTripMs);
        this.lastError = safe(lastError);
    }

    public static EdiabasBridgeStatus unavailable(String error) {
        return new EdiabasBridgeStatus(false, "", "", Collections.emptyList(), false,
                false, "", "", "NOT_CONNECTED", "", 0, 0, 0, error);
    }

    private static String safe(String value) { return value == null ? "" : value; }
}
