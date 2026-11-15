using System;
using System.Runtime.ExceptionServices;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.Marshalling;

namespace CadPreview;

/// <summary>
/// Carries a managed log callback through the native log_user pointer for one render call.
/// Exceptions cannot cross back into native code, so the first one is kept and rethrown once
/// the native call has returned; later messages are dropped.
/// </summary>
internal sealed class LogRelay : IDisposable
{
    private readonly Action<LogLevel, string> _callback;
    private GCHandle _self;
    private ExceptionDispatchInfo? _error;

    internal LogRelay(Action<LogLevel, string> callback)
    {
        _callback = callback;
        _self = GCHandle.Alloc(this);
    }

    internal nint Cookie => GCHandle.ToIntPtr(_self);

    internal static LogRelay? For(Action<LogLevel, string>? callback) =>
        callback is null ? null : new LogRelay(callback);

    public void Dispose()
    {
        if (_self.IsAllocated)
        {
            _self.Free();
        }
    }

    internal void RethrowCallbackException() => _error?.Throw();

    [UnmanagedCallersOnly]
    internal static unsafe void Forward(nint user, int level, byte* message)
    {
        LogRelay? relay = null;
        try
        {
            relay = GCHandle.FromIntPtr(user).Target as LogRelay;
            if (relay is null || relay._error is not null)
            {
                return;
            }

            relay._callback((LogLevel)level, Utf8StringMarshaller.ConvertToManaged(message) ?? string.Empty);
        }
#pragma warning disable CA1031 // The exception is captured and rethrown after the native call returns.
        catch (Exception exception)
#pragma warning restore CA1031
        {
            if (relay is not null)
            {
                relay._error = ExceptionDispatchInfo.Capture(exception);
            }
        }
    }
}
