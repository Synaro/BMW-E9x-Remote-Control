package com.synaro.bmwe9xcontrol.ediabas;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticRequest;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;
import com.synaro.bmwe9xcontrol.diagnostic.UsbDeviceDescriptor;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.Base64;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Locale;
import java.util.concurrent.atomic.AtomicLong;
import java.util.function.Supplier;

/** Authenticated NDJSON client; the rest of the app never sees TCP details. */
public final class LocalEdiabasBridgeClient implements EdiabasBridge {
    private final Gson gson = new Gson();
    private final AtomicLong ids = new AtomicLong();
    private final Supplier<BridgeEndpoint> endpointSupplier;
    private final Supplier<Boolean> installedSupplier;
    private final int connectTimeoutMs;
    private final int readTimeoutMs;
    private volatile String lastJob = "";
    private volatile long lastJobExecutionMs;
    private volatile long lastIpcMs;
    private volatile long lastRoundTripMs;
    private volatile String lastError = "";

    public LocalEdiabasBridgeClient(Supplier<BridgeEndpoint> endpointSupplier,
                                    Supplier<Boolean> installedSupplier) {
        this(endpointSupplier, installedSupplier, 1500, 15000);
    }

    public LocalEdiabasBridgeClient(Supplier<BridgeEndpoint> endpointSupplier,
                                    Supplier<Boolean> installedSupplier, int connectTimeoutMs, int readTimeoutMs) {
        this.endpointSupplier = endpointSupplier;
        this.installedSupplier = installedSupplier;
        this.connectTimeoutMs = connectTimeoutMs;
        this.readTimeoutMs = readTimeoutMs;
    }

    @Override public boolean isInstalled() { return Boolean.TRUE.equals(installedSupplier.get()); }
    @Override public EdiabasBridgeStatus status() {
        if (!isInstalled()) return EdiabasBridgeStatus.unavailable("BRIDGE_APK_NOT_INSTALLED");
        try {
            JsonObject result = call("status", new JsonObject());
            List<UsbDeviceDescriptor> usbDevices = new ArrayList<>();
            JsonArray devices = result.has("usbDevices") ? result.getAsJsonArray("usbDevices") : new JsonArray();
            for (JsonElement element : devices) {
                JsonObject device = element.getAsJsonObject();
                usbDevices.add(new UsbDeviceDescriptor(intValue(device, "vendorId"),
                        intValue(device, "productId"), text(device, "deviceName", ""),
                        text(device, "serialNumber", ""), boolValue(device, "permissionGranted")));
            }
            return new EdiabasBridgeStatus(true, text(result, "bridgeVersion", "unknown"),
                    text(result, "ediabasVersion", "unknown"), usbDevices,
                    boolValue(result, "usbPermissionGranted"), boolValue(result, "ediabasConfigured"),
                    text(result, "ecuPath", ""), text(result, "activeSgbd", ""),
                    text(result, "state", "UNKNOWN"), lastJob, lastJobExecutionMs,
                    lastIpcMs, lastRoundTripMs, lastError);
        } catch (RuntimeException ex) {
            remember(ex);
            return EdiabasBridgeStatus.unavailable(lastError);
        }
    }
    @Override public String implementationVersion() {
        EdiabasBridgeStatus current = status();
        return current.bridgeVersion.isEmpty() ? (isInstalled() ? "installed/not-started" : "not-installed")
                : current.bridgeVersion;
    }

    @Override public DiagnosticResult connect(UsbDeviceDescriptor ignored, File ecuDirectory) {
        try {
            call("setEcuPath", new JsonObject());
            JsonObject result = call("connect", new JsonObject());
            lastError = "";
            return success(result, "EDIABAS configured; FTDI owned by bridge", false);
        } catch (RuntimeException ex) { return failure(ex); }
    }

    @Override public void disconnect() {
        try { call("disconnect", new JsonObject()); } catch (RuntimeException ignored) { }
    }

    @Override public String identifySgbd(File prgFile) {
        try {
            upload(prgFile);
            JsonObject p = new JsonObject(); p.addProperty("sgbd", baseName(prgFile));
            return text(call("identifySgbd", p), "sgbd", "");
        } catch (RuntimeException ex) { return ""; }
    }

    @Override public DiagnosticResult probe(File prgFile) {
        try {
            upload(prgFile);
            JsonObject p = new JsonObject(); p.addProperty("sgbd", baseName(prgFile));
            return success(call("probeSgbd", p), "Read-only SGBD probe", false);
        } catch (RuntimeException ex) { return failure(ex); }
    }

    @Override public List<EdiabasJobDefinition> listJobs(File prgFile) {
        try {
            upload(prgFile);
            JsonObject p = new JsonObject(); p.addProperty("sgbd", baseName(prgFile));
            JsonArray array = callElement("listJobs", p).getAsJsonArray();
            List<EdiabasJobDefinition> jobs = new ArrayList<>();
            for (JsonElement element : array) {
                JsonObject job = element.getAsJsonObject();
                List<EdiabasArgument> arguments = new ArrayList<>();
                JsonArray args = job.has("arguments") ? job.getAsJsonArray("arguments") : new JsonArray();
                for (JsonElement argElement : args) {
                    JsonObject arg = argElement.getAsJsonObject();
                    arguments.add(new EdiabasArgument(text(arg, "name", ""),
                            join(arg.getAsJsonArray("comments")), null, null, Collections.emptyList()));
                }
                List<EdiabasResultDefinition> results = new ArrayList<>();
                JsonArray resultArray = job.has("results") ? job.getAsJsonArray("results") : new JsonArray();
                for (JsonElement resultElement : resultArray) {
                    JsonObject result = resultElement.getAsJsonObject();
                    results.add(new EdiabasResultDefinition(text(result, "name", ""),
                            text(result, "type", ""), strings(result.getAsJsonArray("comments"))));
                }
                jobs.add(new EdiabasJobDefinition(text(job, "name", ""),
                        join(job.getAsJsonArray("comments")), arguments, results));
            }
            return Collections.unmodifiableList(jobs);
        } catch (RuntimeException ex) { return Collections.emptyList(); }
    }

    @Override public DiagnosticResult execute(DiagnosticRequest request) {
        long startedNs = System.nanoTime();
        try {
            JsonObject p = new JsonObject();
            p.addProperty("sgbd", request.getSgbd());
            p.addProperty("job", request.getJob());
            p.addProperty("arguments", encodeArguments(request.getArguments()));
            p.addProperty("results", request.getArguments().getOrDefault("_results", ""));
            JsonObject result = call("executeJob", p);
            long roundTripMs = Math.max(0L, (System.nanoTime() - startedNs) / 1_000_000L);
            long jobExecutionMs = longValue(result, "jobExecutionMs");
            long ipcMs = Math.max(0L, roundTripMs - jobExecutionMs);
            lastJob = request.getSgbd() + "/" + request.getJob();
            lastJobExecutionMs = jobExecutionMs;
            lastIpcMs = ipcMs;
            lastRoundTripMs = roundTripMs;
            Map<String, String> values = new LinkedHashMap<>();
            values.put("sets", result.has("sets") ? result.get("sets").toString() : "[]");
            values.put("roundTripMs", Long.toString(roundTripMs));
            values.put("jobExecutionMs", Long.toString(jobExecutionMs));
            values.put("ipcOverheadMs", Long.toString(ipcMs));
            values.put("bridgeDispatchMs", text(result, "ipcDispatchMs", "0"));
            String error = text(result, "ediabasError", "");
            if (!error.isEmpty()) {
                lastError = error;
                return new DiagnosticResult(DiagnosticResult.Status.ERROR, values, error, roundTripMs, false);
            }
            lastError = "";
            return new DiagnosticResult(DiagnosticResult.Status.SUCCESS, values, "EDIABAS_JOB_OK",
                    roundTripMs, false);
        } catch (RuntimeException ex) { return failure(ex); }
    }

    @Override public DiagnosticResult abortJob() {
        try { return success(call("abortJob", new JsonObject()), "Abort requested", false); }
        catch (RuntimeException ex) { return failure(ex); }
    }

    @Override public String getTrace() {
        try { return text(call("getTrace", new JsonObject()), "trace", ""); }
        catch (RuntimeException ex) { return ""; }
    }

    private void upload(File file) {
        try {
            byte[] bytes = Files.readAllBytes(file.toPath());
            JsonObject p = new JsonObject();
            p.addProperty("fileName", file.getName());
            p.addProperty("base64", Base64.getEncoder().encodeToString(bytes));
            p.addProperty("sha256", hex(MessageDigest.getInstance("SHA-256").digest(bytes)));
            call("uploadPrg", p);
        } catch (Exception ex) { throw new BridgeException("PRG_UPLOAD_FAILED", ex.getMessage()); }
    }

    private JsonObject call(String method, JsonObject params) {
        JsonElement result = callElement(method, params);
        return result == null || result.isJsonNull() ? new JsonObject() : result.getAsJsonObject();
    }

    private JsonElement callElement(String method, JsonObject params) {
        BridgeEndpoint endpoint = endpointSupplier.get();
        if (endpoint == null) throw new BridgeException("BRIDGE_ABSENT", "bridge service has not bootstrapped");
        if (endpoint.protocolVersion() != BridgeEndpoint.SUPPORTED_PROTOCOL_VERSION)
            throw new BridgeException("PROTOCOL_VERSION_MISMATCH", "unsupported bridge protocol");
        JsonObject request = new JsonObject();
        request.addProperty("id", ids.incrementAndGet());
        request.addProperty("version", BridgeEndpoint.SUPPORTED_PROTOCOL_VERSION);
        request.addProperty("token", endpoint.token());
        request.addProperty("method", method);
        request.add("params", params);
        try (Socket socket = new Socket()) {
            socket.connect(new InetSocketAddress(endpoint.host(), endpoint.port()), connectTimeoutMs);
            socket.setSoTimeout(readTimeoutMs);
            BufferedWriter writer = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8));
            BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));
            writer.write(gson.toJson(request)); writer.write('\n'); writer.flush();
            String line = reader.readLine();
            if (line == null) throw new BridgeException("DISCONNECTED", "bridge closed connection");
            JsonObject response = gson.fromJson(line, JsonObject.class);
            if (!response.has("ok") || !response.get("ok").getAsBoolean()) {
                JsonObject error = response.has("error") ? response.getAsJsonObject("error") : new JsonObject();
                throw new BridgeException(text(error, "code", "RPC_ERROR"), text(error, "message", "unknown error"));
            }
            return response.get("result");
        } catch (IOException ex) { throw new BridgeException("BRIDGE_TIMEOUT_OR_DISCONNECT", ex.getMessage()); }
    }

    private static String encodeArguments(Map<String, String> arguments) {
        if (arguments.containsKey("_raw")) return arguments.get("_raw");
        List<String> values = new ArrayList<>();
        for (Map.Entry<String, String> entry : arguments.entrySet())
            if (!entry.getKey().startsWith("_")) values.add(entry.getValue());
        return String.join(";", values);
    }
    private static String baseName(File f) { String n = f.getName(); return n.toUpperCase(Locale.ROOT).endsWith(".PRG") ? n.substring(0, n.length()-4) : n; }
    private static String join(JsonArray values) { if (values == null) return ""; List<String> s = new ArrayList<>(); for (JsonElement e : values) s.add(e.getAsString()); return String.join(" | ", s); }
    private static List<String> strings(JsonArray values) { if (values == null) return Collections.emptyList(); List<String> s = new ArrayList<>(); for (JsonElement e : values) s.add(e.getAsString()); return s; }
    private static String text(JsonObject o, String n, String fallback) { return o != null && o.has(n) && !o.get(n).isJsonNull() ? o.get(n).getAsString() : fallback; }
    private static boolean boolValue(JsonObject o, String n) { return o != null && o.has(n) && o.get(n).getAsBoolean(); }
    private static int intValue(JsonObject o, String n) { return o != null && o.has(n) ? o.get(n).getAsInt() : 0; }
    private static long longValue(JsonObject o, String n) { return o != null && o.has(n) ? o.get(n).getAsLong() : 0; }
    private static String hex(byte[] b) { StringBuilder s = new StringBuilder(); for (byte v : b) s.append(String.format("%02x", v)); return s.toString(); }
    private static DiagnosticResult success(JsonObject result, String detail, boolean simulated) { Map<String,String> v=new LinkedHashMap<>(); v.put("result", result.toString()); return new DiagnosticResult(DiagnosticResult.Status.SUCCESS,v,detail,0,simulated); }
    private DiagnosticResult failure(RuntimeException ex) {
        remember(ex);
        String code = ex instanceof BridgeException ? ((BridgeException) ex).code : "MALFORMED_RESPONSE";
        DiagnosticResult.Status s = "COMMAND_BUSY".equals(code) ? DiagnosticResult.Status.REJECTED : DiagnosticResult.Status.UNAVAILABLE;
        return new DiagnosticResult(s, Collections.emptyMap(), code + ": " + ex.getMessage(),0,false);
    }

    private void remember(RuntimeException ex) {
        String code = ex instanceof BridgeException ? ((BridgeException) ex).code : "MALFORMED_RESPONSE";
        lastError = code + ": " + (ex.getMessage() == null ? "" : ex.getMessage());
    }

    private static final class BridgeException extends RuntimeException {
        final String code;
        BridgeException(String code, String message) { super(message == null ? "" : message); this.code = code; }
    }
}
