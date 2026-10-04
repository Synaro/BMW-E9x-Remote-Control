package com.synaro.bmwe9xcontrol.repository;

import com.synaro.bmwe9xcontrol.model.RawEvent;
import com.synaro.bmwe9xcontrol.model.VehicleSnapshot;
import com.synaro.bmwe9xcontrol.transport.TransportState;
import com.synaro.bmwe9xcontrol.transport.TransportStats;
import com.synaro.bmwe9xcontrol.transport.VehicleTransport;

import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.concurrent.CopyOnWriteArrayList;

public final class VehicleRepository implements VehicleTransport.Listener, AutoCloseable {
    private static final int MAX_EVENTS = 500;
    private final VehicleTransport transport;
    private final ArrayDeque<RawEvent> events = new ArrayDeque<>();
    private final CopyOnWriteArrayList<Observer> observers = new CopyOnWriteArrayList<>();
    private volatile VehicleSnapshot latest;
    private volatile TransportState state = TransportState.STOPPED;
    private volatile String stateDetail = "";

    public VehicleRepository(VehicleTransport transport) {
        this.transport = Objects.requireNonNull(transport, "transport");
    }

    public void start() throws Exception { transport.start(this); }
    public void stop() { transport.stop(); }
    public String transportName() { return transport.name(); }
    public VehicleSnapshot latest() { return latest; }
    public TransportState state() { return state; }
    public String stateDetail() { return stateDetail; }
    public TransportStats stats() { return transport.stats(); }

    public synchronized List<RawEvent> events() { return new ArrayList<>(events); }
    public void addObserver(Observer observer) { observers.add(observer); }
    public void removeObserver(Observer observer) { observers.remove(observer); }

    @Override public void onSnapshot(VehicleSnapshot snapshot) {
        latest = snapshot;
        for (Observer observer : observers) observer.onRepositoryChanged();
    }

    @Override public synchronized void onRawEvent(RawEvent event) {
        if (events.size() == MAX_EVENTS) events.removeFirst();
        events.addLast(event);
        for (Observer observer : observers) observer.onRepositoryChanged();
    }

    @Override public void onStateChanged(TransportState next, String detail) {
        state = next;
        stateDetail = detail == null ? "" : detail;
        for (Observer observer : observers) observer.onRepositoryChanged();
    }

    @Override public void close() { stop(); }

    public interface Observer { void onRepositoryChanged(); }
}
