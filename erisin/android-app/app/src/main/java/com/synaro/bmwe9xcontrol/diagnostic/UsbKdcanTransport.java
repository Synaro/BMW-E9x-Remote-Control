package com.synaro.bmwe9xcontrol.diagnostic;

import android.content.Context;
import android.hardware.usb.UsbManager;

import com.synaro.bmwe9xcontrol.ediabas.EdiabasBridge;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** USB-host/FTDI transport. Vehicle I/O only occurs inside the installed EdiabasBridge. */
public final class UsbKdcanTransport implements DiagnosticTransport {
    private final UsbManager usbManager;
    private final EdiabasBridge bridge;
    private final File ecuDirectory;
    private final List<DiagnosticMessage> messages = new ArrayList<>();
    private DiagnosticState state = DiagnosticState.DISCONNECTED;
    private String detail = "USB K+DCAN disconnected";

    public UsbKdcanTransport(Context context, EdiabasBridge bridge, File ecuDirectory) {
        this.usbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        this.bridge = bridge;
        this.ecuDirectory = ecuDirectory;
    }

    public UsbDeviceDescriptor detectedAdapter() {
        return UsbKdcanDetector.findFtdi(AndroidUsbCatalog.describe(usbManager));
    }

    @Override public DiagnosticResult connect() {
        UsbDeviceDescriptor device = detectedAdapter();
        if (device == null) return fail(DiagnosticState.DISCONNECTED, "No FTDI USB K+DCAN detected");
        if (!bridge.isInstalled()) return fail(DiagnosticState.ERROR,
                "FTDI detected, but " + bridge.implementationVersion() + " EDIABAS bridge is unavailable");
        state = DiagnosticState.CONNECTING;
        DiagnosticResult result = bridge.connect(device, ecuDirectory);
        state = result.isSuccess() ? DiagnosticState.CONNECTED :
                result.getDetail().contains("permission") ? DiagnosticState.USB_PERMISSION_REQUIRED : DiagnosticState.ERROR;
        detail = result.getDetail();
        return result;
    }

    private DiagnosticResult fail(DiagnosticState next, String message) {
        state = next;
        detail = message;
        return DiagnosticResult.unavailable(message);
    }

    @Override public void disconnect() {
        bridge.disconnect();
        state = DiagnosticState.DISCONNECTED;
        detail = "USB K+DCAN disconnected";
    }

    @Override public DiagnosticResult send(DiagnosticRequest request) {
        if (state != DiagnosticState.CONNECTED) return DiagnosticResult.unavailable("diagnostic transport not connected");
        DiagnosticResult result = bridge.execute(request);
        messages.add(new DiagnosticMessage(System.currentTimeMillis(), "EDIABAS",
                request.getSgbd() + "/" + request.getJob() + " -> " + result.getStatus()));
        return result;
    }

    @Override public List<DiagnosticMessage> receive() {
        List<DiagnosticMessage> copy = new ArrayList<>(messages);
        messages.clear();
        return Collections.unmodifiableList(copy);
    }
    @Override public DiagnosticState state() { return state; }
    @Override public String detail() { return detail; }
}
