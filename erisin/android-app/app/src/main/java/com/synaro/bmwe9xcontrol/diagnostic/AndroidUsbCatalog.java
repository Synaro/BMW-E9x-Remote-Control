package com.synaro.bmwe9xcontrol.diagnostic;

import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbManager;

import java.util.ArrayList;
import java.util.List;

public final class AndroidUsbCatalog {
    private AndroidUsbCatalog() {}

    public static List<UsbDeviceDescriptor> describe(UsbManager manager) {
        List<UsbDeviceDescriptor> result = new ArrayList<>();
        for (UsbDevice device : manager.getDeviceList().values()) {
            String product = "";
            String serial = "";
            try {
                product = device.getProductName();
                if (manager.hasPermission(device)) serial = device.getSerialNumber();
            } catch (SecurityException ignored) {
                // Metadata remains optional until Android grants USB permission.
            }
            result.add(new UsbDeviceDescriptor(device.getVendorId(), device.getProductId(),
                    product, serial, manager.hasPermission(device)));
        }
        return result;
    }

    public static UsbDevice findFtdi(UsbManager manager) {
        for (UsbDevice device : manager.getDeviceList().values()) {
            if (device.getVendorId() == 0x0403) return device;
        }
        return null;
    }
}
