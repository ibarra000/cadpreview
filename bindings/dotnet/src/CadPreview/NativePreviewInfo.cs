using System.Runtime.InteropServices;

namespace CadPreview;

/// <summary>Mirror of struct cp_preview_info (version 1).</summary>
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativePreviewInfo
{
    public uint StructSize;
    public int Ok;
    public uint Tier;
    public uint Width;
    public uint Height;
    public int Format;
    public int ReasonCode;
    public uint Reserved;
    public ulong RenderTimeUs;
    public byte* FormatName;
    public byte* Version;
    public byte* Reason;
    public byte* Warnings;
}
