package com.synaro.bmwe9xcontrol.transport;

import com.synaro.bmwe9xcontrol.model.RawEvent;
import com.synaro.bmwe9xcontrol.model.VehicleSnapshot;

/** Read-only application boundary. Deliberately contains no transmit method. */
public interface VehicleTransport extends AutoCloseable {
    String name();
    void start(Listener listener) throws Exception;
    void stop();
    TransportState state();
    TransportStats stats();

    @Override
    default void close() { stop(); }

    interface Listener {
        void onSnapshot(VehicleSnapshot snapshot);
        void onRawEvent(RawEvent event);
        void onStateChanged(TransportState state, String detail);
    }
}
