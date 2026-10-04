package com.synaro.bmwe9xcontrol.transport;

/**
 * Read-only placeholder for evidence-driven XRC discovery.
 *
 * No serial path, Binder descriptor, protocol bytes, or send method are guessed.
 * It stays unavailable until a fresh profile and vendor-component analysis from
 * the owner's ES3360I proves the receive path.
 */
public final class XrcMcuTransport implements VehicleTransport {
    private volatile TransportState state = TransportState.STOPPED;

    @Override public String name() { return "XrcMcuTransport (DISCOVERY_ONLY)"; }

    @Override
    public void start(Listener listener) {
        state = TransportState.UNAVAILABLE;
        listener.onStateChanged(state,
                "XRC interface not qualified on this device; no device is opened");
    }

    @Override public void stop() { state = TransportState.STOPPED; }
    @Override public TransportState state() { return state; }
    @Override public TransportStats stats() { return new TransportStats(0, 0, 0); }
}
