package com.synaro.bmwe9xcontrol.ediabas;

import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticRequest;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;
import com.synaro.bmwe9xcontrol.diagnostic.UsbDeviceDescriptor;

import java.io.File;
import java.util.List;

/**
 * Boundary around the GPL EdiabasLib Android runtime.
 *
 * EdiabasLib is .NET for Android, while this APK is Java/Gradle. The separately
 * installed .NET Android bridge implements this boundary through
 * authenticated loopback NDJSON. No .NET assembly is loaded into the JVM.
 */
public interface EdiabasBridge {
    boolean isInstalled();
    String implementationVersion();
    DiagnosticResult connect(UsbDeviceDescriptor device, File ecuDirectory);
    void disconnect();
    String identifySgbd(File prgFile);
    DiagnosticResult probe(File prgFile);
    List<EdiabasJobDefinition> listJobs(File prgFile);
    DiagnosticResult execute(DiagnosticRequest request);
    DiagnosticResult abortJob();
    String getTrace();
}
