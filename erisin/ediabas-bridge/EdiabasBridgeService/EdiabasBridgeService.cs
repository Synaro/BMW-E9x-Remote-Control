using System.Security.Cryptography;
using Android.App;
using Android.Content;
using Android.OS;
using BmwE9x.EdiabasBridge.Core;

namespace BmwE9x.EdiabasBridge.Android;

[Service(Name = "com.synaro.bmwe9xediabasbridge.EdiabasBridgeService", Exported = true)]
public sealed class EdiabasBridgeService : Service
{
    public const string ActionStart = "com.synaro.bmwe9xediabasbridge.START";
    public const string ActionStop = "com.synaro.bmwe9xediabasbridge.STOP";
    public const string ExtraCallback = "callback";
    public const string ExtraToken = "token";
    public const string ExtraPort = "port";
    public const string ExtraProtocolVersion = "protocolVersion";
    private const int NotificationId = 4210;
    private const string ChannelId = "ediabas_bridge";

    private string _token = string.Empty;
    private EdiabasLibEngine? _engine;
    private LoopbackRpcServer? _server;
    private UsbDetachReceiver? _usbReceiver;

    public override void OnCreate()
    {
        base.OnCreate();
        _usbReceiver = new UsbDetachReceiver(() => _ = _engine?.DisconnectAsync(CancellationToken.None));
        var filter = new IntentFilter(global::Android.Hardware.Usb.UsbManager.ActionUsbDeviceDetached);
#pragma warning disable CA1422
        RegisterReceiver(_usbReceiver, filter);
#pragma warning restore CA1422
    }

    public override IBinder? OnBind(Intent? intent) => null;

    public override StartCommandResult OnStartCommand(Intent? intent, StartCommandFlags flags, int startId)
    {
        if (intent?.Action == ActionStop)
        {
            string? suppliedToken = intent.GetStringExtra(ExtraToken);
            if (HasValidToken(suppliedToken)) _ = StopAsync();
            return StartCommandResult.NotSticky;
        }

        EnsureForeground();
        if (_server is null)
        {
            _token = Convert.ToHexString(RandomNumberGenerator.GetBytes(32)).ToLowerInvariant();
            _engine = new EdiabasLibEngine(this);
            string ecuDir = Path.Combine(FilesDir!.AbsolutePath, "ediabas", "ecu");
            var dispatcher = new BridgeDispatcher(_engine, ecuDir, () => _token, "1.0.0");
            _server = new LoopbackRpcServer(dispatcher, BridgeProtocol.DefaultPort);
            _server.Start();
        }
        SendBootstrap(intent);
        return StartCommandResult.Sticky;
    }

    public override void OnDestroy()
    {
        if (_usbReceiver is not null)
        {
            try { UnregisterReceiver(_usbReceiver); } catch (ArgumentException) { }
            _usbReceiver = null;
        }
        StopAsync().GetAwaiter().GetResult();
        base.OnDestroy();
    }

    private void SendBootstrap(Intent? intent)
    {
        PendingIntent? callback = OperatingSystem.IsAndroidVersionAtLeast(33)
            ? intent?.GetParcelableExtra(ExtraCallback, Java.Lang.Class.FromType(typeof(PendingIntent))) as PendingIntent
#pragma warning disable CS0618
            : intent?.GetParcelableExtra(ExtraCallback) as PendingIntent;
#pragma warning restore CS0618
        if (callback is null || !IsAllowedClient(callback.CreatorPackage)) return;
        var result = new Intent()
            .PutExtra(ExtraToken, _token)
            .PutExtra(ExtraPort, _server?.Port ?? BridgeProtocol.DefaultPort)
            .PutExtra(ExtraProtocolVersion, BridgeProtocol.Version);
        try { callback.Send(this, Result.Ok, result); } catch (PendingIntent.CanceledException) { }
    }

    private static bool IsAllowedClient(string? packageName) =>
        string.Equals(packageName, "com.synaro.bmwe9xcontrol", StringComparison.Ordinal) ||
        string.Equals(packageName, "com.synaro.bmwe9xcontrol.debug", StringComparison.Ordinal);

    private bool HasValidToken(string? suppliedToken)
    {
        if (string.IsNullOrEmpty(_token) || string.IsNullOrEmpty(suppliedToken)) return false;
        byte[] expected = System.Text.Encoding.UTF8.GetBytes(_token);
        byte[] supplied = System.Text.Encoding.UTF8.GetBytes(suppliedToken);
        return expected.Length == supplied.Length &&
               CryptographicOperations.FixedTimeEquals(expected, supplied);
    }

    private void EnsureForeground()
    {
        var manager = (NotificationManager)GetSystemService(NotificationService)!;
        manager.CreateNotificationChannel(new NotificationChannel(ChannelId, "BMW E9x EDIABAS",
            NotificationImportance.Low));
        var notification = new Notification.Builder(this, ChannelId)
            .SetContentTitle("BMW E9x EDIABAS")
            .SetContentText("K+DCAN bridge ready")
            .SetSmallIcon(global::Android.Resource.Drawable.IcDialogInfo)
            .SetOngoing(true)
            .Build();
        StartForeground(NotificationId, notification);
    }

    private async Task StopAsync()
    {
        if (_server is not null) { await _server.DisposeAsync(); _server = null; }
        if (_engine is not null) { await _engine.DisposeAsync(); _engine = null; }
        StopForeground(StopForegroundFlags.Remove);
        StopSelf();
    }

    private sealed class UsbDetachReceiver(Action onDetach) : BroadcastReceiver
    {
        public override void OnReceive(Context? context, Intent? intent)
        {
            if (intent?.Action == global::Android.Hardware.Usb.UsbManager.ActionUsbDeviceDetached) onDetach();
        }
    }
}
