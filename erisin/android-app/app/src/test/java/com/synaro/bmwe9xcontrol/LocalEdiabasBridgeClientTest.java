package com.synaro.bmwe9xcontrol;

import com.google.gson.JsonObject;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticRequest;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;
import com.synaro.bmwe9xcontrol.diagnostic.UsbDeviceDescriptor;
import com.synaro.bmwe9xcontrol.ediabas.BridgeEndpoint;
import com.synaro.bmwe9xcontrol.ediabas.LocalEdiabasBridgeClient;

import org.junit.Test;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.Collections;

import static org.junit.Assert.*;

public class LocalEdiabasBridgeClientTest {
    private static final String TOKEN = "0123456789abcdef0123456789abcdef";

    @Test public void status_and_execute_round_trip_over_loopback_ndjson() throws Exception {
        try (MockServer server = new MockServer((request) -> {
            String method = request.substring(request.indexOf("\"method\":\"") + 10).split("\"")[0];
            if ("status".equals(method)) return ok("{\"bridgeVersion\":\"1.0.0\"}");
            if ("setEcuPath".equals(method)) return ok("{\"ecuPath\":\"private\"}");
            if ("connect".equals(method)) return ok("{\"connected\":true}");
            if ("executeJob".equals(method)) return ok("{\"elapsedMs\":20,\"jobExecutionMs\":18,\"ipcDispatchMs\":2,\"sets\":[],\"ediabasError\":null}");
            return ok("{}");
        })) {
            LocalEdiabasBridgeClient client = client(server.port(), 1000);
            assertEquals("1.0.0", client.implementationVersion());
            assertTrue(client.connect(new UsbDeviceDescriptor(0x0403, 0x6001, "x", "", false), new File(".")).isSuccess());
            DiagnosticResult result = client.execute(new DiagnosticRequest("FRM_87", "IDENT", Collections.emptyMap()));
            assertTrue(result.isSuccess());
            assertEquals("18", result.getValues().get("jobExecutionMs"));
            assertNotNull(result.getValues().get("roundTripMs"));
            assertNotNull(result.getValues().get("ipcOverheadMs"));
            assertEquals("2", result.getValues().get("bridgeDispatchMs"));
        }
    }

    @Test public void malformed_response_fails_closed() throws Exception {
        try (MockServer server = new MockServer((ignored) -> "not-json")) {
            DiagnosticResult result = client(server.port(), 1000).execute(
                    new DiagnosticRequest("FRM_87", "IDENT", Collections.emptyMap()));
            assertEquals(DiagnosticResult.Status.UNAVAILABLE, result.getStatus());
            assertTrue(result.getDetail().startsWith("MALFORMED_RESPONSE"));
        }
    }

    @Test public void timeout_and_disconnect_fail_closed() throws Exception {
        try (MockServer server = new MockServer((ignored) -> { Thread.sleep(500); return ok("{}"); })) {
            DiagnosticResult result = client(server.port(), 50).abortJob();
            assertEquals(DiagnosticResult.Status.UNAVAILABLE, result.getStatus());
            assertTrue(result.getDetail().contains("BRIDGE_TIMEOUT_OR_DISCONNECT"));
        }
        try (ServerSocket unused = new ServerSocket(0)) {
            int port = unused.getLocalPort(); unused.close();
            DiagnosticResult result = client(port, 100).abortJob();
            assertEquals(DiagnosticResult.Status.UNAVAILABLE, result.getStatus());
        }
    }

    @Test public void wrong_protocol_version_is_rejected_before_socket_use() {
        LocalEdiabasBridgeClient client = new LocalEdiabasBridgeClient(
                () -> new BridgeEndpoint("127.0.0.1", 1, TOKEN, 99), () -> true, 10, 10);
        DiagnosticResult result = client.abortJob();
        assertTrue(result.getDetail().contains("PROTOCOL_VERSION_MISMATCH"));
    }

    private static LocalEdiabasBridgeClient client(int port, int timeout) {
        return new LocalEdiabasBridgeClient(() -> new BridgeEndpoint("127.0.0.1", port, TOKEN, 1),
                () -> true, timeout, timeout);
    }

    private static String ok(String result) { return "{\"id\":1,\"ok\":true,\"result\":" + result + "}"; }

    private interface Handler { String handle(String request) throws Exception; }
    private static final class MockServer implements AutoCloseable {
        private final ServerSocket server;
        private final Thread thread;
        private volatile boolean closed;
        MockServer(Handler handler) throws Exception {
            server = new ServerSocket(0, 20, java.net.InetAddress.getByName("127.0.0.1"));
            thread = new Thread(() -> {
                while (!closed) try (Socket socket = server.accept()) {
                    BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
                    BufferedWriter out = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8));
                    String request = in.readLine();
                    String response = handler.handle(request);
                    out.write(response); out.write('\n'); out.flush();
                } catch (Exception ignored) { if (!closed) { /* next request */ } }
            }, "mock-dotnet-bridge");
            thread.start();
        }
        int port() { return server.getLocalPort(); }
        @Override public void close() throws Exception { closed = true; server.close(); thread.join(1000); }
    }
}
