using System.Runtime.InteropServices;

namespace CadPreview;

/// <summary>Mirror of cp_format_info (version 1).</summary>
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeFormatInfo
{
    public uint StructSize;
    public int Format;
    public fixed byte Name[32];
    public fixed byte Version[64];
}
