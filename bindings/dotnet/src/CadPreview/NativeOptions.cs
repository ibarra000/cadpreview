using System.Runtime.InteropServices;

namespace CadPreview;

/// <summary>Mirror of cp_options (version 1).</summary>
[StructLayout(LayoutKind.Sequential)]
internal unsafe struct NativeOptions
{
    public uint StructSize;
    public uint Size;
    public uint Tier;
    public uint Reserved;
    public delegate* unmanaged<nint, int, byte*, void> Log;
    public nint LogUser;
}
