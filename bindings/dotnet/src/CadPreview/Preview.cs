using System;
using System.IO;

namespace CadPreview;

/// <summary>A rendered PNG preview held in native memory. Dispose it when done.</summary>
public sealed class Preview : IDisposable
{
    private readonly PreviewSafeHandle _handle;

    internal Preview(PreviewSafeHandle handle, PreviewInfo info)
    {
        _handle = handle;
        Info = info;
    }

    /// <summary>Facts about the preview, copied when it was rendered.</summary>
    public PreviewInfo Info { get; }

    /// <summary>Returns a copy of the PNG bytes.</summary>
    public byte[] ToArray() => WithPng(png => png.ToArray());

    /// <summary>Writes the PNG bytes to a stream.</summary>
    public void CopyTo(Stream destination)
    {
        ArgumentNullException.ThrowIfNull(destination);
        WithPng(png =>
        {
            destination.Write(png);
            return true;
        });
    }

    /// <summary>Writes the PNG to a file, replacing it if it exists.</summary>
    public void Save(string path)
    {
        ArgumentException.ThrowIfNullOrEmpty(path);
        var png = ToArray(); // before touching the file, so a disposed preview cannot truncate it
        File.WriteAllBytes(path, png);
    }

    /// <summary>Releases the native preview.</summary>
    public void Dispose() => _handle.Dispose();

    private delegate T PngReader<T>(ReadOnlySpan<byte> png);

    private unsafe T WithPng<T>(PngReader<T> reader)
    {
        ObjectDisposedException.ThrowIf(_handle.IsClosed, this);
        var added = false;
        try
        {
            _handle.DangerousAddRef(ref added);
            byte* data = null;
            nuint length = 0;
            var status = NativeMethods.PreviewPng(_handle.DangerousGetHandle(), &data, &length);
            Status.ThrowIfFailed(status);
            return reader(new ReadOnlySpan<byte>(data, checked((int)length)));
        }
        finally
        {
            if (added)
            {
                _handle.DangerousRelease();
            }
        }
    }
}
