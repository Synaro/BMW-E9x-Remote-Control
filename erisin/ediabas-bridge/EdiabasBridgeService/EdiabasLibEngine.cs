using System.Diagnostics;
using System.Globalization;
using System.Text;
using Android.App;
using Android.Content;
using Android.Hardware.Usb;
using BmwE9x.EdiabasBridge.Core;
using EdiabasLib;
using Hoho.Android.UsbSerial.Driver;

namespace BmwE9x.EdiabasBridge.Android;

internal sealed class EdiabasLibEngine : IEdiabasEngine
{
    private const int FtdiVendorId = 0x0403;
    private const string UsbPermissionAction = "com.synaro.bmwe9xediabasbridge.USB_PERMISSION";
    private readonly Service _service;
    private readonly UsbManager _usbManager;
    private readonly string _traceDirectory;
    private readonly object _sync = new();
    private EdiabasNet? _ediabas;
    private volatile bool _abortRequested;
    private string _ecuPath = string.Empty;
    private string _trace = string.Empty;

    public EdiabasLibEngine(Service service)
    {
        _service = service;
        _usbManager = (UsbManager)service.GetSystemService(Context.UsbService)!;
        _traceDirectory = Path.Combine(service.FilesDir!.AbsolutePath, "ediabas", "trace");
    }

    public string EdiabasVersion => EdiabasNet.EdiabasVersionString;
    public string EcuPath => _ecuPath;
    public string? ActiveSgbd => _ediabas?.SgbdFileName;
    public bool IsConfigured => _ediabas is not null;
    public bool IsJobRunning => _ediabas?.JobRunning ?? false;

    public Task<IReadOnlyList<UsbDeviceInfo>> ListUsbDevicesAsync(CancellationToken cancellationToken)
    {
        var devices = (_usbManager.DeviceList?.Values ?? [])
            .Where(device => device.VendorId == FtdiVendorId)
            .Select(device => new UsbDeviceInfo(device.VendorId, device.ProductId, device.DeviceName,
                SafeSerial(device), _usbManager.HasPermission(device)))
            .ToArray();
        return Task.FromResult<IReadOnlyList<UsbDeviceInfo>>(devices);
    }

    public Task SetEcuPathAsync(string ecuPath, CancellationToken cancellationToken)
    {
        _ecuPath = Path.GetFullPath(ecuPath);
        Directory.CreateDirectory(_ecuPath);
        _ediabas?.SetConfigProperty("EcuPath", _ecuPath);
        AppendTrace("ECU_PATH", _ecuPath);
        return Task.CompletedTask;
    }

    public Task ConnectAsync(CancellationToken cancellationToken)
    {
        UsbDevice? device = _usbManager.DeviceList?.Values.FirstOrDefault(x => x.VendorId == FtdiVendorId);
        if (device is null) throw new InvalidOperationException("no FTDI device with VID 0x0403 detected");
        if (!_usbManager.HasPermission(device))
        {
            PendingIntentFlags flags = OperatingSystem.IsAndroidVersionAtLeast(31)
                ? PendingIntentFlags.Mutable : PendingIntentFlags.OneShot;
            var permissionIntent = PendingIntent.GetBroadcast(_service, 0,
                new Intent(UsbPermissionAction).SetPackage(_service.PackageName), flags);
            _usbManager.RequestPermission(device, permissionIntent);
            throw new UnauthorizedAccessException("USB permission requested; retry connect after approval");
        }
        if (string.IsNullOrWhiteSpace(_ecuPath)) throw new InvalidOperationException("ECU path is not configured");

        lock (_sync)
        {
            _ediabas?.Dispose();
            var ediabas = new EdiabasNet();
            ediabas.SetConfigProperty("EcuPath", _ecuPath);
            Directory.CreateDirectory(_traceDirectory);
            ediabas.SetConfigProperty("TracePath", _traceDirectory);
            ediabas.SetConfigProperty("IfhTrace", ((int)EdiabasNet.EdLogLevel.Error).ToString(CultureInfo.InvariantCulture));
            ediabas.SetConfigProperty("AppendTrace", "0");
            ediabas.SetConfigProperty("IfhTraceBuffering", "0");
            ediabas.AbortJobFunc = () => _abortRequested;
            var obd = new EdInterfaceObd { ComPort = EdFtdiInterface.PortId + "0" };
            ediabas.EdInterfaceClass = obd;
            ediabas.EdInterfaceClass.ConnectParameter = new EdFtdiInterface.ConnectParameterType(_usbManager);
            _ediabas = ediabas;
            AppendTrace("CONNECT", "STD:OBD configured; FTDI0 selected");
        }
        return Task.CompletedTask;
    }

    public Task DisconnectAsync(CancellationToken cancellationToken)
    {
        lock (_sync)
        {
            _abortRequested = true;
            _ediabas?.Dispose();
            _ediabas = null;
            AppendTrace("DISCONNECT", "EDIABAS disposed and FTDI released");
        }
        return Task.CompletedTask;
    }

    public Task<string> IdentifySgbdAsync(string sgbd, CancellationToken cancellationToken) =>
        Task.Run(() =>
        {
            string resolved = ResolveSgbd(sgbd);
            AppendTrace("IDENTIFY_SGBD", resolved);
            return resolved;
        }, cancellationToken);

    public Task<IReadOnlyList<JobDefinition>> ListJobsAsync(string sgbd, CancellationToken cancellationToken) =>
        Task.Run<IReadOnlyList<JobDefinition>>(() =>
        {
            var jobs = ReadJobMetadata(sgbd);
            AppendTrace("LIST_JOBS", $"{sgbd}: {jobs.Count}");
            return jobs;
        }, cancellationToken);

    public Task<JobExecutionResult> ExecuteJobAsync(string sgbd, string job, string arguments, string results,
        CancellationToken cancellationToken) => Task.Run(() =>
    {
        EdiabasNet ediabas = RequireEdiabas();
        ResolveSgbd(sgbd);
        _abortRequested = false;
        using CancellationTokenRegistration registration = cancellationToken.Register(() => _abortRequested = true);
        ediabas.ArgString = arguments ?? string.Empty;
        ediabas.ArgBinaryStd = null;
        ediabas.ResultsRequests = results ?? string.Empty;
        var sw = Stopwatch.StartNew();
        ediabas.ExecuteJob(job);
        sw.Stop();
        var sets = ConvertSets(ediabas.ResultSets);
        string? error = ediabas.ErrorCodeLast == EdiabasNet.ErrorCodes.EDIABAS_ERR_NONE
            ? null : ediabas.ErrorCodeLast.ToString();
        AppendTrace("EXECUTE_JOB", $"{sgbd}/{job} {sw.ElapsedMilliseconds}ms error={error ?? "none"}");
        return new JobExecutionResult(sw.ElapsedMilliseconds, sw.ElapsedMilliseconds, sets,
            System.Text.Json.JsonSerializer.Serialize(sets), error);
    }, cancellationToken);

    public Task AbortJobAsync(CancellationToken cancellationToken)
    {
        _abortRequested = true;
        _ediabas?.EdInterfaceClass?.TransmitCancel(true);
        AppendTrace("ABORT_JOB", "abort requested");
        return Task.CompletedTask;
    }

    public Task<string> GetTraceAsync(CancellationToken cancellationToken)
    {
        string operationTrace;
        lock (_sync) operationTrace = _trace;
        string ifhPath = Path.Combine(_traceDirectory, "ifh.trc");
        if (!File.Exists(ifhPath)) return Task.FromResult(operationTrace);
        try
        {
            using var stream = new FileStream(ifhPath, FileMode.Open, FileAccess.Read, FileShare.ReadWrite);
            if (stream.Length > 65536) stream.Seek(-65536, SeekOrigin.End);
            using var reader = new StreamReader(stream, Encoding.UTF8, true, 4096, false);
            string ifhTrace = reader.ReadToEnd();
            return Task.FromResult(operationTrace + "--- EDIABAS IFH TRACE (tail) ---" +
                                   Environment.NewLine + ifhTrace);
        }
        catch (IOException ex)
        {
            return Task.FromResult(operationTrace + "TRACE_READ_ERROR " + ex.Message + Environment.NewLine);
        }
    }

    public async ValueTask DisposeAsync() => await DisconnectAsync(CancellationToken.None);

    private string ResolveSgbd(string requested)
    {
        EdiabasNet ediabas = RequireEdiabas();
        string name = Path.GetFileName(requested);
        if (string.IsNullOrWhiteSpace(Path.GetExtension(name))) name += EdiabasNet.PrgFileExt;
        string path = Path.Combine(_ecuPath, name);
        if (!File.Exists(path)) throw new FileNotFoundException("SGBD is not present in bridge storage", name);
        ediabas.ResolveSgbdFile(path);
        return Path.GetFileNameWithoutExtension(ediabas.SgbdFileName);
    }

    private IReadOnlyList<JobDefinition> ReadJobMetadata(string sgbd)
    {
        EdiabasNet ediabas = RequireEdiabas();
        ResolveSgbd(sgbd);
        var jobs = new List<MutableJob>();
        ediabas.NoInitForVJobs = true;
        ediabas.ArgString = "ALL";
        ediabas.ArgBinaryStd = null;
        ediabas.ResultsRequests = string.Empty;
        ediabas.ExecuteJob("_JOBS");
        string objectName = GetString(ediabas.ResultSets?.FirstOrDefault(), "OBJECT");
        foreach (var set in ediabas.ResultSets?.Skip(1) ?? [])
        {
            string name = GetString(set, "JOBNAME");
            if (!string.IsNullOrWhiteSpace(name)) jobs.Add(new MutableJob(name, objectName));
        }
        foreach (MutableJob job in jobs)
        {
            job.Comments.AddRange(ReadIndexedComments(ediabas, "_JOBCOMMENTS", job.Name, "JOBCOMMENT"));
            ReadArguments(ediabas, job);
            ReadResults(ediabas, job);
        }
        return jobs.Select(x => x.ToDefinition()).ToArray();
    }

    private static IEnumerable<string> ReadIndexedComments(EdiabasNet ediabas, string metaJob, string argument,
        string prefix)
    {
        ediabas.ArgString = argument;
        ediabas.ArgBinaryStd = null;
        ediabas.ResultsRequests = string.Empty;
        ediabas.NoInitForVJobs = true;
        ediabas.ExecuteJob(metaJob);
        var set = ediabas.ResultSets?.Skip(1).FirstOrDefault();
        if (set is null) yield break;
        for (int i = 0; ; i++)
        {
            string value = GetString(set, prefix + i.ToString(CultureInfo.InvariantCulture));
            if (string.IsNullOrEmpty(value)) yield break;
            yield return value;
        }
    }

    private static void ReadArguments(EdiabasNet ediabas, MutableJob job)
    {
        RunMeta(ediabas, "_ARGUMENTS", job.Name);
        foreach (var set in ediabas.ResultSets?.Skip(1) ?? [])
            job.Arguments.Add(new JobArgument(GetString(set, "ARG"), GetString(set, "ARGTYPE"),
                Indexed(set, "ARGCOMMENT")));
    }

    private static void ReadResults(EdiabasNet ediabas, MutableJob job)
    {
        RunMeta(ediabas, "_RESULTS", job.Name);
        foreach (var set in ediabas.ResultSets?.Skip(1) ?? [])
            job.Results.Add(new JobResultDefinition(GetString(set, "RESULT"), GetString(set, "RESULTTYPE"),
                Indexed(set, "RESULTCOMMENT")));
    }

    private static void RunMeta(EdiabasNet ediabas, string metaJob, string argument)
    {
        ediabas.ArgString = argument;
        ediabas.ArgBinaryStd = null;
        ediabas.ResultsRequests = string.Empty;
        ediabas.NoInitForVJobs = true;
        ediabas.ExecuteJob(metaJob);
    }

    private static IReadOnlyList<string> Indexed(Dictionary<string, EdiabasNet.ResultData> set, string prefix)
    {
        var values = new List<string>();
        for (int i = 0; ; i++)
        {
            string value = GetString(set, prefix + i.ToString(CultureInfo.InvariantCulture));
            if (string.IsNullOrEmpty(value)) return values;
            values.Add(value);
        }
    }

    private static string GetString(Dictionary<string, EdiabasNet.ResultData>? set, string key) =>
        set is not null && set.TryGetValue(key, out var value) ? Convert.ToString(value.OpData, CultureInfo.InvariantCulture) ?? string.Empty : string.Empty;

    private static IReadOnlyList<IReadOnlyDictionary<string, object?>> ConvertSets(
        List<Dictionary<string, EdiabasNet.ResultData>>? sets) => (sets ?? [])
        .Select(set => (IReadOnlyDictionary<string, object?>)set.ToDictionary(x => x.Key, x => Normalize(x.Value.OpData)))
        .ToArray();

    private static object? Normalize(object? value) => value switch
    {
        null => null,
        byte[] bytes => Convert.ToHexString(bytes),
        IFormattable valueFormattable => valueFormattable.ToString(null, CultureInfo.InvariantCulture),
        _ => value.ToString()
    };

    private EdiabasNet RequireEdiabas() => _ediabas ?? throw new InvalidOperationException("EDIABAS is not connected");

    private void AppendTrace(string category, string detail)
    {
        string line = $"{DateTimeOffset.UtcNow:O} {category} {detail}{Environment.NewLine}";
        lock (_sync)
        {
            _trace += line;
            if (_trace.Length > 65536) _trace = _trace[^65536..];
        }
    }

    private string? SafeSerial(UsbDevice device)
    {
        if (!_usbManager.HasPermission(device)) return null;
        try { return device.SerialNumber; } catch { return null; }
    }

    private sealed class MutableJob(string name, string objectName)
    {
        public string Name { get; } = name;
        public string ObjectName { get; } = objectName;
        public List<string> Comments { get; } = [];
        public List<JobArgument> Arguments { get; } = [];
        public List<JobResultDefinition> Results { get; } = [];
        public JobDefinition ToDefinition() => new(Name, ObjectName, Comments, Arguments, Results);
    }
}
