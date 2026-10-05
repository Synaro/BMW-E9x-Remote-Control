using System.Text.Json;
using System.Text.Json.Nodes;
using System.Net;
using System.Net.Sockets;
using System.Text;
using BmwE9x.EdiabasBridge.Core;
using Xunit;

namespace BmwE9x.EdiabasBridge.Tests;

public sealed class BridgeDispatcherTests : IDisposable
{
    private const string Token = "0123456789abcdef0123456789abcdef";
    private readonly string _temp = Path.Combine(Path.GetTempPath(), "ediabas-bridge-tests-" + Guid.NewGuid());
    private readonly MockEngine _engine = new();
    private readonly BridgeDispatcher _dispatcher;

    public BridgeDispatcherTests()
    {
        _dispatcher = new BridgeDispatcher(_engine, _temp, () => Token, "test");
    }

    [Fact]
    public async Task Status_requires_valid_token_and_version()
    {
        RpcResponse unauthorized = await Call("status", token: "wrong");
        Assert.False(unauthorized.Ok);
        Assert.Equal("UNAUTHORIZED", unauthorized.Error?.Code);
        RpcResponse version = await Call("status", version: 99);
        Assert.Equal("PROTOCOL_VERSION_MISMATCH", version.Error?.Code);
        RpcResponse ok = await Call("status");
        Assert.True(ok.Ok);
        BridgeStatus? status = JsonSerializer.Deserialize<BridgeStatus>(
            JsonSerializer.Serialize(ok.Result, BridgeProtocol.JsonOptions), BridgeProtocol.JsonOptions);
        Assert.NotNull(status);
        Assert.Single(status.UsbDevices);
        Assert.Equal(0x0403, status.UsbDevices[0].VendorId);
        Assert.Equal(0x6001, status.UsbDevices[0].ProductId);
        Assert.True(status.UsbDevices[0].PermissionGranted);
    }

    [Fact]
    public async Task Malformed_json_is_rejected()
    {
        RpcResponse response = await _dispatcher.DispatchAsync("{", CancellationToken.None);
        Assert.Equal("MALFORMED_JSON", response.Error?.Code);
    }

    [Fact]
    public async Task Missing_prg_is_rejected()
    {
        RpcResponse response = await Call("identifySgbd", new JsonObject { ["sgbd"] = "FRM_87" });
        Assert.Equal("PRG_NOT_FOUND", response.Error?.Code);
    }

    [Fact]
    public async Task Upload_prg_checks_hash_and_stays_in_private_directory()
    {
        byte[] bytes = [1, 2, 3, 4];
        string hash = Convert.ToHexString(System.Security.Cryptography.SHA256.HashData(bytes)).ToLowerInvariant();
        RpcResponse response = await Call("uploadPrg", new JsonObject
        {
            ["fileName"] = "FRM_87.PRG", ["base64"] = Convert.ToBase64String(bytes), ["sha256"] = hash
        });
        Assert.True(response.Ok);
        Assert.True(File.Exists(Path.Combine(_temp, "FRM_87.PRG")));
    }

    [Fact]
    public async Task Ediabas_error_and_abort_are_returned_without_fake_success()
    {
        _engine.ExecuteError = new InvalidOperationException("IFH-0009");
        RpcResponse failed = await Call("executeJob", new JsonObject
        {
            ["sgbd"] = "FRM_87", ["job"] = "IDENT", ["arguments"] = "", ["results"] = ""
        });
        Assert.Equal("EDIABAS_ERROR", failed.Error?.Code);
        RpcResponse abort = await Call("abortJob");
        Assert.True(abort.Ok);
        Assert.True(_engine.AbortCalled);
    }

    [Fact]
    public async Task Concurrent_command_is_rejected()
    {
        _engine.BlockExecution = true;
        Task<RpcResponse> first = Call("executeJob", new JsonObject
        {
            ["sgbd"] = "FRM_87", ["job"] = "IDENT", ["arguments"] = "", ["results"] = ""
        });
        await _engine.Entered.Task.WaitAsync(TimeSpan.FromSeconds(2), TestContext.Current.CancellationToken);
        RpcResponse second = await Call("listJobs", new JsonObject { ["sgbd"] = "FRM_87" });
        Assert.Equal("COMMAND_BUSY", second.Error?.Code);
        _engine.Release.TrySetResult();
        Assert.True((await first).Ok);
    }

    [Fact]
    public async Task Loopback_server_exchanges_authenticated_ndjson()
    {
        await using var server = new LoopbackRpcServer(_dispatcher, 0);
        server.Start();
        using var client = new TcpClient();
        await client.ConnectAsync(IPAddress.Loopback, server.Port, TestContext.Current.CancellationToken);
        await using NetworkStream stream = client.GetStream();
        await using var writer = new StreamWriter(stream, new UTF8Encoding(false), leaveOpen: true)
            { AutoFlush = true };
        using var reader = new StreamReader(stream, new UTF8Encoding(false), leaveOpen: true);
        string request = JsonSerializer.Serialize(new RpcRequest(81, 1, Token, "status", null),
            BridgeProtocol.JsonOptions);
        await writer.WriteLineAsync(request);
        string? responseJson = await reader.ReadLineAsync(TestContext.Current.CancellationToken);
        Assert.NotNull(responseJson);
        RpcResponse? response = JsonSerializer.Deserialize<RpcResponse>(responseJson, BridgeProtocol.JsonOptions);
        Assert.True(response?.Ok);
        Assert.Equal(81, response?.Id);
    }

    private async Task<RpcResponse> Call(string method, JsonObject? p = null, string token = Token, int version = 1)
    {
        string json = JsonSerializer.Serialize(new RpcRequest(7, version, token, method, p), BridgeProtocol.JsonOptions);
        return await _dispatcher.DispatchAsync(json, CancellationToken.None);
    }

    public void Dispose()
    {
        if (Directory.Exists(_temp)) Directory.Delete(_temp, true);
    }

    private sealed class MockEngine : IEdiabasEngine
    {
        public string EdiabasVersion => "7.6.0";
        public string EcuPath { get; private set; } = "";
        public string? ActiveSgbd => "FRM_87.PRG";
        public bool IsConfigured => true;
        public bool IsJobRunning => false;
        public Exception? ExecuteError { get; set; }
        public bool AbortCalled { get; private set; }
        public bool BlockExecution { get; set; }
        public TaskCompletionSource Entered { get; } = new(TaskCreationOptions.RunContinuationsAsynchronously);
        public TaskCompletionSource Release { get; } = new(TaskCreationOptions.RunContinuationsAsynchronously);
        public Task<IReadOnlyList<UsbDeviceInfo>> ListUsbDevicesAsync(CancellationToken c) =>
            Task.FromResult<IReadOnlyList<UsbDeviceInfo>>([new(0x0403, 0x6001, "FTDI", null, true)]);
        public Task SetEcuPathAsync(string path, CancellationToken c) { EcuPath = path; return Task.CompletedTask; }
        public Task ConnectAsync(CancellationToken c) => Task.CompletedTask;
        public Task DisconnectAsync(CancellationToken c) => Task.CompletedTask;
        public Task<string> IdentifySgbdAsync(string sgbd, CancellationToken c) =>
            File.Exists(Path.Combine(EcuPath, sgbd + ".PRG")) ? Task.FromResult(sgbd) :
                Task.FromException<string>(new FileNotFoundException(sgbd));
        public Task<IReadOnlyList<JobDefinition>> ListJobsAsync(string s, CancellationToken c) =>
            Task.FromResult<IReadOnlyList<JobDefinition>>([new("IDENT", s, ["read only"], [], [])]);
        public async Task<JobExecutionResult> ExecuteJobAsync(string s, string j, string a, string r, CancellationToken c)
        {
            Entered.TrySetResult();
            if (BlockExecution) await Release.Task.WaitAsync(c);
            if (ExecuteError is not null) throw ExecuteError;
            return new(1, 1, [], "[]", null);
        }
        public Task AbortJobAsync(CancellationToken c) { AbortCalled = true; return Task.CompletedTask; }
        public Task<string> GetTraceAsync(CancellationToken c) => Task.FromResult("");
        public ValueTask DisposeAsync() => ValueTask.CompletedTask;
    }
}
