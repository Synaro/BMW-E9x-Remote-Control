package com.synaro.bmwe9xcontrol.service;

import android.content.Context;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.synaro.bmwe9xcontrol.model.DeviceProfile;
import com.synaro.bmwe9xcontrol.model.RawEvent;
import com.synaro.bmwe9xcontrol.model.VehicleSnapshot;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.io.Writer;
import java.nio.charset.StandardCharsets;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

public final class EvidenceExporter {
    private EvidenceExporter() {}

    public static File export(Context context, DeviceProfile profile, String transport,
                              VehicleSnapshot snapshot, List<RawEvent> events) throws IOException {
        File base = context.getExternalFilesDir("exports");
        if (base == null) base = new File(context.getFilesDir(), "exports");
        if (!base.exists() && !base.mkdirs()) throw new IOException("cannot create export directory");
        File output = new File(base, "bmw-e9x-control-" + System.currentTimeMillis() + ".json");
        Map<String, Object> document = new LinkedHashMap<>();
        document.put("schema_version", 1);
        document.put("exported_at_ms", System.currentTimeMillis());
        document.put("profile", profile);
        document.put("transport", transport);
        document.put("latest_snapshot", snapshot);
        document.put("events", events);
        Gson gson = new GsonBuilder().setPrettyPrinting().create();
        try (Writer writer = new OutputStreamWriter(
                new FileOutputStream(output), StandardCharsets.UTF_8)) {
            gson.toJson(document, writer);
        }
        return output;
    }
}
