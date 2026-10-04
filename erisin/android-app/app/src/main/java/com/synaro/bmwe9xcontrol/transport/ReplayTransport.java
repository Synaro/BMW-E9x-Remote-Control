package com.synaro.bmwe9xcontrol.transport;

import com.google.gson.Gson;
import com.google.gson.JsonObject;
import com.synaro.bmwe9xcontrol.model.DataOrigin;
import com.synaro.bmwe9xcontrol.model.RawEvent;
import com.synaro.bmwe9xcontrol.model.VehicleSnapshot;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.Reader;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicLong;

public final class ReplayTransport implements VehicleTransport {
    private final List<Record> records;
    private final double speed;
    private final AtomicLong rxCount = new AtomicLong();
    private ScheduledExecutorService executor;
    private volatile TransportState state = TransportState.STOPPED;

    public ReplayTransport(Reader reader, double speed) throws IOException {
        if (speed <= 0.0) throw new IllegalArgumentException("speed must be positive");
        this.speed = speed;
        this.records = parse(reader);
    }

    static List<Record> parse(Reader reader) throws IOException {
        Gson gson = new Gson();
        List<Record> parsed = new ArrayList<>();
        try (BufferedReader buffered = new BufferedReader(reader)) {
            String line;
            int lineNumber = 0;
            while ((line = buffered.readLine()) != null) {
                lineNumber++;
                if (line.trim().isEmpty() || line.trim().startsWith("#")) continue;
                JsonObject object;
                try {
                    object = gson.fromJson(line, JsonObject.class);
                } catch (RuntimeException ex) {
                    throw new IOException("invalid NDJSON at line " + lineNumber, ex);
                }
                if (object == null || !object.has("timestamp_ms") || !object.has("type")) {
                    throw new IOException("missing timestamp_ms/type at line " + lineNumber);
                }
                parsed.add(new Record(object.get("timestamp_ms").getAsLong(), object));
            }
        }
        for (int i = 1; i < parsed.size(); i++) {
            if (parsed.get(i).timestampMs < parsed.get(i - 1).timestampMs) {
                throw new IOException("timestamps are not monotonic");
            }
        }
        return parsed;
    }

    @Override public String name() { return "ReplayTransport (REPLAY)"; }

    @Override
    public synchronized void start(Listener listener) {
        if (executor != null) return;
        state = TransportState.REPLAYING;
        listener.onStateChanged(state, "Replaying " + records.size() + " records");
        executor = Executors.newSingleThreadScheduledExecutor();
        long base = records.isEmpty() ? 0L : records.get(0).timestampMs;
        for (Record record : records) {
            long delay = Math.max(0L, Math.round((record.timestampMs - base) / speed));
            executor.schedule(() -> emit(record, listener), delay, TimeUnit.MILLISECONDS);
        }
    }

    private void emit(Record record, Listener listener) {
        JsonObject object = record.object;
        String type = object.get("type").getAsString();
        if ("snapshot".equals(type) && object.has("values")) {
            Map<String, Object> values = new LinkedHashMap<>();
            object.getAsJsonObject("values").entrySet().forEach(entry ->
                    values.put(entry.getKey(), new Gson().fromJson(entry.getValue(), Object.class)));
            listener.onSnapshot(new VehicleSnapshot(record.timestampMs, DataOrigin.REPLAY, values));
        }
        listener.onRawEvent(new RawEvent(record.timestampMs, DataOrigin.REPLAY,
                getString(object, "source", "REPLAY"), type,
                getString(object, "id", ""), getString(object, "payload", ""),
                getString(object, "decoded_value", ""), "REPLAY"));
        rxCount.incrementAndGet();
    }

    private static String getString(JsonObject object, String name, String fallback) {
        return object.has(name) && !object.get(name).isJsonNull() ? object.get(name).getAsString() : fallback;
    }

    @Override public synchronized void stop() {
        if (executor != null) executor.shutdownNow();
        executor = null;
        state = TransportState.STOPPED;
    }
    @Override public TransportState state() { return state; }
    @Override public TransportStats stats() { return new TransportStats(rxCount.get(), 0, 0); }

    static final class Record {
        final long timestampMs;
        final JsonObject object;
        Record(long timestampMs, JsonObject object) { this.timestampMs = timestampMs; this.object = object; }
    }
}
