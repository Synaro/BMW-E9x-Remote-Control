namespace BmwE9x.EdiabasBridge.Core;

public interface IEdiabasEngine : IAsyncDisposable
{
    string EdiabasVersion { get; }
    string EcuPath { get; }
    string? ActiveSgbd { get; }
    bool IsConfigured { get; }
    bool IsJobRunning { get; }
    Task<IReadOnlyList<UsbDeviceInfo>> ListUsbDevicesAsync(CancellationToken cancellationToken);
    Task SetEcuPathAsync(string ecuPath, CancellationToken cancellationToken);
    Task ConnectAsync(CancellationToken cancellationToken);
    Task DisconnectAsync(CancellationToken cancellationToken);
    Task<string> IdentifySgbdAsync(string sgbd, CancellationToken cancellationToken);
    Task<IReadOnlyList<JobDefinition>> ListJobsAsync(string sgbd, CancellationToken cancellationToken);
    Task<JobExecutionResult> ExecuteJobAsync(string sgbd, string job, string arguments, string results,
        CancellationToken cancellationToken);
    Task AbortJobAsync(CancellationToken cancellationToken);
    Task<string> GetTraceAsync(CancellationToken cancellationToken);
}
