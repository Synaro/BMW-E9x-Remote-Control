package com.synaro.bmwe9xcontrol.ediabas;

import android.content.Context;
import android.net.Uri;
import android.provider.OpenableColumns;
import android.database.Cursor;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Locale;

public final class EdiabasFileStore {
    private final Context context;
    private final File ecuDirectory;

    public EdiabasFileStore(Context context) {
        this.context = context.getApplicationContext();
        this.ecuDirectory = new File(context.getFilesDir(), "ediabas/ecu");
        if (!ecuDirectory.exists() && !ecuDirectory.mkdirs()) {
            throw new IllegalStateException("unable to create private EDIABAS ECU directory");
        }
    }

    public File directory() { return ecuDirectory; }

    public File[] importedPrgFiles() {
        File[] files = ecuDirectory.listFiles((dir, name) -> name.toUpperCase(Locale.ROOT).endsWith(".PRG"));
        if (files == null) return new File[0];
        Arrays.sort(files, Comparator.comparing(File::getName));
        return files;
    }

    public File importPrg(Uri uri) throws IOException {
        String name = displayName(uri);
        if (name == null || !name.toUpperCase(Locale.ROOT).endsWith(".PRG")) {
            throw new IOException("only .PRG files are accepted");
        }
        name = sanitize(name);
        File destination = new File(ecuDirectory, name);
        try (InputStream input = context.getContentResolver().openInputStream(uri);
             FileOutputStream output = new FileOutputStream(destination)) {
            if (input == null) throw new IOException("content provider returned no stream");
            byte[] buffer = new byte[8192];
            int count;
            while ((count = input.read(buffer)) >= 0) output.write(buffer, 0, count);
        }
        return destination;
    }

    private String displayName(Uri uri) {
        try (Cursor cursor = context.getContentResolver().query(uri, null, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (index >= 0) return cursor.getString(index);
            }
        }
        return uri.getLastPathSegment();
    }

    private static String sanitize(String name) {
        return name.replaceAll("[^A-Za-z0-9._-]", "_");
    }
}
