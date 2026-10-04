package com.synaro.bmwe9xcontrol.ui;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;

import com.synaro.bmwe9xcontrol.R;
import com.synaro.bmwe9xcontrol.model.DeviceProfile;
import com.synaro.bmwe9xcontrol.model.RawEvent;
import com.synaro.bmwe9xcontrol.model.VehicleSnapshot;
import com.synaro.bmwe9xcontrol.repository.VehicleRepository;
import com.synaro.bmwe9xcontrol.service.DeviceProfileCollector;
import com.synaro.bmwe9xcontrol.service.EvidenceExporter;
import com.synaro.bmwe9xcontrol.transport.MockTransport;
import com.synaro.bmwe9xcontrol.transport.ReplayTransport;
import com.synaro.bmwe9xcontrol.transport.TransportStats;

import java.io.File;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.text.DateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public final class MainActivity extends Activity implements VehicleRepository.Observer {
    private static final int OPEN_REPLAY_REQUEST = 1001;
    private final DateFormat clock = DateFormat.getTimeInstance(DateFormat.MEDIUM, Locale.ROOT);
    private VehicleRepository repository;
    private DeviceProfile profile = new DeviceProfile();
    private TextView title;
    private TextView content;
    private String selectedPage = "Diagnostics";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        repository = new VehicleRepository(new MockTransport());
        repository.addObserver(this);
        setContentView(buildUi());
        new Thread(() -> {
            profile = DeviceProfileCollector.collectBasic();
            runOnUiThread(this::render);
        }, "device-profile-readonly").start();
        try {
            repository.start();
        } catch (Exception ex) {
            Toast.makeText(this, "Transport: " + ex.getMessage(), Toast.LENGTH_LONG).show();
        }
        render();
    }

    private View buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(24, 16, 24, 16);
        root.setBackgroundColor(Color.rgb(16, 18, 22));

        title = text(24, Color.WHITE);
        title.setText(R.string.read_only_title);
        root.addView(title);

        HorizontalScrollView navScroller = new HorizontalScrollView(this);
        LinearLayout nav = new LinearLayout(this);
        nav.setOrientation(LinearLayout.HORIZONTAL);
        addNav(nav, "Diagnostics");
        addNav(nav, "Live Data");
        addNav(nav, "Raw Events");
        Button mock = button("Use Mock");
        mock.setOnClickListener(view -> switchToMock());
        nav.addView(mock);
        Button replay = button("Replay demo");
        replay.setOnClickListener(view -> switchToReplay());
        nav.addView(replay);
        Button loadReplay = button("Load replay");
        loadReplay.setOnClickListener(view -> selectReplayFile());
        nav.addView(loadReplay);
        Button export = button("Export JSON");
        export.setOnClickListener(view -> exportEvidence());
        nav.addView(export);
        navScroller.addView(nav);
        root.addView(navScroller);

        content = text(17, Color.rgb(225, 230, 235));
        content.setTypeface(android.graphics.Typeface.MONOSPACE);
        content.setMovementMethod(new ScrollingMovementMethod());
        ScrollView scroll = new ScrollView(this);
        scroll.addView(content);
        root.addView(scroll, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));
        return root;
    }

    private void addNav(LinearLayout nav, String page) {
        Button button = button(page);
        button.setOnClickListener(view -> { selectedPage = page; render(); });
        nav.addView(button);
    }

    private Button button(String label) {
        Button button = new Button(this);
        button.setText(label);
        button.setAllCaps(false);
        button.setGravity(Gravity.CENTER);
        return button;
    }

    private TextView text(int sp, int color) {
        TextView view = new TextView(this);
        view.setTextSize(sp);
        view.setTextColor(color);
        return view;
    }

    @Override public void onRepositoryChanged() { runOnUiThread(this::render); }

    private void render() {
        if (content == null) return;
        if ("Live Data".equals(selectedPage)) renderLive();
        else if ("Raw Events".equals(selectedPage)) renderEvents();
        else renderDiagnostics();
    }

    private void renderDiagnostics() {
        TransportStats stats = repository.stats();
        StringBuilder out = new StringBuilder();
        line(out, "DEVICE", profile.manufacturer + " " + profile.model);
        line(out, "ANDROID", profile.androidRelease + " / API " + profile.apiLevel);
        line(out, "FINGERPRINT", profile.fingerprint);
        line(out, "XRC VERSION", profile.xrcVersion + " (collect with ADB script)");
        line(out, "MCU VERSION", profile.mcuVersion + " (collect with ADB script)");
        line(out, "ROOT", profile.rootState);
        line(out, "SELINUX", profile.selinux);
        line(out, "TRANSPORT", repository.transportName());
        line(out, "STATE", repository.state() + " — " + repository.stateDetail());
        line(out, "RX / TX / ERR", stats.getRxCount() + " / " + stats.getTxCount() + " / " + stats.getErrorCount());
        List<RawEvent> events = repository.events();
        line(out, "LAST EVENT", events.isEmpty() ? "none" : formatEvent(events.get(events.size() - 1)));
        out.append("\nNo vehicle TX API is present in this build.\n");
        content.setText(out.toString());
    }

    private void renderLive() {
        VehicleSnapshot snapshot = repository.latest();
        if (snapshot == null) { content.setText(R.string.waiting_for_data); return; }
        StringBuilder out = new StringBuilder();
        line(out, "ORIGIN", snapshot.getOrigin().name());
        line(out, "TIMESTAMP", Long.toString(snapshot.getTimestampMs()));
        for (Map.Entry<String, Object> entry : snapshot.getValues().entrySet()) {
            line(out, entry.getKey().toUpperCase(Locale.ROOT), String.valueOf(entry.getValue()));
        }
        out.append("\nValues labelled SIMULATED are not vehicle observations.\n");
        content.setText(out.toString());
    }

    private void renderEvents() {
        List<RawEvent> events = repository.events();
        StringBuilder out = new StringBuilder();
        int first = Math.max(0, events.size() - 80);
        for (int i = first; i < events.size(); i++) out.append(formatEvent(events.get(i))).append('\n');
        content.setText(out.toString());
    }

    private String formatEvent(RawEvent event) {
        return clock.format(new Date(event.getTimestampMs())) + " | " + event.getOrigin() + " | "
                + event.getSource() + " | " + event.getType() + " | " + event.getId() + " | "
                + event.getPayload() + " | " + event.getDecodedValue() + " | " + event.getConfidence();
    }

    private static void line(StringBuilder out, String name, String value) {
        out.append(String.format(Locale.ROOT, "%-14s %s%n", name + ":", value));
    }

    private void exportEvidence() {
        try {
            File file = EvidenceExporter.export(this, profile, repository.transportName(),
                    repository.latest(), repository.events());
            Toast.makeText(this, "Exported: " + file.getAbsolutePath(), Toast.LENGTH_LONG).show();
        } catch (Exception ex) {
            Toast.makeText(this, "Export failed: " + ex.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void switchToMock() {
        replaceRepository(new VehicleRepository(new MockTransport()));
    }

    private void switchToReplay() {
        try {
            ReplayTransport replay = new ReplayTransport(new InputStreamReader(
                    getAssets().open("replay_demo.ndjson"), StandardCharsets.UTF_8), 1.0);
            replaceRepository(new VehicleRepository(replay));
        } catch (Exception ex) {
            Toast.makeText(this, "Replay failed: " + ex.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void selectReplayFile() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, OPEN_REPLAY_REQUEST);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != OPEN_REPLAY_REQUEST || resultCode != RESULT_OK || data == null) return;
        Uri uri = data.getData();
        if (uri == null) return;
        try {
            java.io.InputStream input = getContentResolver().openInputStream(uri);
            if (input == null) throw new java.io.IOException("content provider returned no stream");
            ReplayTransport replay = new ReplayTransport(
                    new InputStreamReader(input, StandardCharsets.UTF_8), 1.0);
            replaceRepository(new VehicleRepository(replay));
        } catch (Exception ex) {
            Toast.makeText(this, "Replay failed: " + ex.getMessage(), Toast.LENGTH_LONG).show();
        }
    }

    private void replaceRepository(VehicleRepository next) {
        repository.removeObserver(this);
        repository.close();
        repository = next;
        repository.addObserver(this);
        try {
            repository.start();
        } catch (Exception ex) {
            Toast.makeText(this, "Transport: " + ex.getMessage(), Toast.LENGTH_LONG).show();
        }
        render();
    }

    @Override protected void onDestroy() {
        repository.removeObserver(this);
        repository.close();
        super.onDestroy();
    }
}
