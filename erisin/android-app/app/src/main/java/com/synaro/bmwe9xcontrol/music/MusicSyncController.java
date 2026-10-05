package com.synaro.bmwe9xcontrol.music;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;

import java.util.concurrent.atomic.AtomicBoolean;

public final class MusicSyncController implements AutoCloseable {
    public interface Listener {
        void onFeatures(AudioFeatures features);
        void onError(String detail);
    }

    private static final int SAMPLE_RATE = 16_000;
    private final MusicAnalyzer analyzer;
    private final Listener listener;
    private final AtomicBoolean running = new AtomicBoolean();
    private Thread thread;
    private AudioRecord recorder;
    private int bpmMultiplier = 1;

    public MusicSyncController(MusicAnalyzer analyzer, Listener listener) {
        this.analyzer = analyzer;
        this.listener = listener;
    }

    public void setBpmMultiplier(int multiplier) {
        if (multiplier < 1 || multiplier > 4) throw new IllegalArgumentException("BPM multiplier must be 1..4");
        bpmMultiplier = multiplier;
    }

    public synchronized void start() {
        if (running.get()) return;
        int minimum = AudioRecord.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT);
        if (minimum <= 0) { listener.onError("AudioRecord configuration unsupported"); return; }
        try {
            recorder = new AudioRecord(MediaRecorder.AudioSource.MIC, SAMPLE_RATE,
                    AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, minimum * 2);
            recorder.startRecording();
        } catch (SecurityException | IllegalStateException ex) {
            listener.onError("AudioRecord unavailable: " + ex.getMessage());
            return;
        }
        running.set(true);
        thread = new Thread(() -> capture(minimum), "music-sync-audio");
        thread.start();
    }

    private void capture(int size) {
        short[] buffer = new short[Math.max(256, size / 2)];
        int beatCounter = 0;
        while (running.get()) {
            int count = recorder.read(buffer, 0, buffer.length);
            if (count <= 0) continue;
            short[] exact = count == buffer.length ? buffer : java.util.Arrays.copyOf(buffer, count);
            AudioFeatures features = analyzer.analyze(exact, SAMPLE_RATE, System.currentTimeMillis());
            if (features.beat) beatCounter++;
            if (!features.beat || beatCounter % bpmMultiplier == 0) listener.onFeatures(features);
        }
    }

    public synchronized void stop() {
        running.set(false);
        if (recorder != null) {
            try { recorder.stop(); } catch (IllegalStateException ignored) {}
            recorder.release();
            recorder = null;
        }
        if (thread != null) {
            thread.interrupt();
            thread = null;
        }
    }
    public boolean isRunning() { return running.get(); }
    @Override public void close() { stop(); }
}
