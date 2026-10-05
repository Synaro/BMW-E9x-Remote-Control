using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;

namespace BmwE9x.EdiabasBridge.Core;

public sealed class LoopbackRpcServer : IAsyncDisposable
{
    private readonly BridgeDispatcher _dispatcher;
    private readonly TcpListener _listener;
    private readonly CancellationTokenSource _stop = new();
    private Task? _acceptTask;

    public LoopbackRpcServer(BridgeDispatcher dispatcher, int port = BridgeProtocol.DefaultPort)
    {
        _dispatcher = dispatcher;
        _listener = new TcpListener(IPAddress.Loopback, port);
    }

    public int Port => ((IPEndPoint)_listener.LocalEndpoint).Port;

    public void Start()
    {
        _listener.Start();
        _acceptTask = AcceptLoopAsync(_stop.Token);
    }

    private async Task AcceptLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            TcpClient client;
            try { client = await _listener.AcceptTcpClientAsync(cancellationToken).ConfigureAwait(false); }
            catch (OperationCanceledException) { break; }
            _ = HandleClientAsync(client, cancellationToken);
        }
    }

    private async Task HandleClientAsync(TcpClient client, CancellationToken cancellationToken)
    {
        await using NetworkStream stream = client.GetStream();
        using (client)
        using (var reader = new StreamReader(stream, new UTF8Encoding(false), false, 4096, true))
        await using (var writer = new StreamWriter(stream, new UTF8Encoding(false), 4096, true) { AutoFlush = true })
        {
            string? line;
            while ((line = await reader.ReadLineAsync(cancellationToken).ConfigureAwait(false)) is not null)
            {
                if (Encoding.UTF8.GetByteCount(line) > BridgeProtocol.MaxLineBytes)
                {
                    await writer.WriteLineAsync(JsonSerializer.Serialize(
                        RpcResponse.Failure(0, "REQUEST_TOO_LARGE", "NDJSON line exceeds limit"),
                        BridgeProtocol.JsonOptions)).ConfigureAwait(false);
                    break;
                }
                RpcResponse response = await _dispatcher.DispatchAsync(line, cancellationToken).ConfigureAwait(false);
                await writer.WriteLineAsync(JsonSerializer.Serialize(response, BridgeProtocol.JsonOptions))
                    .ConfigureAwait(false);
            }
        }
    }

    public async ValueTask DisposeAsync()
    {
        _stop.Cancel();
        _listener.Stop();
        if (_acceptTask is not null)
            try { await _acceptTask.ConfigureAwait(false); } catch (OperationCanceledException) { }
        _stop.Dispose();
    }
}
