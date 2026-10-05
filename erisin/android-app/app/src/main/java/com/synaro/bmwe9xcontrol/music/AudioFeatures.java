package com.synaro.bmwe9xcontrol.music;

public final class AudioFeatures {
    public final double envelope;
    public final double bassEnergy;
    public final boolean beat;
    public final long timestampMs;

    public AudioFeatures(double envelope, double bassEnergy, boolean beat, long timestampMs) {
        this.envelope = envelope;
        this.bassEnergy = bassEnergy;
        this.beat = beat;
        this.timestampMs = timestampMs;
    }
}
