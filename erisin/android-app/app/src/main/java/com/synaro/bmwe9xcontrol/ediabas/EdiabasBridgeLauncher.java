package com.synaro.bmwe9xcontrol.ediabas;

import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;

public final class EdiabasBridgeLauncher {
    public static final String PACKAGE = "com.synaro.bmwe9xediabasbridge";
    public static final String SERVICE = PACKAGE + ".EdiabasBridgeService";
    public static final String ACTION_START = PACKAGE + ".START";
    public static final String ACTION_STOP = PACKAGE + ".STOP";
    public static final String EXTRA_CALLBACK = "callback";
    public static final String EXTRA_TOKEN = "token";
    public static final String EXTRA_PORT = "port";
    public static final String EXTRA_PROTOCOL_VERSION = "protocolVersion";
    private final Context context;

    public EdiabasBridgeLauncher(Context context) { this.context = context.getApplicationContext(); }

    public boolean isInstalled() {
        try { context.getPackageManager().getPackageInfo(PACKAGE, 0); return true; }
        catch (PackageManager.NameNotFoundException ex) { return false; }
    }

    public boolean start() {
        if (!isInstalled()) return false;
        Intent callbackIntent = new Intent(context, BridgeBootstrapReceiver.class);
        int callbackFlags = PendingIntent.FLAG_UPDATE_CURRENT |
                (Build.VERSION.SDK_INT >= 31 ? PendingIntent.FLAG_MUTABLE : 0);
        PendingIntent callback = PendingIntent.getBroadcast(context, 4210, callbackIntent, callbackFlags);
        Intent service = new Intent(ACTION_START)
                .setComponent(new ComponentName(PACKAGE, SERVICE))
                .putExtra(EXTRA_CALLBACK, callback);
        context.startForegroundService(service);
        return true;
    }

    public void stop() {
        if (!isInstalled()) return;
        BridgeEndpoint endpoint = BridgeEndpointRegistry.get();
        Intent stopIntent = new Intent(ACTION_STOP).setComponent(new ComponentName(PACKAGE, SERVICE));
        if (endpoint != null) stopIntent.putExtra(EXTRA_TOKEN, endpoint.token());
        context.startService(stopIntent);
        BridgeEndpointRegistry.clear();
    }
}
