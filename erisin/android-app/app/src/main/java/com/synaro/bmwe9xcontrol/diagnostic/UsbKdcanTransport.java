package com.synaro.bmwe9xcontrol.diagnostic;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;
import android.os.Build;

import com.synaro.bmwe9xcontrol.ediabas.EdiabasBridge;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** USB-host/FTDI transport. Vehicle I/O only occurs inside the installed EdiabasBridge. */
public final class UsbKdcanTransport implements DiagnosticTransport {
    public static final String ACTION_USB_PERMISSION = "com.synaro.bmwe9xcontrol.USB_PERMISSION";
    private final Context context;
    private final UsbManager usbManager;
    private final EdiabasBridge bridge;
    private final File ecuDirectory;
    private final List<DiagnosticMessage> messages = new ArrayList<>();
    private DiagnosticState state = DiagnosticState.DISCONNECTED;
    private String detail = "USB K+DCAN disconnected";

    public UsbKdcanTransport(Context context, EdiabasBridge bridge, File ecuDirectory) {
        this.context = context.getApplicationContext();
        this.usbManager = (UsbManager) context.getSystemService(Context.USB_SERVICE);
        this.bridge = bridge;
        this.ecuDirectory = ecuDirectory;
    }

    public UsbDeviceDescriptor detectedAdapter() {
        return UsbKdcanDetector.findFtdi(AndroidUsbCatalog.describe(usbManager));
    }

    public boolean requestUsbPermission() {
        UsbDevice device = AndroidUsbCatalog.findFtdi(usbManager);
        if (device == null || usbManager.hasPermission(device)) return false;
        int flags = Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0;
        PendingIntent pending = PendingIntent.getBroadcast(context, 0,
                new Intent(ACTION_USB_PERMISSION).setPackage(context.getPackageName()), flags);
        usbManager.requestPermission(device, pending);
        state = DiagnosticState.USB_PERMISSION_REQUIRED;
        detail = "Android USB permission requested";
        return true;
    }

    @Override public DiagnosticResult connect() {
        UsbDeviceDescriptor device = detectedAdapter();
        if (device == null) return fail(DiagnosticState.DISCONNECTED, "No FTDI USB K+DCAN detected");
        if (!device.permissionGranted) return fail(DiagnosticState.USB_PERMISSION_REQUIRED,
                "FTDI detected; Android USB permission required");
        if (!bridge.isInstalled()) return fail(DiagnosticState.ERROR,
                "FTDI detected, but " + bridge.implementationVersion() + " EDIABAS bridge is unavailable");
        state = DiagnosticState.CONNECTING;
        DiagnosticResult result = bridge.connect(device, ecuDirectory);
        state = result.isSuccess() ? DiagnosticState.CONNECTED : DiagnosticState.ERROR;
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
