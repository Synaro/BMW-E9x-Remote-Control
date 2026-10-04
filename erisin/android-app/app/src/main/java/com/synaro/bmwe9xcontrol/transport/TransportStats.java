package com.synaro.bmwe9xcontrol.transport;

public final class TransportStats {
    private final long rxCount;
    private final long txCount;
    private final long errorCount;

    public TransportStats(long rxCount, long txCount, long errorCount) {
        this.rxCount = rxCount;
        this.txCount = txCount;
        this.errorCount = errorCount;
    }

    public long getRxCount() { return rxCount; }
    public long getTxCount() { return txCount; }
    public long getErrorCount() { return errorCount; }
}
