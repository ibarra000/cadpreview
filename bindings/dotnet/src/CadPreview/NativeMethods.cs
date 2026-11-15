using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

[assembly: DisableRuntimeMarshalling]

// Resolve cadpreview.dll only from the package's runtimes/<rid>/native assets or next to this
// assembly, never from the current directory or PATH.
[assembly: DefaultDllImportSearchPaths(DllImportSearchPath.AssemblyDirectory)]

namespace CadPreview;

internal static unsafe partial class NativeMethods
{
    private const string Library = "cadpreview";

    internal const int StatusOk = 0;
    internal const int StatusInvalidArgument = 1;
    internal const int StatusOutOfMemory = 2;

    [LibraryImport(Library, EntryPoint = "cp_render_file", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int RenderFile(string utf8Path, NativeOptions* options, out PreviewSafeHandle preview);

    [LibraryImport(Library, EntryPoint = "cp_render_memory")]
    internal static partial int RenderMemory(byte* data, nuint length, NativeOptions* options, out PreviewSafeHandle preview);

    [LibraryImport(Library, EntryPoint = "cp_detect_format", StringMarshalling = StringMarshalling.Utf8)]
    internal static partial int DetectFormat(string utf8Path, NativeFormatInfo* info);

    [LibraryImport(Library, EntryPoint = "cp_preview_png")]
    internal static partial int PreviewPng(nint preview, byte** data, nuint* length);

    [LibraryImport(Library, EntryPoint = "cp_preview_info")]
    internal static partial int PreviewInfo(nint preview, NativePreviewInfo* info);

    [LibraryImport(Library, EntryPoint = "cp_preview_free")]
    internal static partial void PreviewFree(nint preview);

    [LibraryImport(Library, EntryPoint = "cp_version")]
    internal static partial byte* Version();
}
