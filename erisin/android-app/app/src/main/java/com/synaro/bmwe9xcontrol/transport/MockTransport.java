package com.synaro.bmwe9xcontrol.transport;

import com.synaro.bmwe9xcontrol.model.DataOrigin;
import com.synaro.bmwe9xcontrol.model.RawEvent;
import com.synaro.bmwe9xcontrol.model.VehicleSnapshot;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

public final class MockTransport implements VehicleTransport {
    private final AtomicLong rxCount = new AtomicLong();
    private ScheduledExecutorService executor;
    private volatile TransportState state = TransportState.STOPPED;
    private int tick;

    @Override public String name() { return "MockTransport (SIMULATED)"; }

    @Override
    public synchronized void start(Listener listener) {
        if (executor != null) return;
        state = TransportState.CONNECTED;
        listener.onStateChanged(state, "All values are SIMULATED");
        executor = Executors.newSingleThreadScheduledExecutor(r -> {
            Thread t = new Thread(r, "mock-vehicle-transport");
            t.setDaemon(true);
            return t;
        });
        executor.scheduleWithFixedDelay(() -> emit(listener), 0, 500, TimeUnit.MILLISECONDS);
    }

    private void emit(Listener listener) {
        long now = System.currentTimeMillis();
        int localTick = tick++;
        double rpm = 780.0 + Math.sin(localTick / 3.0) * 15.0;
        int speed = (localTick * 3) % 90;
        boolean brake = localTick % 12 >= 8;
        boolean left = localTick % 16 < 3;
        boolean right = localTick % 16 >= 8 && localTick % 16 < 11;
        VehicleSnapshot snapshot = VehicleSnapshot.builder(now, DataOrigin.SIMULATED)
                .put("rpm", rpm)
                .put("speed_kmh", speed)
                .put("brake", brake)
                .put("low_beam", true)
                .put("high_beam", localTick % 20 == 0)
                .put("turn_left", left)
                .put("turn_right", right)
                .put("door_driver", localTick % 30 == 0)
                .put("door_other", false)
                .put("reverse", localTick % 40 >= 35)
                .build();
        rxCount.incrementAndGet();
        listener.onSnapshot(snapshot);
        listener.onRawEvent(new RawEvent(now, DataOrigin.SIMULATED, "MOCK", "VEHICLE_SNAPSHOT",
                "", "tick=" + localTick, "rpm=" + Math.round(rpm) + ",speed=" + speed,
                "SIMULATED"));
    }

    @Override public synchronized void stop() {
        if (executor != null) executor.shutdownNow();
        executor = null;
        state = TransportState.STOPPED;
    }
    @Override public TransportState state() { return state; }
    @Override public TransportStats stats() { return new TransportStats(rxCount.get(), 0, 0); }
}
