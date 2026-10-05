package com.synaro.bmwe9xcontrol.diagnostic;

import java.util.List;

public final class UsbKdcanDetector {
    private UsbKdcanDetector() {}

    public static UsbDeviceDescriptor findFtdi(List<UsbDeviceDescriptor> devices) {
        if (devices == null) return null;
        for (UsbDeviceDescriptor device : devices) if (device != null && device.isFtdi()) return device;
        return null;
    }
}
