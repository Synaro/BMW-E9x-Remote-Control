package com.synaro.bmwe9xcontrol.lightshow;

public final class CommandWatchdog {
    private final long timeoutMs;
    private long lastHeartbeatMs;
    private boolean armed;

    public CommandWatchdog(long timeoutMs) {
        if (timeoutMs <= 0) throw new IllegalArgumentException("timeout must be positive");
        this.timeoutMs = timeoutMs;
    }

    public synchronized void arm(long nowMs) { armed = true; lastHeartbeatMs = nowMs; }
    public synchronized void heartbeat(long nowMs) { if (armed) lastHeartbeatMs = nowMs; }
    public synchronized boolean hasExpired(long nowMs) { return armed && nowMs - lastHeartbeatMs >= timeoutMs; }
    public synchronized void disarm() { armed = false; }
}
