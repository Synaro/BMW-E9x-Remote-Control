package com.synaro.bmwe9xcontrol.service;

import android.os.Build;

import com.synaro.bmwe9xcontrol.model.DeviceProfile;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.TimeUnit;

public final class DeviceProfileCollector {
    private DeviceProfileCollector() {}

    public static DeviceProfile collectBasic() {
        DeviceProfile profile = new DeviceProfile();
        profile.manufacturer = Build.MANUFACTURER;
        profile.model = Build.MODEL;
        profile.androidRelease = Build.VERSION.RELEASE;
        profile.apiLevel = Build.VERSION.SDK_INT;
        profile.fingerprint = Build.FINGERPRINT;
        boolean root = probeCommand(new String[] {"id"}, "uid=0")
                || probeCommand(new String[] {"su", "-c", "id"}, "uid=0");
        profile.rootState = root ? "YES" : "NO_OR_UNAVAILABLE";
        String enforce = readCommand(new String[] {"getenforce"});
        profile.selinux = enforce.isEmpty() ? "UNAVAILABLE" : enforce.trim();
        return profile;
    }

    static boolean probeCommand(String[] command, String marker) {
        return readCommand(command).contains(marker);
    }

    static String readCommand(String[] command) {
        Process process = null;
        try {
            process = new ProcessBuilder(command).redirectErrorStream(true).start();
            StringBuilder output = new StringBuilder();
            try (BufferedReader reader = new BufferedReader(new InputStreamReader(
                    process.getInputStream(), StandardCharsets.UTF_8))) {
                String line;
                while ((line = reader.readLine()) != null && output.length() < 32_768) {
                    output.append(line).append('\n');
                }
            }
            if (!process.waitFor(2, TimeUnit.SECONDS)) process.destroyForcibly();
            return output.toString();
        } catch (IOException | InterruptedException ignored) {
            if (ignored instanceof InterruptedException) Thread.currentThread().interrupt();
            return "";
        } finally {
            if (process != null) process.destroy();
        }
    }
}
