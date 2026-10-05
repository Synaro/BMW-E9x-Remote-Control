using System.Diagnostics;
using System.Security.Cryptography;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace BmwE9x.EdiabasBridge.Core;

public sealed class BridgeDispatcher
{
    private readonly IEdiabasEngine _engine;
    private readonly SemaphoreSlim _commandGate = new(1, 1);
    private readonly string _privateEcuDirectory;
    private readonly Func<string> _tokenProvider;
    private readonly string _bridgeVersion;

    public BridgeDispatcher(IEdiabasEngine engine, string privateEcuDirectory, Func<string> tokenProvider,
        string bridgeVersion)
    {
        _engine = engine;
        _privateEcuDirectory = Path.GetFullPath(privateEcuDirectory);
        _tokenProvider = tokenProvider;
        _bridgeVersion = bridgeVersion;
        Directory.CreateDirectory(_privateEcuDirectory);
    }

    public async Task<RpcResponse> DispatchAsync(string json, CancellationToken cancellationToken)
    {
        RpcRequest? request;
        try
        {
            request = JsonSerializer.Deserialize<RpcRequest>(json, BridgeProtocol.JsonOptions);
        }
        catch (JsonException ex)
        {
            return RpcResponse.Failure(0, "MALFORMED_JSON", ex.Message);
        }

        if (request is null || string.IsNullOrWhiteSpace(request.Method))
            return RpcResponse.Failure(request?.Id ?? 0, "INVALID_REQUEST", "method is required");
        if (request.Version != BridgeProtocol.Version)
            return RpcResponse.Failure(request.Id, "PROTOCOL_VERSION_MISMATCH",
                $"expected {BridgeProtocol.Version}, received {request.Version}");
        if (!CryptographicOperations.FixedTimeEquals(
                System.Text.Encoding.UTF8.GetBytes(request.Token ?? string.Empty),
                System.Text.Encoding.UTF8.GetBytes(_tokenProvider())))
            return RpcResponse.Failure(request.Id, "UNAUTHORIZED", "invalid session token");

        if (string.Equals(request.Method, "abortJob", StringComparison.Ordinal))
        {
            await _engine.AbortJobAsync(cancellationToken).ConfigureAwait(false);
            return RpcResponse.Success(request.Id, new { aborted = true });
        }

        if (!await _commandGate.WaitAsync(0, cancellationToken).ConfigureAwait(false))
            return RpcResponse.Failure(request.Id, "COMMAND_BUSY", "another EDIABAS command is running");

        try
        {
            return await DispatchExclusiveAsync(request, cancellationToken).ConfigureAwait(false);
        }
        catch (FileNotFoundException ex)
        {
            return RpcResponse.Failure(request.Id, "PRG_NOT_FOUND", ex.Message);
        }
        catch (OperationCanceledException)
        {
            return RpcResponse.Failure(request.Id, "ABORTED", "operation aborted");
        }
        catch (Exception ex)
        {
            return RpcResponse.Failure(request.Id, "EDIABAS_ERROR", ex.Message);
        }
        finally
        {
            _commandGate.Release();
        }
    }

    private async Task<RpcResponse> DispatchExclusiveAsync(RpcRequest request, CancellationToken cancellationToken)
    {
        var p = request.Params ?? new JsonObject();
        switch (request.Method)
        {
            case "status":
            {
                var usb = await _engine.ListUsbDevicesAsync(cancellationToken).ConfigureAwait(false);
                return RpcResponse.Success(request.Id, new BridgeStatus(
                    BridgeProtocol.Version, _bridgeVersion, _engine.EdiabasVersion,
                    usb,
                    usb.Count > 0, usb.Any(x => x.PermissionGranted), _engine.IsConfigured,
                    _engine.IsJobRunning, _engine.EcuPath, _engine.ActiveSgbd,
                    _engine.IsConfigured ? "EDIABAS_CONFIGURED" : usb.Count > 0 ? "FTDI_DETECTED" : "IDLE"));
            }
            case "listUsbDevices":
                return RpcResponse.Success(request.Id,
                    await _engine.ListUsbDevicesAsync(cancellationToken).ConfigureAwait(false));
            case "connect":
                await _engine.ConnectAsync(cancellationToken).ConfigureAwait(false);
                return RpcResponse.Success(request.Id, new { connected = true, interfaceName = "STD:OBD" });
            case "disconnect":
                await _engine.DisconnectAsync(cancellationToken).ConfigureAwait(false);
                return RpcResponse.Success(request.Id, new { connected = false });
            case "setEcuPath":
                await _engine.SetEcuPathAsync(_privateEcuDirectory, cancellationToken).ConfigureAwait(false);
                return RpcResponse.Success(request.Id, new { ecuPath = _privateEcuDirectory });
            case "uploadPrg":
                return RpcResponse.Success(request.Id, UploadPrg(p));
            case "identifySgbd":
                return RpcResponse.Success(request.Id, new
                {
                    sgbd = await _engine.IdentifySgbdAsync(Required(p, "sgbd"), cancellationToken).ConfigureAwait(false)
                });
            case "probeSgbd":
            {
                string sgbd = Required(p, "sgbd");
                IReadOnlyList<JobDefinition> jobs = await _engine.ListJobsAsync(sgbd, cancellationToken).ConfigureAwait(false);
                JobDefinition? ident = jobs.FirstOrDefault(x => string.Equals(x.Name, "IDENT", StringComparison.OrdinalIgnoreCase));
                if (ident is null)
                    return RpcResponse.Failure(request.Id, "NO_READ_ONLY_PROBE_JOB",
                        "SGBD loaded but no IDENT job is documented; choose a read-only job explicitly");
                return RpcResponse.Success(request.Id,
                    await _engine.ExecuteJobAsync(sgbd, ident.Name, string.Empty, string.Empty, cancellationToken)
                        .ConfigureAwait(false));
            }
            case "listJobs":
                return RpcResponse.Success(request.Id,
                    await _engine.ListJobsAsync(Required(p, "sgbd"), cancellationToken).ConfigureAwait(false));
            case "executeJob":
            {
                var sw = Stopwatch.StartNew();
                JobExecutionResult result = await _engine.ExecuteJobAsync(
                    Required(p, "sgbd"), Required(p, "job"), Optional(p, "arguments"),
                    Optional(p, "results"), cancellationToken).ConfigureAwait(false);
                sw.Stop();
                return RpcResponse.Success(request.Id, new
                {
                    elapsedMs = sw.ElapsedMilliseconds,
                    result.JobExecutionMs,
                    ipcDispatchMs = Math.Max(0, sw.ElapsedMilliseconds - result.JobExecutionMs),
                    result.Sets,
                    result.Raw,
                    result.EdiabasError
                });
            }
            case "getTrace":
                return RpcResponse.Success(request.Id,
                    new { trace = await _engine.GetTraceAsync(cancellationToken).ConfigureAwait(false) });
            default:
                return RpcResponse.Failure(request.Id, "METHOD_NOT_FOUND", request.Method);
        }
    }

    private object UploadPrg(JsonObject p)
    {
        string fileName = Path.GetFileName(Required(p, "fileName"));
        if (!string.Equals(Path.GetExtension(fileName), ".prg", StringComparison.OrdinalIgnoreCase))
            throw new InvalidDataException("only .PRG files are accepted");
        byte[] data = Convert.FromBase64String(Required(p, "base64"));
        string expected = Required(p, "sha256").ToLowerInvariant();
        string actual = Convert.ToHexString(SHA256.HashData(data)).ToLowerInvariant();
        if (!CryptographicOperations.FixedTimeEquals(
                System.Text.Encoding.ASCII.GetBytes(expected), System.Text.Encoding.ASCII.GetBytes(actual)))
            throw new InvalidDataException("PRG SHA-256 mismatch");
        string target = Path.GetFullPath(Path.Combine(_privateEcuDirectory, fileName));
        if (!target.StartsWith(_privateEcuDirectory + Path.DirectorySeparatorChar, StringComparison.Ordinal))
            throw new InvalidDataException("invalid PRG path");
        File.WriteAllBytes(target, data);
        return new { fileName, sha256 = actual, size = data.LongLength };
    }

    private static string Required(JsonObject p, string name) =>
        p[name]?.GetValue<string>() is { Length: > 0 } value ? value :
            throw new InvalidDataException($"{name} is required");

    private static string Optional(JsonObject p, string name) => p[name]?.GetValue<string>() ?? string.Empty;
}
