using System;

namespace CadPreview;

internal static class Status
{
    internal static void ThrowIfFailed(int status)
    {
        switch (status)
        {
            case NativeMethods.StatusOk:
                return;
            case NativeMethods.StatusInvalidArgument:
                throw new ArgumentException("The native cadpreview library rejected an argument.");
            case NativeMethods.StatusOutOfMemory:
                throw new InsufficientMemoryException("The native cadpreview library ran out of memory.");
            default:
                throw new InvalidOperationException($"Unexpected cadpreview status {status}.");
        }
    }
}
