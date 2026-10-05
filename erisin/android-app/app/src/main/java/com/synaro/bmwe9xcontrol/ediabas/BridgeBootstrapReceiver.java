package com.synaro.bmwe9xcontrol.ediabas;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class BridgeBootstrapReceiver extends BroadcastReceiver {
    @Override public void onReceive(Context context, Intent intent) {
        String token = intent.getStringExtra(EdiabasBridgeLauncher.EXTRA_TOKEN);
        int port = intent.getIntExtra(EdiabasBridgeLauncher.EXTRA_PORT, -1);
        int version = intent.getIntExtra(EdiabasBridgeLauncher.EXTRA_PROTOCOL_VERSION, -1);
        if (token == null || port < 1) return;
        try { BridgeEndpointRegistry.set(new BridgeEndpoint("127.0.0.1", port, token, version)); }
        catch (IllegalArgumentException ignored) { BridgeEndpointRegistry.clear(); }
    }
}
