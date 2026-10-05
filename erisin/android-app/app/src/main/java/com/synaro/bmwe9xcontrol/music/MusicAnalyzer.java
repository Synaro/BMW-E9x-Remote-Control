package com.synaro.bmwe9xcontrol.music;

/** Small deterministic analyzer suitable for API 29 without native DSP dependencies. */
public final class MusicAnalyzer {
    private double averageEnvelope;
    private boolean initialized;
    private long lastBeatMs = Long.MIN_VALUE;
    private double sensitivity = 1.45;
    private long minimumIntervalMs = 180;

    public void setSensitivity(double sensitivity) {
        if (sensitivity < 1.0 || sensitivity > 5.0) throw new IllegalArgumentException("sensitivity must be 1..5");
        this.sensitivity = sensitivity;
    }
    public void setMinimumIntervalMs(long minimumIntervalMs) {
        if (minimumIntervalMs < 50) throw new IllegalArgumentException("minimum interval must be at least 50 ms");
        this.minimumIntervalMs = minimumIntervalMs;
    }

    public AudioFeatures analyze(short[] samples, int sampleRate, long timestampMs) {
        if (samples == null || samples.length == 0) throw new IllegalArgumentException("samples are required");
        if (sampleRate <= 0) throw new IllegalArgumentException("sampleRate must be positive");
        double sumSquares = 0;
        for (short sample : samples) {
            double normalized = sample / 32768.0;
            sumSquares += normalized * normalized;
        }
        double envelope = Math.sqrt(sumSquares / samples.length);
        double bass = bassEnergy(samples, sampleRate);
        if (!initialized) {
            averageEnvelope = envelope;
            initialized = true;
        }
        boolean intervalPassed = lastBeatMs == Long.MIN_VALUE || timestampMs - lastBeatMs >= minimumIntervalMs;
        boolean beat = intervalPassed && envelope > Math.max(0.015, averageEnvelope * sensitivity);
        if (beat) lastBeatMs = timestampMs;
        averageEnvelope = averageEnvelope * 0.92 + envelope * 0.08;
        return new AudioFeatures(envelope, bass, beat, timestampMs);
    }

    private static double bassEnergy(short[] samples, int sampleRate) {
        int n = Math.min(samples.length, 256);
        if (n < 16) return 0;
        int maxBin = Math.max(1, Math.min(n / 2, (int) Math.ceil(250.0 * n / sampleRate)));
        double energy = 0;
        for (int bin = 1; bin <= maxBin; bin++) {
            double real = 0;
            double imaginary = 0;
            for (int i = 0; i < n; i++) {
                double window = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / (n - 1));
                double value = (samples[i] / 32768.0) * window;
                double angle = -2 * Math.PI * bin * i / n;
                real += value * Math.cos(angle);
                imaginary += value * Math.sin(angle);
            }
            energy += real * real + imaginary * imaginary;
        }
        return energy / (n * n);
    }
}
