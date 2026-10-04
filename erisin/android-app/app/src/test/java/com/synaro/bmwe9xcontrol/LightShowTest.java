package com.synaro.bmwe9xcontrol;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import com.synaro.bmwe9xcontrol.lightshow.CommandWatchdog;
import com.synaro.bmwe9xcontrol.lightshow.LightShow;
import com.synaro.bmwe9xcontrol.lightshow.LightShowEngine;
import com.synaro.bmwe9xcontrol.lightshow.LightShowParser;
import com.synaro.bmwe9xcontrol.lightshow.SimulatedLightSink;

import org.junit.Test;

import java.io.StringReader;

public final class LightShowTest {
    @Test public void parsesAndRunsOnlyOnExplicitSimulatedSink() throws Exception {
        String json = "{\"name\":\"demo\",\"version\":1,\"timeline\":["
                + "{\"timeMs\":0,\"channel\":\"sim_a\",\"value\":100},"
                + "{\"timeMs\":10,\"channel\":\"sim_a\",\"value\":0}]}";
        LightShow show = LightShowParser.parse(new StringReader(json));
        SimulatedLightSink sink = new SimulatedLightSink();
        assertTrue(sink.isSimulationOnly());
        try (LightShowEngine engine = new LightShowEngine(sink)) {
            engine.play(show, 10.0);
            Thread.sleep(100);
            assertEquals(Integer.valueOf(0), sink.values().get("sim_a"));
        }
    }

    @Test(expected = IllegalArgumentException.class)
    public void rejectsUnsafeValueRange() {
        LightShowParser.parse(new StringReader(
                "{\"name\":\"bad\",\"version\":1,\"timeline\":["
                + "{\"timeMs\":0,\"channel\":\"x\",\"value\":101}]}"));
    }

    @Test(expected = IllegalArgumentException.class)
    public void rejectsNonMonotonicTimeline() {
        LightShowParser.parse(new StringReader(
                "{\"name\":\"bad\",\"version\":1,\"timeline\":["
                + "{\"timeMs\":5,\"channel\":\"x\",\"value\":1},"
                + "{\"timeMs\":4,\"channel\":\"x\",\"value\":0}]}"));
    }

    @Test public void watchdogExpiresAndCanBeDisarmed() {
        CommandWatchdog watchdog = new CommandWatchdog(100);
        watchdog.arm(1_000);
        assertFalse(watchdog.hasExpired(1_099));
        assertTrue(watchdog.hasExpired(1_100));
        watchdog.disarm();
        assertFalse(watchdog.hasExpired(9_999));
    }
}
