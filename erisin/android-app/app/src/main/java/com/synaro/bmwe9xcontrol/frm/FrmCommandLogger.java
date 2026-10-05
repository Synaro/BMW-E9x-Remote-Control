package com.synaro.bmwe9xcontrol.frm;

import com.google.gson.Gson;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticRequest;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.LinkedHashMap;
import java.util.Map;

public final class FrmCommandLogger {
    private final File file;
    private final Gson gson = new Gson();

    public FrmCommandLogger(File file) { this.file = file; }

    public synchronized void append(long timestampMs, DiagnosticRequest request, DiagnosticResult result)
            throws IOException {
        File parent = file.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) throw new IOException("cannot create log directory");
        Map<String, Object> record = new LinkedHashMap<>();
        record.put("timestampMs", timestampMs);
        record.put("sgbd", request.getSgbd());
        record.put("job", request.getJob());
        record.put("arguments", request.getArguments());
        record.put("result", result.getValues());
        record.put("elapsedMs", result.getElapsedMs());
        record.put("status", result.getStatus().name());
        record.put("simulated", result.isSimulated());
        try (FileWriter writer = new FileWriter(file, true)) {
            writer.write(gson.toJson(record));
            writer.write("\n");
        }
    }

    public File file() { return file; }
}
