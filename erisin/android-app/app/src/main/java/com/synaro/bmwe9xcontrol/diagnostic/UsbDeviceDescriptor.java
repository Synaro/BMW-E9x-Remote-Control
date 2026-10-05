package com.synaro.bmwe9xcontrol.diagnostic;

public final class UsbDeviceDescriptor {
    public final int vendorId;
    public final int productId;
    public final String productName;
    public final String serialNumber;
    public final boolean permissionGranted;

    public UsbDeviceDescriptor(int vendorId, int productId, String productName,
                               String serialNumber, boolean permissionGranted) {
        this.vendorId = vendorId;
        this.productId = productId;
        this.productName = productName == null ? "" : productName;
        this.serialNumber = serialNumber == null ? "" : serialNumber;
        this.permissionGranted = permissionGranted;
    }

    public boolean isFtdi() { return vendorId == 0x0403; }
    public String identity() {
        return String.format("%04X:%04X %s %s", vendorId, productId, productName, serialNumber).trim();
    }
}
