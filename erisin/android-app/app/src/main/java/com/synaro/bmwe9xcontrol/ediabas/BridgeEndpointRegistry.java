package com.synaro.bmwe9xcontrol.ediabas;

import java.util.concurrent.atomic.AtomicReference;

public final class BridgeEndpointRegistry {
    private static final AtomicReference<BridgeEndpoint> ENDPOINT = new AtomicReference<>();
    private BridgeEndpointRegistry() {}
    public static BridgeEndpoint get() { return ENDPOINT.get(); }
    public static void set(BridgeEndpoint endpoint) { ENDPOINT.set(endpoint); }
    public static void clear() { ENDPOINT.set(null); }
}
