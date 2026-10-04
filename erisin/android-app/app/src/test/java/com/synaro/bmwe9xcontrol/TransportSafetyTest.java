package com.synaro.bmwe9xcontrol;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.synaro.bmwe9xcontrol.model.DataOrigin;
import com.synaro.bmwe9xcontrol.model.RawEvent;
import com.synaro.bmwe9xcontrol.model.VehicleSnapshot;
import com.synaro.bmwe9xcontrol.transport.MockTransport;
import com.synaro.bmwe9xcontrol.transport.TransportState;
import com.synaro.bmwe9xcontrol.transport.VehicleTransport;
import com.synaro.bmwe9xcontrol.transport.XrcMcuTransport;

import org.junit.Test;

import java.lang.reflect.Method;
import java.util.Locale;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public final class TransportSafetyTest {
    @Test public void readOnlyTransportContractHasNoTransmitSurface() {
        for (Method method : VehicleTransport.class.getDeclaredMethods()) {
            String name = method.getName().toLowerCase(Locale.ROOT);
            assertFalse("unexpected write surface: " + name,
                    name.contains("transmit") || name.equals("send") || name.equals("write"));
        }
    }

    @Test public void mockAlwaysMarksDataSimulated() throws Exception {
        MockTransport transport = new MockTransport();
        CountDownLatch latch = new CountDownLatch(1);
        final DataOrigin[] origin = new DataOrigin[1];
        transport.start(new ListenerAdapter() {
            @Override public void onSnapshot(VehicleSnapshot snapshot) {
                origin[0] = snapshot.getOrigin();
                latch.countDown();
            }
        });
        assertTrue(latch.await(2, TimeUnit.SECONDS));
        transport.stop();
        assertEquals(DataOrigin.SIMULATED, origin[0]);
        assertEquals(0L, transport.stats().getTxCount());
    }

    @Test public void xrcBackendDoesNotGuessOrOpenAnything() {
        XrcMcuTransport transport = new XrcMcuTransport();
        transport.start(new ListenerAdapter() {});
        assertEquals(TransportState.UNAVAILABLE, transport.state());
        assertEquals(0L, transport.stats().getRxCount());
        assertEquals(0L, transport.stats().getTxCount());
    }

    private abstract static class ListenerAdapter implements VehicleTransport.Listener {
        @Override public void onSnapshot(VehicleSnapshot snapshot) {}
        @Override public void onRawEvent(RawEvent event) {}
        @Override public void onStateChanged(TransportState state, String detail) {}
    }
}
