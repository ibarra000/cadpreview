using Microsoft.Win32.SafeHandles;

namespace CadPreview;

internal sealed class PreviewSafeHandle : SafeHandleZeroOrMinusOneIsInvalid
{
    // The LibraryImport marshaller creates instances through this constructor.
    public PreviewSafeHandle()
        : base(ownsHandle: true)
    {
    }

    protected override bool ReleaseHandle()
    {
        NativeMethods.PreviewFree(handle);
        return true;
    }
}
