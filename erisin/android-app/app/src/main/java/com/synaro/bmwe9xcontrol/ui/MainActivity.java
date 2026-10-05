package com.synaro.bmwe9xcontrol.ui;

import android.Manifest;
import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.text.InputType;
import android.view.Gravity;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.GridLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.SeekBar;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import com.synaro.bmwe9xcontrol.R;
import com.synaro.bmwe9xcontrol.diagnostic.DiagnosticResult;
import com.synaro.bmwe9xcontrol.diagnostic.UsbDeviceDescriptor;
import com.synaro.bmwe9xcontrol.diagnostic.UsbKdcanTransport;
import com.synaro.bmwe9xcontrol.ediabas.EdiabasFileStore;
import com.synaro.bmwe9xcontrol.ediabas.EdiabasSession;
import com.synaro.bmwe9xcontrol.ediabas.BridgeEndpointRegistry;
import com.synaro.bmwe9xcontrol.ediabas.EdiabasBridge;
import com.synaro.bmwe9xcontrol.ediabas.EdiabasBridgeLauncher;
import com.synaro.bmwe9xcontrol.ediabas.LocalEdiabasBridgeClient;
import com.synaro.bmwe9xcontrol.frm.FrmCapability;
import com.synaro.bmwe9xcontrol.frm.FrmCommandLogger;
import com.synaro.bmwe9xcontrol.frm.FrmDiscoveryResult;
import com.synaro.bmwe9xcontrol.frm.FrmLightSink;
import com.synaro.bmwe9xcontrol.frm.GhostController;
import com.synaro.bmwe9xcontrol.frm.GhostProfile;
import com.synaro.bmwe9xcontrol.lightshow.LightShow;
import com.synaro.bmwe9xcontrol.lightshow.LightShowEngine;
import com.synaro.bmwe9xcontrol.lightshow.LightShowStore;
import com.synaro.bmwe9xcontrol.lightshow.PresetLibrary;
import com.synaro.bmwe9xcontrol.lightshow.SimulatedLightSink;
import com.synaro.bmwe9xcontrol.model.DeviceProfile;
import com.synaro.bmwe9xcontrol.model.RawEvent;
import com.synaro.bmwe9xcontrol.model.VehicleSnapshot;
import com.synaro.bmwe9xcontrol.music.AudioFeatures;
import com.synaro.bmwe9xcontrol.music.MusicAnalyzer;
import com.synaro.bmwe9xcontrol.music.MusicSyncController;
import com.synaro.bmwe9xcontrol.repository.VehicleRepository;
import com.synaro.bmwe9xcontrol.service.DeviceProfileCollector;
import com.synaro.bmwe9xcontrol.service.EvidenceExporter;
import com.synaro.bmwe9xcontrol.transport.MockTransport;
import com.synaro.bmwe9xcontrol.transport.ReplayTransport;
import com.synaro.bmwe9xcontrol.transport.TransportStats;

import java.io.File;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.nio.charset.StandardCharsets;
import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public final class MainActivity extends Activity implements VehicleRepository.Observer, MusicSyncController.Listener {
    private static final int OPEN_REPLAY = 1001;
    private static final int IMPORT_PRG = 1002;
    private static final int LOAD_SHOW = 1003;
    private static final int RECORD_AUDIO_PERMISSION = 1004;
    private final DateFormat clock = DateFormat.getTimeInstance(DateFormat.MEDIUM, Locale.ROOT);

    private VehicleRepository repository;
    private DeviceProfile profile = new DeviceProfile();
    private String selectedPage = "HOME";
    private LinearLayout page;
    private TextView title;
    private EdiabasFileStore ediabasFiles;
    private EdiabasBridge ediabasBridge;
    private EdiabasBridgeLauncher bridgeLauncher;
    private UsbKdcanTransport diagnosticTransport;
    private EdiabasSession ediabasSession;
    private FrmDiscoveryResult frm = new FrmDiscoveryResult(null, "", new ArrayList<>(), "Not scanned");
    private final SimulatedLightSink simulatedSink = new SimulatedLightSink();
    private LightShowEngine.OutputSink activeSink = simulatedSink;
    private LightShowEngine showEngine = new LightShowEngine(simulatedSink);
    private LightShow editingShow = new LightShow();
    private LightShowStore showStore;
    private MusicSyncController music;
    private AudioFeatures latestAudio;
    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        repository = new VehicleRepository(new MockTransport());
        repository.addObserver(this);
        ediabasFiles = new EdiabasFileStore(this);
        bridgeLauncher = new EdiabasBridgeLauncher(this);
        ediabasBridge = new LocalEdiabasBridgeClient(BridgeEndpointRegistry::get, bridgeLauncher::isInstalled);
        bridgeLauncher.start();
        diagnosticTransport = new UsbKdcanTransport(this, ediabasBridge, ediabasFiles.directory());
        ediabasSession = new EdiabasSession(ediabasBridge, ediabasFiles);
        showStore = new LightShowStore(this);
        editingShow.name = "Custom 1";
        setContentView(buildUi());
        new Thread(() -> {
            profile = DeviceProfileCollector.collectBasic();
            runOnUiThread(this::render);
        }, "device-profile-readonly").start();
        try { repository.start(); } catch (Exception ex) { toast("Transport: " + ex.getMessage()); }
        render();
    }

    private View buildUi() {
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(20, 12, 20, 12);
        root.setBackgroundColor(Color.rgb(12, 15, 19));
        title = text(28, Color.WHITE);
        title.setText(R.string.read_only_title);
        root.addView(title);
        HorizontalScrollView scroller = new HorizontalScrollView(this);
        LinearLayout nav = new LinearLayout(this);
        nav.setOrientation(LinearLayout.HORIZONTAL);
        for (String name : new String[]{"HOME", "LIGHTS", "GHOST", "SHOWS", "MUSIC", "DIAGNOSTICS", "SETTINGS"}) {
            Button button = button(name);
            button.setOnClickListener(v -> { selectedPage = name; render(); });
            nav.addView(button);
        }
        scroller.addView(nav);
        root.addView(scroller);
        ScrollView scroll = new ScrollView(this);
        page = new LinearLayout(this);
        page.setOrientation(LinearLayout.VERTICAL);
        page.setPadding(8, 12, 8, 32);
        scroll.addView(page);
        root.addView(scroll, new LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, 0, 1));
        return root;
    }

    private void render() {
        if (page == null) return;
        page.removeAllViews();
        title.setText(getString(R.string.page_title_format, selectedPage));
        switch (selectedPage) {
            case "LIGHTS": renderLights(); break;
            case "GHOST": renderGhost(); break;
            case "SHOWS": renderShows(); break;
            case "MUSIC": renderMusic(); break;
            case "DIAGNOSTICS": renderDiagnostics(); break;
            case "SETTINGS": renderSettings(); break;
            default: renderHome();
        }
    }

    private void renderHome() {
        heading("BMW E9x LIGHT CONTROL");
        info("Transport télémétrie", repository.transportName() + " / " + repository.state());
        info("Transport diagnostic", diagnosticTransport.state() + " — " + diagnosticTransport.detail());
        info("Mode sorties", activeSink.isSimulationOnly() ? "SIMULATED" : "FRM / EDIABAS");
        info("FRM", frm.sgbd.isEmpty() ? "NOT DETECTED" : frm.sgbd);
        info("Sécurité", "Aucune commande réelle sans pont EdiabasLib, SGBD importé, connexion et capacité armée.");
        VehicleSnapshot snapshot = repository.latest();
        if (snapshot != null) info("Live data", snapshot.getOrigin() + " — " + snapshot.getValues());
    }

    private void renderLights() {
        heading("MANUAL LIGHT CONTROL");
        if (frm.capabilities.isEmpty()) {
            info("Aucune capacité", "Importez les PRG puis lancez FRM discovery. Les noms de lampes ne sont jamais inventés.");
            addSimulationControls();
            return;
        }
        for (FrmCapability capability : frm.capabilities) {
            LinearLayout row = horizontal();
            TextView label = text(17, Color.WHITE);
            label.setText(getString(R.string.capability_format, capability.label, capability.ediabasJob,
                    capability.valueArgument, capability.isArmed() ? "ARMED" : "REVIEW REQUIRED"));
            row.addView(label, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
            Button arm = button(capability.isArmed() ? "DISARM" : "ARM");
            arm.setOnClickListener(v -> { capability.setArmed(!capability.isArmed()); render(); });
            row.addView(arm);
            Button toggle = button("ON");
            toggle.setEnabled(capability.isArmed());
            toggle.setOnClickListener(v -> executeLight(capability.id, 100));
            row.addView(toggle);
            page.addView(row);
            if (capability.supportsPwm) {
                SeekBar pwm = new SeekBar(this);
                pwm.setMax(100);
                pwm.setProgress(50);
                pwm.setOnSeekBarChangeListener(onStop(value -> executeLight(capability.id, value)));
                page.addView(pwm);
            }
        }
    }

    private void addSimulationControls() {
        heading("SIMULATION");
        GridLayout grid = new GridLayout(this);
        grid.setColumnCount(2);
        for (String channel : new String[]{"SIMULATED_LEFT", "SIMULATED_RIGHT", "SIMULATED_DRL_LEFT", "SIMULATED_DRL_RIGHT"}) {
            Button button = button(channel);
            button.setOnClickListener(v -> {
                int next = simulatedSink.values().getOrDefault(channel, 0) == 0 ? 100 : 0;
                simulatedSink.apply(channel, next);
                toast(channel + " = " + next + " (SIMULATED)");
            });
            grid.addView(button);
        }
        page.addView(grid);
    }

    private void renderGhost() {
        heading("GHOST MODE — TEMPORARY OVERRIDES");
        info("Règle", "Aucun codage FSW/PSW. RESTORE exige un job de restitution qualifié pour chaque capacité réelle.");
        page.addView(action("ACTIVATE GHOST", v -> {
            GhostProfile ghost = new GhostProfile();
            if (activeSink.isSimulationOnly()) {
                for (String channel : PresetLibrary.simulatedBindings().values()) ghost.channelValues.put(channel, 0);
            } else {
                for (FrmCapability capability : frm.capabilities) if (capability.isArmed()) ghost.channelValues.put(capability.id, 0);
            }
            try { new GhostController(activeSink).activate(ghost); toast("Ghost active — " + modeLabel()); }
            catch (RuntimeException ex) { toast("Ghost rejected: " + ex.getMessage()); }
        }));
        page.addView(action("RESTORE FRM CONTROL", v -> {
            new GhostController(activeSink).restoreFrmControl();
            toast("Restore requested — " + modeLabel());
        }));
    }

    private void renderShows() {
        heading("LIGHT SHOW EDITOR");
        LinearLayout presets = horizontal();
        for (String name : PresetLibrary.NAMES) {
            Button preset = button(name);
            preset.setOnClickListener(v -> { editingShow = PresetLibrary.create(name, PresetLibrary.simulatedBindings()); render(); });
            presets.addView(preset);
        }
        HorizontalScrollView presetScroll = new HorizontalScrollView(this);
        presetScroll.addView(presets);
        page.addView(presetScroll);
        info("Show", editingShow.name + " — " + editingShow.timeline.size() + " steps — loop=" + editingShow.loop);
        for (int i = 0; i < editingShow.timeline.size(); i++) {
            int index = i;
            LightShow.Step step = editingShow.timeline.get(i);
            LinearLayout row = horizontal();
            TextView value = text(15, Color.LTGRAY);
            value.setText(getString(R.string.show_step_format, i, step.timeMs, step.channel,
                    step.value, step.transition, step.durationMs));
            row.addView(value, new LinearLayout.LayoutParams(0, LinearLayout.LayoutParams.WRAP_CONTENT, 1));
            row.addView(action("DUP", v -> { editingShow.timeline.add(index + 1, copy(step)); render(); }));
            row.addView(action("↑", v -> { if (index > 0) { java.util.Collections.swap(editingShow.timeline, index, index - 1); normalizeTimes(); render(); }}));
            row.addView(action("↓", v -> { if (index + 1 < editingShow.timeline.size()) { java.util.Collections.swap(editingShow.timeline, index, index + 1); normalizeTimes(); render(); }}));
            row.addView(action("DEL", v -> { editingShow.timeline.remove(index); render(); }));
            page.addView(row);
        }
        addStepEditor();
        LinearLayout actions = horizontal();
        actions.addView(action("PLAY", v -> playCurrentShow()));
        actions.addView(action("STOP", v -> showEngine.stop()));
        actions.addView(action("SAVE", v -> saveShow()));
        actions.addView(action("LOAD", v -> selectFile(LOAD_SHOW, "application/json")));
        actions.addView(action(editingShow.loop ? "LOOP: ON" : "LOOP: OFF", v -> { editingShow.loop = !editingShow.loop; render(); }));
        page.addView(actions);
    }

    private void addStepEditor() {
        LinearLayout editor = horizontal();
        EditText time = number("Time ms", "0");
        EditText duration = number("Duration", "0");
        EditText channel = input("Channel", activeSink.isSimulationOnly() ? "SIMULATED_LEFT" : "");
        EditText value = number("Value", "100");
        Spinner transition = new Spinner(this);
        transition.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, new String[]{"STEP", "FADE"}));
        editor.addView(time); editor.addView(duration); editor.addView(channel); editor.addView(value); editor.addView(transition);
        editor.addView(action("+ ADD STEP", v -> {
            try {
                LightShow.Step step = new LightShow.Step();
                step.timeMs = Long.parseLong(time.getText().toString());
                step.durationMs = Long.parseLong(duration.getText().toString());
                step.channel = channel.getText().toString().trim();
                step.value = Integer.parseInt(value.getText().toString());
                step.transition = String.valueOf(transition.getSelectedItem());
                editingShow.timeline.add(step);
                editingShow.timeline.sort(java.util.Comparator.comparingLong(s -> s.timeMs));
                render();
            } catch (RuntimeException ex) { toast("Invalid step: " + ex.getMessage()); }
        }));
        page.addView(editor);
    }

    private void renderMusic() {
        heading("MUSIC MODE");
        info("Source", "Android AudioRecord — volume envelope, bass FFT, beat trigger");
        info("Latest", latestAudio == null ? "No samples" : String.format(Locale.ROOT,
                "volume %.3f / bass %.5f / beat %s", latestAudio.envelope, latestAudio.bassEnergy, latestAudio.beat));
        info("Sensitivity", "1.0 — 5.0");
        SeekBar sensitivity = new SeekBar(this);
        sensitivity.setMax(40); sensitivity.setProgress(5);
        page.addView(sensitivity);
        LinearLayout buttons = horizontal();
        buttons.addView(action("START MUSIC", v -> {
            if (checkSelfPermission(Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO}, RECORD_AUDIO_PERMISSION);
                return;
            }
            MusicAnalyzer analyzer = new MusicAnalyzer();
            analyzer.setSensitivity(1.0 + sensitivity.getProgress() / 10.0);
            music = new MusicSyncController(analyzer, this);
            music.start();
        }));
        buttons.addView(action("STOP MUSIC", v -> { if (music != null) music.stop(); }));
        page.addView(buttons);
        info("Mapping", "Beat/energy events are ready for presets; real output still requires armed discovered capabilities.");
    }

    private void renderDiagnostics() {
        heading("FRM DIAGNOSTICS");
        UsbDeviceDescriptor adapter = diagnosticTransport.detectedAdapter();
        info("DEVICE", profile.manufacturer + " " + profile.model);
        info("ANDROID", profile.androidRelease + " / API " + profile.apiLevel);
        info("FINGERPRINT", profile.fingerprint);
        info("XRC / MCU", profile.xrcVersion + " / " + profile.mcuVersion);
        info("ROOT / SELINUX", profile.rootState + " / " + profile.selinux);
        info("USB FTDI", adapter == null ? "NOT DETECTED IN THIS ENVIRONMENT" :
                adapter.identity() + " — permission and handle owned by .NET bridge");
        info("EDIABAS", ediabasBridge.implementationVersion() + " — " + (ediabasBridge.isInstalled() ? "INSTALLED" : "BRIDGE NOT INSTALLED"));
        info("PRG directory", ediabasFiles.directory().getAbsolutePath());
        info("Imported PRG", Integer.toString(ediabasFiles.importedPrgFiles().length));
        info("FRM detected", frm.sgbd.isEmpty() ? "NO" : frm.sgbd);
        info("SGBD", frm.prgFile == null ? "none" : frm.prgFile.getName());
        info("Interface", "USB K+DCAN");
        info("Status", frm.status);
        LinearLayout actions = horizontal();
        actions.addView(action("START BRIDGE", v -> { toast(bridgeLauncher.start() ? "Bridge start requested" : "Bridge APK not installed"); render(); }));
        actions.addView(action("CONNECT", v -> runDiagnosticConnect()));
        actions.addView(action("IMPORT PRG", v -> selectFile(IMPORT_PRG, "application/octet-stream")));
        actions.addView(action("DISCOVER FRM", v -> discoverFrm()));
        actions.addView(action("EXPORT LOG", v -> exportEvidence()));
        page.addView(actions);
        heading("FRM CAPABILITIES");
        if (frm.capabilities.isEmpty()) info("none", "No verified job metadata available");
        for (FrmCapability capability : frm.capabilities) info(capability.id,
                capability.label + " | " + capability.ediabasJob + " | " + capability.evidence);
        heading("RAW EVENTS (ADVANCED)");
        List<RawEvent> events = repository.events();
        int first = Math.max(0, events.size() - 30);
        for (int i = first; i < events.size(); i++) addMono(formatEvent(events.get(i)));
    }

    private void renderSettings() {
        heading("SETTINGS");
        info("Default", "SIMULATED output. Real diagnostic output is never selected automatically.");
        page.addView(action("USE MOCK TELEMETRY", v -> replaceRepository(new VehicleRepository(new MockTransport()))));
        page.addView(action("REPLAY DEMO", v -> switchToReplayDemo()));
        page.addView(action("LOAD REPLAY", v -> selectFile(OPEN_REPLAY, "*/*")));
        page.addView(action("USE SIMULATED LIGHT SINK", v -> selectSink(simulatedSink)));
        Button real = button("USE FRM LIGHT SINK");
        real.setEnabled(!frm.capabilities.isEmpty()
                && diagnosticTransport.state() == com.synaro.bmwe9xcontrol.diagnostic.DiagnosticState.CONNECTED);
        real.setOnClickListener(v -> selectSink(new FrmLightSink(diagnosticTransport, frm.capabilities,
                new FrmCommandLogger(new File(getFilesDir(), "ediabas/logs/frm-commands.ndjson")))));
        page.addView(real);
        TransportStats stats = repository.stats();
        info("RX / TX / ERR", stats.getRxCount() + " / " + stats.getTxCount() + " / " + stats.getErrorCount());
    }

    private void executeLight(String channel, int value) {
        new Thread(() -> {
            try { activeSink.apply(channel, value); runOnUiThread(() -> toast(channel + " = " + value)); }
            catch (RuntimeException ex) { runOnUiThread(() -> toast("Command rejected: " + ex.getMessage())); }
        }, "frm-manual-command").start();
    }

    private void runDiagnosticConnect() {
        new Thread(() -> {
            DiagnosticResult result = diagnosticTransport.connect();
            runOnUiThread(() -> { toast("Connect: " + result.getStatus() + " — " + result.getDetail()); render(); });
        }, "ediabas-connect").start();
    }

    private void discoverFrm() {
        new Thread(() -> {
            FrmDiscoveryResult result = ediabasSession.discoverFrm();
            frm = result;
            runOnUiThread(() -> { toast("FRM discovery: " + result.status); render(); });
        }, "frm-discovery").start();
    }

    private void selectSink(LightShowEngine.OutputSink sink) {
        showEngine.close(); activeSink = sink; showEngine = new LightShowEngine(sink);
        toast("Light sink: " + modeLabel()); render();
    }

    private void playCurrentShow() {
        try { showEngine.play(editingShow, 1.0); toast("Playing " + editingShow.name + " — " + modeLabel()); }
        catch (RuntimeException ex) { toast("Show rejected: " + ex.getMessage()); }
    }

    private void saveShow() {
        try { toast("Saved: " + showStore.save(editingShow).getAbsolutePath()); }
        catch (Exception ex) { toast("Save failed: " + ex.getMessage()); }
    }

    private void normalizeTimes() {
        long time = 0;
        for (LightShow.Step step : editingShow.timeline) { step.timeMs = time; time += Math.max(100, step.durationMs); }
    }

    private static LightShow.Step copy(LightShow.Step source) {
        LightShow.Step copy = new LightShow.Step();
        copy.timeMs = source.timeMs; copy.durationMs = source.durationMs; copy.channel = source.channel;
        copy.value = source.value; copy.transition = source.transition; copy.fade = source.fade;
        copy.group = source.group; copy.scene = source.scene; copy.pause = source.pause;
        return copy;
    }

    private void switchToReplayDemo() {
        try {
            ReplayTransport replay = new ReplayTransport(new InputStreamReader(
                    getAssets().open("replay_demo.ndjson"), StandardCharsets.UTF_8), 1.0);
            replaceRepository(new VehicleRepository(replay));
        } catch (Exception ex) { toast("Replay failed: " + ex.getMessage()); }
    }

    private void selectFile(int requestCode, String type) {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE); intent.setType(type);
        startActivityForResult(intent, requestCode);
    }

    @Override protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != RESULT_OK || data == null || data.getData() == null) return;
        Uri uri = data.getData();
        try {
            if (requestCode == IMPORT_PRG) {
                File imported = ediabasFiles.importPrg(uri);
                toast("Imported privately: " + imported.getName());
            } else if (requestCode == LOAD_SHOW) {
                try (InputStream input = getContentResolver().openInputStream(uri)) {
                    if (input == null) throw new java.io.IOException("content provider returned no stream");
                    editingShow = com.synaro.bmwe9xcontrol.lightshow.LightShowParser.parse(
                            new InputStreamReader(input, StandardCharsets.UTF_8));
                }
                selectedPage = "SHOWS";
            } else if (requestCode == OPEN_REPLAY) {
                InputStream input = getContentResolver().openInputStream(uri);
                if (input == null) throw new java.io.IOException("content provider returned no stream");
                replaceRepository(new VehicleRepository(new ReplayTransport(
                        new InputStreamReader(input, StandardCharsets.UTF_8), 1.0)));
            }
            render();
        } catch (Exception ex) { toast("Import failed: " + ex.getMessage()); }
    }

    private void exportEvidence() {
        try {
            File file = EvidenceExporter.export(this, profile, repository.transportName(), repository.latest(), repository.events());
            toast("Exported: " + file.getAbsolutePath());
        } catch (Exception ex) { toast("Export failed: " + ex.getMessage()); }
    }

    private void replaceRepository(VehicleRepository next) {
        repository.removeObserver(this); repository.close(); repository = next; repository.addObserver(this);
        try { repository.start(); } catch (Exception ex) { toast("Transport: " + ex.getMessage()); }
        render();
    }

    @Override public void onRepositoryChanged() { runOnUiThread(this::render); }
    @Override public void onFeatures(AudioFeatures features) { latestAudio = features; if (features.beat) runOnUiThread(this::render); }
    @Override public void onError(String detail) { runOnUiThread(() -> toast(detail)); }

    private SeekBar.OnSeekBarChangeListener onStop(java.util.function.IntConsumer consumer) {
        return new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {}
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) { consumer.accept(seekBar.getProgress()); }
        };
    }

    private LinearLayout horizontal() { LinearLayout row = new LinearLayout(this); row.setOrientation(LinearLayout.HORIZONTAL); row.setGravity(Gravity.CENTER_VERTICAL); return row; }
    private Button action(String label, View.OnClickListener listener) { Button result = button(label); result.setOnClickListener(listener); return result; }
    private Button button(String label) { Button result = new Button(this); result.setText(label); result.setAllCaps(false); result.setMinHeight(64); return result; }
    private TextView text(int sp, int color) { TextView result = new TextView(this); result.setTextSize(sp); result.setTextColor(color); result.setPadding(6, 6, 6, 6); return result; }
    private void heading(String value) { TextView view = text(22, Color.rgb(68, 184, 255)); view.setText(value); page.addView(view); }
    private void info(String name, String value) { TextView view = text(16, Color.LTGRAY); view.setText(getString(R.string.info_format, name, value)); page.addView(view); }
    private void addMono(String value) { TextView view = text(13, Color.LTGRAY); view.setTypeface(android.graphics.Typeface.MONOSPACE); view.setText(value); page.addView(view); }
    private EditText input(String hint, String value) { EditText edit = new EditText(this); edit.setHint(hint); edit.setText(value); edit.setTextColor(Color.WHITE); edit.setHintTextColor(Color.GRAY); edit.setMinWidth(150); return edit; }
    private EditText number(String hint, String value) { EditText edit = input(hint, value); edit.setInputType(InputType.TYPE_CLASS_NUMBER); return edit; }
    private String modeLabel() { return activeSink.isSimulationOnly() ? "SIMULATED" : "REAL FRM"; }
    private void toast(String value) { Toast.makeText(this, value, Toast.LENGTH_LONG).show(); }
    private String formatEvent(RawEvent event) {
        return clock.format(new Date(event.getTimestampMs())) + " | " + event.getOrigin() + " | "
                + event.getSource() + " | " + event.getType() + " | " + event.getId() + " | "
                + event.getPayload() + " | " + event.getDecodedValue() + " | " + event.getConfidence();
    }

    @Override protected void onDestroy() {
        if (music != null) music.close();
        showEngine.close();
        // The companion foreground service owns FTDI and the EDIABAS session.
        // Do not tear it down for an Activity recreation (for example a
        // configuration change); only a real user exit releases the adapter.
        if (isFinishing()) diagnosticTransport.close();
        repository.removeObserver(this); repository.close();
        super.onDestroy();
    }
}
