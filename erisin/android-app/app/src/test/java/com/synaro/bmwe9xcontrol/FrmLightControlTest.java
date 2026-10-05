package com.synaro.bmwe9xcontrol;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticMessage;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticRequest;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticState;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticTransport;
import com.synaro.bmwe9xcontrol.diagnostic.UsbDeviceDescriptor;
import com.synaro.bmwe9xcontrol.diagnostic.UsbKdcanDetector;
import com.synaro.bmwe9xcontrol.ediabas.EdiabasArgument;
import com.synaro.bmwe9xcontrol.ediabas.EdiabasJobDefinition;
import com.synaro.bmwe9xcontrol.ediabas.UnavailableEdiabasBridge;
import com.synaro.bmwe9xcontrol.frm.FrmCapability;
import com.synaro.bmwe9xcontrol.frm.FrmCapabilityDiscovery;
import com.synaro.bmwe9xcontrol.frm.FrmLightSink;
import com.synaro.bmwe9xcontrol.frm.GhostController;
import com.synaro.bmwe9xcontrol.frm.GhostProfile;
import com.synaro.bmwe9xcontrol.lightshow.LightShow;
import com.synaro.bmwe9xcontrol.lightshow.LightShowEngine;
import com.synaro.bmwe9xcontrol.lightshow.LightShowParser;
import com.synaro.bmwe9xcontrol.lightshow.PresetLibrary;
import com.synaro.bmwe9xcontrol.music.AudioFeatures;
import com.synaro.bmwe9xcontrol.music.MusicAnalyzer;

import org.junit.Test;

import java.io.File;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public final class FrmLightControlTest {
    @Test public void capabilityMappingUsesOnlyDocumentedSgbdMetadata() {
        EdiabasArgument pwm = new EdiabasArgument("PWM_WERT", "Prozent", 0.0, 100.0, Collections.emptyList());
        List<EdiabasJobDefinition> jobs = Arrays.asList(
                new EdiabasJobDefinition("STEUERN_LAMPEN_PWM", "documented output", Collections.singletonList(pwm)),
                new EdiabasJobDefinition("STATUS_MOTORDREHZAHL", "rpm", Collections.singletonList(pwm)));
        List<FrmCapability> capabilities = FrmCapabilityDiscovery.fromJobs("TEST_FRM", jobs);
        assertEquals(1, capabilities.size());
        FrmCapability capability = capabilities.get(0);
        assertEquals("STEUERN_LAMPEN_PWM", capability.ediabasJob);
        assertEquals("PWM_WERT", capability.valueArgument);
        assertTrue(capability.supportsPwm);
        assertFalse(capability.isArmed());
        assertTrue(capability.evidence.startsWith("DOCUMENTED_SGBD_METADATA"));
    }

    @Test public void frmSinkRejectsUnknownAndUnarmedThenMapsAndRestores() {
        RecordingTransport transport = new RecordingTransport();
        FrmCapability capability = capability();
        FrmLightSink sink = new FrmLightSink(transport, Collections.singletonList(capability), null);
        expectFailure(() -> sink.apply("missing", 100));
        expectFailure(() -> sink.apply(capability.id, 100));
        capability.setArmed(true);
        capability.setRestoreJob("STEUERN_LAMPEN_RETURN_CONTROL");
        sink.apply(capability.id, 50);
        assertEquals("50", transport.requests.get(0).getArguments().get("PWM_WERT"));
        sink.restoreControl();
        assertEquals(2, transport.requests.size());
        assertEquals("STEUERN_LAMPEN_RETURN_CONTROL", transport.requests.get(1).getJob());
        assertFalse(sink.isSimulationOnly());
    }

    @Test public void ghostProfileUsesSameSinkAndRestorePrimitive() {
        RecordingSink sink = new RecordingSink();
        GhostProfile profile = new GhostProfile();
        profile.channelValues.put("left", 0);
        profile.channelValues.put("right", 0);
        GhostController ghost = new GhostController(sink);
        ghost.activate(profile);
        assertEquals(Integer.valueOf(0), sink.values.get("left"));
        assertEquals(Integer.valueOf(0), sink.values.get("right"));
        ghost.restoreFrmControl();
        assertTrue(sink.restored);
    }

    @Test public void usbDetectorHandlesAttachDetachAndFtdiIdentity() {
        UsbDeviceDescriptor unrelated = new UsbDeviceDescriptor(0x1234, 1, "other", "", true);
        UsbDeviceDescriptor ftdi = new UsbDeviceDescriptor(0x0403, 0x6001, "K+DCAN", "A1", false);
        assertNull(UsbKdcanDetector.findFtdi(Collections.singletonList(unrelated)));
        assertEquals(ftdi, UsbKdcanDetector.findFtdi(Arrays.asList(unrelated, ftdi)));
        assertTrue(ftdi.identity().contains("0403:6001"));
    }

    @Test public void unavailableBridgeNeverPretendsVehicleSuccess() {
        UnavailableEdiabasBridge bridge = new UnavailableEdiabasBridge();
        assertFalse(bridge.isInstalled());
        DiagnosticResult result = bridge.execute(new DiagnosticRequest("FRM", "JOB", Collections.emptyMap()));
        assertEquals(DiagnosticResult.Status.UNAVAILABLE, result.getStatus());
        assertFalse(result.isSimulated());
    }

    @Test public void versionTwoShowParsesLoopsScenesGroupsAndPause() {
        String json = "{\"name\":\"extended\",\"version\":2,\"loop\":true,\"repeatCount\":2,"
                + "\"durationMs\":500,\"timeline\":["
                + "{\"timeMs\":0,\"scene\":\"intro\",\"value\":100,\"transition\":\"FADE\",\"durationMs\":100},"
                + "{\"timeMs\":200,\"group\":\"rear\",\"value\":0},"
                + "{\"timeMs\":300,\"pause\":true,\"value\":0}]}";
        LightShow show = LightShowParser.parse(new StringReader(json));
        assertTrue(show.loop);
        assertEquals(2, show.repeatCount);
        assertEquals("intro", show.timeline.get(0).scene);
        assertEquals("rear", show.timeline.get(1).group);
        assertTrue(show.timeline.get(2).pause);
    }

    @Test public void finiteRepeatPreservesTimingAndRestores() throws Exception {
        CountDownLatch applications = new CountDownLatch(2);
        RecordingSink sink = new RecordingSink() {
            @Override public synchronized void apply(String channel, int value) {
                super.apply(channel, value);
                applications.countDown();
            }
        };
        LightShow show = new LightShow();
        show.name = "repeat";
        show.repeatCount = 2;
        LightShow.Step step = new LightShow.Step();
        step.channel = "left";
        step.value = 100;
        show.timeline.add(step);
        try (LightShowEngine engine = new LightShowEngine(sink)) {
            engine.play(show, 1.0);
            assertTrue(applications.await(500, TimeUnit.MILLISECONDS));
            Thread.sleep(30);
            assertTrue(sink.restored);
        }
    }

    @Test public void presetNamesExistAndUseProvidedBindings() {
        assertEquals(8, PresetLibrary.NAMES.size());
        LightShow show = PresetLibrary.create("Wig-Wag", PresetLibrary.simulatedBindings());
        assertTrue(show.loop);
        assertEquals("SIMULATED_LEFT", show.timeline.get(0).channel);
        assertTrue(show.validate().isEmpty());
    }

    @Test public void musicAnalyzerExtractsEnvelopeBassAndRateLimitsBeats() {
        MusicAnalyzer analyzer = new MusicAnalyzer();
        short[] quiet = new short[256];
        AudioFeatures first = analyzer.analyze(quiet, 16_000, 1_000);
        assertEquals(0.0, first.envelope, 0.0001);
        short[] loud = new short[256];
        Arrays.fill(loud, (short) 20_000);
        AudioFeatures beat = analyzer.analyze(loud, 16_000, 2_000);
        assertTrue(beat.envelope > 0.5);
        assertTrue(beat.bassEnergy > 0);
        assertTrue(beat.beat);
        assertFalse(analyzer.analyze(loud, 16_000, 2_050).beat);
    }

    private static FrmCapability capability() {
        return new FrmCapability("lamp", "Lamp", "FRM_TEST", "STEUERN_LAMPEN_PWM", "PWM_WERT",
                true, true, 0, 100, "DOCUMENTED_SGBD_METADATA");
    }

    private static void expectFailure(Runnable runnable) {
        try { runnable.run(); throw new AssertionError("expected IllegalStateException"); }
        catch (IllegalStateException expected) { assertNotNull(expected.getMessage()); }
    }

    private static class RecordingSink implements LightShowEngine.OutputSink {
        final Map<String, Integer> values = new LinkedHashMap<>();
        volatile boolean restored;
        @Override public synchronized void apply(String channel, int value) { values.put(channel, value); }
        @Override public void restoreControl() { restored = true; }
        @Override public boolean isSimulationOnly() { return true; }
    }

    private static final class RecordingTransport implements DiagnosticTransport {
        final List<DiagnosticRequest> requests = new ArrayList<>();
        @Override public DiagnosticResult connect() { return success(); }
        @Override public void disconnect() {}
        @Override public DiagnosticResult send(DiagnosticRequest request) { requests.add(request); return success(); }
        @Override public List<DiagnosticMessage> receive() { return Collections.emptyList(); }
        @Override public DiagnosticState state() { return DiagnosticState.CONNECTED; }
        @Override public String detail() { return "test"; }
        private DiagnosticResult success() {
            return new DiagnosticResult(DiagnosticResult.Status.SUCCESS, Collections.emptyMap(), "OK", 1, true);
        }
    }
}
