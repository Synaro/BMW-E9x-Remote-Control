using System.Text.Json;
using System.Text.Json.Nodes;
using System.Text.Json.Serialization;

namespace BmwE9x.EdiabasBridge.Core;

public static class BridgeProtocol
{
    public const int Version = 1;
    public const int DefaultPort = 39721;
    public const int MaxLineBytes = 32 * 1024 * 1024;

    public static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web)
    {
        DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull,
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase
    };
}

public sealed record RpcRequest(long Id, int Version, string Token, string Method, JsonObject? Params);

public sealed record RpcError(string Code, string Message, JsonObject? Data = null);

public sealed record RpcResponse(long Id, bool Ok, object? Result = null, RpcError? Error = null)
{
    public static RpcResponse Success(long id, object? result) => new(id, true, result, null);
    public static RpcResponse Failure(long id, string code, string message, JsonObject? data = null) =>
        new(id, false, null, new RpcError(code, message, data));
}

public sealed record UsbDeviceInfo(int VendorId, int ProductId, string DeviceName, string? SerialNumber, bool PermissionGranted);

public sealed record JobArgument(string Name, string Type, IReadOnlyList<string> Comments);

public sealed record JobResultDefinition(string Name, string Type, IReadOnlyList<string> Comments);

public sealed record JobDefinition(
    string Name,
    string ObjectName,
    IReadOnlyList<string> Comments,
    IReadOnlyList<JobArgument> Arguments,
    IReadOnlyList<JobResultDefinition> Results);

public sealed record JobExecutionResult(
    long ElapsedMs,
    long JobExecutionMs,
    IReadOnlyList<IReadOnlyDictionary<string, object?>> Sets,
    string Raw,
    string? EdiabasError);

public sealed record BridgeStatus(
    int Version,
    string BridgeVersion,
    string EdiabasVersion,
    IReadOnlyList<UsbDeviceInfo> UsbDevices,
    bool UsbDetected,
    bool UsbPermissionGranted,
    bool EdiabasConfigured,
    bool JobRunning,
    string EcuPath,
    string? ActiveSgbd,
    string State);
