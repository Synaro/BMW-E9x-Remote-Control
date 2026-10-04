package com.synaro.bmwe9xcontrol.lightshow;

import java.util.List;
import java.util.Objects;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;

/** Scheduler usable only with an explicitly supplied sink; production vehicle sinks do not exist. */
public final class LightShowEngine implements AutoCloseable {
    private final OutputSink sink;
    private ScheduledExecutorService executor;

    public LightShowEngine(OutputSink sink) { this.sink = Objects.requireNonNull(sink, "sink"); }

    public synchronized void play(LightShow show, double speed) {
        List<String> errors = show.validate();
        if (!errors.isEmpty()) throw new IllegalArgumentException(String.join("; ", errors));
        if (speed <= 0.0) throw new IllegalArgumentException("speed must be positive");
        stop();
        executor = Executors.newSingleThreadScheduledExecutor();
        for (LightShow.Step step : show.timeline) {
            executor.schedule(() -> sink.apply(step.channel, step.value),
                    Math.round(step.timeMs / speed), TimeUnit.MILLISECONDS);
        }
    }

    public synchronized void stop() {
        if (executor != null) executor.shutdownNow();
        executor = null;
        sink.allOff();
    }

    public synchronized boolean isPlaying() { return executor != null && !executor.isShutdown(); }
    @Override public void close() { stop(); }

    public interface OutputSink {
        void apply(String channel, int value);
        void allOff();
        boolean isSimulationOnly();
    }
}
