package com.synaro.bmwe9xcontrol.lightshow;

import android.content.Context;

import com.google.gson.GsonBuilder;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

public final class LightShowStore {
    private final File directory;

    public LightShowStore(Context context) {
        directory = new File(context.getFilesDir(), "lightshows");
        if (!directory.exists() && !directory.mkdirs()) throw new IllegalStateException("cannot create lightshows directory");
    }

    public File save(LightShow show) throws IOException {
        java.util.List<String> errors = show.validate();
        if (!errors.isEmpty()) throw new IllegalArgumentException(String.join("; ", errors));
        String name = show.name.replaceAll("[^A-Za-z0-9._-]", "_");
        File destination = new File(directory, name + ".json");
        try (FileWriter writer = new FileWriter(destination)) {
            new GsonBuilder().setPrettyPrinting().create().toJson(show, writer);
        }
        return destination;
    }

    public LightShow load(File file) throws IOException {
        try (FileReader reader = new FileReader(file)) { return LightShowParser.parse(reader); }
    }
    public File directory() { return directory; }
}
