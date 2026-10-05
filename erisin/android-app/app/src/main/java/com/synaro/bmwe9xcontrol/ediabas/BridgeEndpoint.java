package com.synaro.bmwe9xcontrol.ediabas;

/** Ephemeral bootstrap data. The token is intentionally never persisted. */
public final class BridgeEndpoint {
    public static final int SUPPORTED_PROTOCOL_VERSION = 1;
    private final String host;
    private final int port;
    private final String token;
    private final int protocolVersion;

    public BridgeEndpoint(String host, int port, String token, int protocolVersion) {
        if (!"127.0.0.1".equals(host)) throw new IllegalArgumentException("bridge must use loopback");
        if (port < 1 || port > 65535) throw new IllegalArgumentException("invalid port");
        if (token == null || token.length() < 32) throw new IllegalArgumentException("invalid token");
        this.host = host;
        this.port = port;
        this.token = token;
        this.protocolVersion = protocolVersion;
    }
    public String host() { return host; }
    public int port() { return port; }
    public String token() { return token; }
    public int protocolVersion() { return protocolVersion; }
}
