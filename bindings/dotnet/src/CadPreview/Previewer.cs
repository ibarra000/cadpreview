using System;
using System.Runtime.InteropServices.Marshalling;

namespace CadPreview;

/// <summary>Entry points for rendering CAD files to PNG previews.</summary>
/// <remarks>
/// The native library ships for win-x64 and win-arm64; on other platforms the first call throws
/// <see cref="DllNotFoundException"/>.
/// </remarks>
public static class Previewer
{
    /// <summary>Version of the native library, such as "0.1.0".</summary>
    public static unsafe string Version => Utf8StringMarshaller.ConvertToManaged(NativeMethods.Version()) ?? string.Empty;

    /// <summary>Renders the file at <paramref name="path"/>. Unreadable files give a placeholder.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="path"/> is null.</exception>
    /// <exception cref="ArgumentException"><paramref name="path"/> contains a NUL character.</exception>
    /// <exception cref="ArgumentOutOfRangeException">An option is outside its documented range.</exception>
    public static unsafe Preview Render(string path, RenderOptions? options = null)
    {
        ValidatePath(path);
        var settings = Validated(options);
        using var relay = LogRelay.For(settings.Log);
        var native = ToNative(settings, relay);
        var status = NativeMethods.RenderFile(path, &native, out var handle);
        return Complete(status, handle, relay);
    }

    /// <summary>Renders a file held in memory. Unsupported content gives a placeholder.</summary>
    /// <exception cref="ArgumentOutOfRangeException">An option is outside its documented range.</exception>
    public static unsafe Preview Render(ReadOnlySpan<byte> data, RenderOptions? options = null)
    {
        var settings = Validated(options);
        using var relay = LogRelay.For(settings.Log);
        var native = ToNative(settings, relay);
        PreviewSafeHandle handle;
        int status;
        fixed (byte* bytes = data)
        {
            status = NativeMethods.RenderMemory(bytes, (nuint)data.Length, &native, out handle);
        }

        return Complete(status, handle, relay);
    }

    /// <summary>Detects the format of the file at <paramref name="path"/> from its content.</summary>
    /// <exception cref="ArgumentNullException"><paramref name="path"/> is null.</exception>
    /// <exception cref="ArgumentException"><paramref name="path"/> contains a NUL character.</exception>
    public static unsafe FormatInfo DetectFormat(string path)
    {
        ValidatePath(path);
        NativeFormatInfo info = default;
        info.StructSize = (uint)sizeof(NativeFormatInfo);
        Status.ThrowIfFailed(NativeMethods.DetectFormat(path, &info));
        RequireFilled(info.StructSize, sizeof(NativeFormatInfo));
        var version = FixedString(new ReadOnlySpan<byte>(info.Version, 64));
        return new FormatInfo
        {
            Format = (CadFormat)info.Format,
            Name = FixedString(new ReadOnlySpan<byte>(info.Name, 32)),
            Version = version.Length == 0 ? null : version,
        };
    }

    private static void ValidatePath(string path)
    {
        ArgumentNullException.ThrowIfNull(path);
        if (path.Contains('\0', StringComparison.Ordinal))
        {
            throw new ArgumentException("The path contains a NUL character.", nameof(path));
        }
    }

    private static RenderOptions Validated(RenderOptions? options)
    {
        var settings = options ?? new RenderOptions();
        if (settings.Size is not 0 and (< RenderOptions.MinSize or > RenderOptions.MaxSize))
        {
            throw new ArgumentOutOfRangeException(
                nameof(options), settings.Size, $"{nameof(RenderOptions.Size)} must be 0 (default) or 16 to 4096.");
        }

        if (!Enum.IsDefined(settings.Tier))
        {
            throw new ArgumentOutOfRangeException(nameof(options), settings.Tier, $"Unknown {nameof(RenderOptions.Tier)}.");
        }

        return settings;
    }

    private static unsafe NativeOptions ToNative(RenderOptions settings, LogRelay? relay) => new()
    {
        StructSize = (uint)sizeof(NativeOptions),
        Size = (uint)settings.Size,
        Tier = (uint)settings.Tier,
        Log = relay is null ? null : &LogRelay.Forward,
        LogUser = relay?.Cookie ?? 0,
    };

    private static Preview Complete(int status, PreviewSafeHandle handle, LogRelay? relay)
    {
        try
        {
            relay?.RethrowCallbackException();
            Status.ThrowIfFailed(status);
            return new Preview(handle, ReadInfo(handle));
        }
        catch
        {
            handle.Dispose();
            throw;
        }
    }

    private static unsafe PreviewInfo ReadInfo(PreviewSafeHandle handle)
    {
        NativePreviewInfo info = default;
        info.StructSize = (uint)sizeof(NativePreviewInfo);
        var added = false;
        try
        {
            handle.DangerousAddRef(ref added);
            Status.ThrowIfFailed(NativeMethods.PreviewInfo(handle.DangerousGetHandle(), &info));
            RequireFilled(info.StructSize, sizeof(NativePreviewInfo));
            return ToManaged(info);
        }
        finally
        {
            if (added)
            {
                handle.DangerousRelease();
            }
        }
    }

    private static unsafe PreviewInfo ToManaged(in NativePreviewInfo info)
    {
        var version = Utf8StringMarshaller.ConvertToManaged(info.Version);
        var warnings = Utf8StringMarshaller.ConvertToManaged(info.Warnings) ?? string.Empty;
        return new PreviewInfo
        {
            Ok = info.Ok != 0,
            Tier = (Tier)info.Tier,
            Width = checked((int)info.Width),
            Height = checked((int)info.Height),
            Format = (CadFormat)info.Format,
            FormatName = Utf8StringMarshaller.ConvertToManaged(info.FormatName) ?? string.Empty,
            Version = string.IsNullOrEmpty(version) ? null : version,
            Reason = (FailureReason)info.ReasonCode,
            ReasonText = Utf8StringMarshaller.ConvertToManaged(info.Reason) ?? string.Empty,
            Warnings = warnings.Length == 0 ? [] : warnings.Split('\n'),
            RenderTime = TimeSpan.FromMicroseconds(checked((long)info.RenderTimeUs)),
        };
    }

    private static void RequireFilled(uint filled, int expected)
    {
        if (filled < expected)
        {
            throw new InvalidOperationException("The native cadpreview library is older than this wrapper.");
        }
    }

    private static string FixedString(ReadOnlySpan<byte> buffer)
    {
        var end = buffer.IndexOf((byte)0);
        return System.Text.Encoding.UTF8.GetString(end < 0 ? buffer : buffer[..end]);
    }
}
