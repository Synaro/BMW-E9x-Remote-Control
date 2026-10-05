package com.synaro.bmwe9xcontrol.diagnostic;

import java.util.List;

/** Transport for explicit diagnostic jobs. It is intentionally separate from VehicleTransport. */
public interface DiagnosticTransport extends AutoCloseable {
    DiagnosticResult connect();
    void disconnect();
    DiagnosticResult send(DiagnosticRequest request);
    List<DiagnosticMessage> receive();
    DiagnosticState state();
    String detail();
    @Override default void close() { disconnect(); }
}
