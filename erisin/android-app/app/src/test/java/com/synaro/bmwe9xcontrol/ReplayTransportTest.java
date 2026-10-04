package com.synaro.bmwe9xcontrol;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import com.synaro.bmwe9xcontrol.model.DataOrigin;
import com.synaro.bmwe9xcontrol.model.RawEvent;
import com.synaro.bmwe9xcontrol.model.VehicleSnapshot;
import com.synaro.bmwe9xcontrol.transport.ReplayTransport;
import com.synaro.bmwe9xcontrol.transport.TransportState;
import com.synaro.bmwe9xcontrol.transport.VehicleTransport;

import org.junit.Test;

import java.io.IOException;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public final class ReplayTransportTest {
    @Test public void replaysSnapshotsWithReplayProvenanceInOrder() throws Exception {
        String input = "{\"timestamp_ms\":100,\"type\":\"snapshot\",\"values\":{\"rpm\":0}}\n"
                + "{\"timestamp_ms\":150,\"type\":\"snapshot\",\"values\":{\"rpm\":780}}\n";
        ReplayTransport transport = new ReplayTransport(new StringReader(input), 100.0);
        CountDownLatch latch = new CountDownLatch(2);
        List<VehicleSnapshot> snapshots = new ArrayList<>();
        transport.start(new ListenerAdapter() {
            @Override public void onSnapshot(VehicleSnapshot snapshot) {
                snapshots.add(snapshot);
                latch.countDown();
            }
        });
        assertTrue(latch.await(2, TimeUnit.SECONDS));
        transport.stop();
        assertEquals(2, snapshots.size());
        assertEquals(DataOrigin.REPLAY, snapshots.get(0).getOrigin());
        assertEquals(100L, snapshots.get(0).getTimestampMs());
        assertEquals(150L, snapshots.get(1).getTimestampMs());
        assertEquals(0L, transport.stats().getTxCount());
    }

    @Test(expected = IOException.class)
    public void rejectsNonMonotonicTimestamps() throws Exception {
        new ReplayTransport(new StringReader(
                "{\"timestamp_ms\":2,\"type\":\"event\"}\n"
                + "{\"timestamp_ms\":1,\"type\":\"event\"}\n"), 1.0);
    }

    @Test(expected = IOException.class)
    public void rejectsMissingRequiredFields() throws Exception {
        new ReplayTransport(new StringReader("{\"timestamp_ms\":2}\n"), 1.0);
    }

    private abstract static class ListenerAdapter implements VehicleTransport.Listener {
        @Override public void onSnapshot(VehicleSnapshot snapshot) {}
        @Override public void onRawEvent(RawEvent event) {}
        @Override public void onStateChanged(TransportState state, String detail) {}
    }
}
