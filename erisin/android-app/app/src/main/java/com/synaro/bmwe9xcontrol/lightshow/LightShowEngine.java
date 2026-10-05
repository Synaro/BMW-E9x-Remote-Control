package com.synaro.bmwe9xcontrol.lightshow;

import java.util.List;
import java.util.Objects;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

/** Transport-independent scheduler. The supplied sink owns all vehicle safety decisions. */
public final class LightShowEngine implements AutoCloseable {
    private final OutputSink sink;
    private ScheduledExecutorService executor;
    private final AtomicLong generation = new AtomicLong();

    public LightShowEngine(OutputSink sink) { this.sink = Objects.requireNonNull(sink, "sink"); }

    public synchronized void play(LightShow show, double speed) {
        List<String> errors = show.validate();
        if (!errors.isEmpty()) throw new IllegalArgumentException(String.join("; ", errors));
        if (speed <= 0.0) throw new IllegalArgumentException("speed must be positive");
        stop();
        long token = generation.incrementAndGet();
        executor = Executors.newSingleThreadScheduledExecutor();
        if (show.loop) {
            scheduleLoop(show, speed, token);
        } else {
            long cycle = cycleDuration(show, speed);
            for (int repetition = 0; repetition < show.repeatCount; repetition++) {
                scheduleCycle(show, speed, token, repetition * cycle);
            }
            long end = show.repeatCount * cycle;
            executor.schedule(() -> finish(token), end, TimeUnit.MILLISECONDS);
        }
    }

    private void scheduleLoop(LightShow show, double speed, long token) {
        if (token != generation.get() || executor == null || executor.isShutdown()) return;
        scheduleCycle(show, speed, token, 0);
        executor.schedule(() -> scheduleLoop(show, speed, token), cycleDuration(show, speed), TimeUnit.MILLISECONDS);
    }

    private void scheduleCycle(LightShow show, double speed, long token, long offsetMs) {
        for (LightShow.Step step : show.timeline) {
            long delay = offsetMs + Math.round(step.timeMs / speed);
            executor.schedule(() -> {
                if (token == generation.get()) sink.applyStep(step);
            }, delay, TimeUnit.MILLISECONDS);
        }
    }

    private static long cycleDuration(LightShow show, double speed) {
        long raw = show.durationMs;
        for (LightShow.Step step : show.timeline) raw = Math.max(raw, step.timeMs + step.durationMs);
        return Math.max(1, Math.round((raw + 1) / speed));
    }

    private synchronized void finish(long token) {
        if (token != generation.get()) return;
        sink.restoreControl();
        if (executor != null) executor.shutdown();
    }

    public synchronized void stop() {
        generation.incrementAndGet();
        if (executor != null) executor.shutdownNow();
        executor = null;
        sink.restoreControl();
    }

    public synchronized boolean isPlaying() { return executor != null && !executor.isShutdown(); }
    @Override public void close() { stop(); }

    public interface OutputSink {
        void apply(String channel, int value);
        default void applyStep(LightShow.Step step) {
            if (step.pause) return;
            if (step.scene != null && !step.scene.trim().isEmpty()) applyScene(step.scene, step.value);
            else if (step.group != null && !step.group.trim().isEmpty()) applyGroup(step.group, step.value);
            else apply(step.channel, step.value);
        }
        default void applyGroup(String group, int value) { apply(group, value); }
        default void applyScene(String scene, int value) { apply(scene, value); }
        void restoreControl();
        boolean isSimulationOnly();
    }
}
