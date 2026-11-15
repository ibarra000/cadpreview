using System;
using System.Collections.Concurrent;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Xunit;

namespace CadPreview.Tests;

public sealed class PreviewerTests : IDisposable
{
    private static readonly byte[] PngSignature = [0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A];
    private readonly string _directory = Directory.CreateTempSubdirectory("cadpreview-net-").FullName;

    public void Dispose() => Directory.Delete(_directory, recursive: true);

    [Fact]
    public void Render_MissingFile_ReturnsCannotOpenPlaceholder()
    {
        using var preview = Previewer.Render(Path.Combine(_directory, "missing.sldprt"));

        Assert.False(preview.Info.Ok);
        Assert.Equal(Tier.Placeholder, preview.Info.Tier);
        Assert.Equal(FailureReason.CannotOpen, preview.Info.Reason);
        Assert.Equal(512, preview.Info.Width);
        Assert.Equal("unknown", preview.Info.FormatName);
        Assert.Null(preview.Info.Version);
        Assert.Empty(preview.Info.Warnings);
        Assert.False(string.IsNullOrEmpty(preview.Info.ReasonText));
    }

    [Fact]
    public void Render_Bytes_ReturnsUnknownFormatPngOfRequestedSize()
    {
        using var preview = Previewer.Render("not a cad file"u8, new RenderOptions { Size = 96 });

        Assert.Equal(FailureReason.UnknownFormat, preview.Info.Reason);
        Assert.Equal(96, preview.Info.Height);
        Assert.Equal(PngSignature, preview.ToArray().Take(8));
    }

    [Fact]
    public void Render_EmptySpan_ReturnsEmptyInputPlaceholder()
    {
        using var preview = Previewer.Render(ReadOnlySpan<byte>.Empty);

        Assert.Equal(FailureReason.EmptyInput, preview.Info.Reason);
    }

    [Fact]
    public void Render_ForcedPlaceholder_ReportsRequest()
    {
        using var preview = Previewer.Render("x"u8, new RenderOptions { Tier = Tier.Placeholder });

        Assert.Equal(FailureReason.PlaceholderRequested, preview.Info.Reason);
    }

    [Fact]
    public void Save_And_CopyTo_WriteTheSamePng()
    {
        using var preview = Previewer.Render("x"u8, new RenderOptions { Size = 32 });
        var file = Path.Combine(_directory, "out.png");
        using var stream = new MemoryStream();

        preview.Save(file);
        preview.CopyTo(stream);

        Assert.Equal(File.ReadAllBytes(file), stream.ToArray());
        Assert.Equal(preview.ToArray(), stream.ToArray());
    }

    [Theory]
    [InlineData(15)]
    [InlineData(4097)]
    [InlineData(-1)]
    public void Render_SizeOutOfRange_Throws(int size)
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => Previewer.Render("x"u8, new RenderOptions { Size = size }));
    }

    [Fact]
    public void Render_UndefinedTier_Throws()
    {
        Assert.Throws<ArgumentOutOfRangeException>(() => Previewer.Render("x"u8, new RenderOptions { Tier = (Tier)9 }));
    }

    [Fact]
    public void Render_NullPath_Throws()
    {
        Assert.Throws<ArgumentNullException>(() => Previewer.Render((string)null!));
        Assert.Throws<ArgumentNullException>(() => Previewer.DetectFormat(null!));
    }

    [Fact]
    public void Dispose_IsIdempotent_AndBlocksFurtherUse()
    {
        var preview = Previewer.Render("x"u8);
        preview.Dispose();
        preview.Dispose();

        Assert.Throws<ObjectDisposedException>(() => preview.ToArray());
        Assert.False(preview.Info.Ok);
    }

    [Fact]
    public void Log_ReceivesMessages()
    {
        var messages = new ConcurrentQueue<(LogLevel Level, string Text)>();
        using var preview = Previewer.Render("x"u8, new RenderOptions { Log = (l, m) => messages.Enqueue((l, m)) });

        Assert.Contains(messages, m => m.Level == LogLevel.Info && m.Text.Contains("placeholder", StringComparison.Ordinal));
    }

    [Fact]
    public void Log_CallbackException_IsRethrownAfterTheNativeCall()
    {
        var calls = 0;
        var options = new RenderOptions
        {
            Log = (_, _) =>
            {
                calls++;
                throw new InvalidOperationException("callback failed");
            },
        };

        var error = Assert.Throws<InvalidOperationException>(() => Previewer.Render("x"u8, options));

        Assert.Equal("callback failed", error.Message);
        Assert.Equal(1, calls);
    }

    [Fact]
    public void DetectFormat_UnreadableFile_IsUnknown()
    {
        var info = Previewer.DetectFormat(Path.Combine(_directory, "missing.dwg"));

        Assert.Equal(CadFormat.Unknown, info.Format);
        Assert.Equal("unknown", info.Name);
        Assert.Null(info.Version);
    }

    [Fact]
    public void DetectFormat_CompoundFile_IsReadAndStillUnknown()
    {
        var file = Path.Combine(_directory, "container.bin");
        File.WriteAllBytes(file, [0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1, .. new byte[504]]);

        var info = Previewer.DetectFormat(file);

        Assert.Equal("unknown", info.Name);
    }

    [Fact]
    public void Render_NonAsciiPath_IsOpened()
    {
        var file = Path.Combine(_directory, "pièce-零件.bin");
        File.WriteAllBytes(file, [1, 2, 3]);

        using var preview = Previewer.Render(file);

        Assert.Equal(FailureReason.UnknownFormat, preview.Info.Reason);
    }

    [Fact]
    public void PathWithNul_Throws()
    {
        Assert.Throws<ArgumentException>(() => Previewer.Render("a.dwg\0.txt"));
        Assert.Throws<ArgumentException>(() => Previewer.DetectFormat("a.dwg\0.txt"));
    }

    [Fact]
    public void Render_SizeZero_UsesTheLibraryDefault()
    {
        using var preview = Previewer.Render("x"u8, new RenderOptions { Size = 0 });

        Assert.Equal(512, preview.Info.Width);
    }

    [Fact]
    public void Save_AfterDispose_LeavesAnExistingFileIntact()
    {
        var file = Path.Combine(_directory, "keep.png");
        File.WriteAllBytes(file, [1, 2, 3]);
        var preview = Previewer.Render("x"u8);
        preview.Dispose();

        Assert.Throws<ObjectDisposedException>(() => preview.Save(file));
        Assert.Equal([1, 2, 3], File.ReadAllBytes(file));
    }

    [Fact]
    public void DisposeRacingWithReads_NeverCrashes()
    {
        for (var round = 0; round < 50; round++)
        {
            var preview = Previewer.Render("x"u8, new RenderOptions { Size = 16 });
            Parallel.Invoke(
                () => ReadIgnoringDisposal(preview),
                () => ReadIgnoringDisposal(preview),
                preview.Dispose);
        }
    }

    [Fact]
    public void Status_OutOfMemory_ThrowsInsufficientMemory()
    {
        Assert.Throws<InsufficientMemoryException>(() => Status.ThrowIfFailed(NativeMethods.StatusOutOfMemory));
        Assert.Throws<ArgumentException>(() => Status.ThrowIfFailed(NativeMethods.StatusInvalidArgument));
        Assert.Throws<InvalidOperationException>(() => Status.ThrowIfFailed(42));
    }

    [Fact]
    public void PreviewInfo_EqualityComparesWarningsByValue()
    {
        var a = new PreviewInfo { Warnings = ["w1", "w2"] };
        var b = new PreviewInfo { Warnings = ["w1", "w2"] };

        Assert.Equal(a, b);
        Assert.Equal(a.GetHashCode(), b.GetHashCode());
        Assert.NotEqual(a, b with { Warnings = ["w1"] });
    }

    private static void ReadIgnoringDisposal(Preview preview)
    {
        try
        {
            Assert.NotEmpty(preview.ToArray());
        }
        catch (ObjectDisposedException)
        {
            // Expected when Dispose wins the race; anything else fails the test.
        }
    }

    [Fact]
    public void Version_IsSemanticVersion()
    {
        Assert.Matches(@"^\d+\.\d+\.\d+$", Previewer.Version);
    }

    [Fact]
    public void Render_FromManyThreads_Succeeds()
    {
        var file = Path.Combine(_directory, "shared.bin");
        File.WriteAllBytes(file, [1, 2, 3]);

        Parallel.For(0, 200, i =>
        {
            using var preview = i % 2 == 0
                ? Previewer.Render(file, new RenderOptions { Size = 16 + (i % 8) })
                : Previewer.Render([1, 2, 3], new RenderOptions { Size = 16 + (i % 8) });
            Assert.Equal(16 + (i % 8), preview.Info.Width);
        });
    }
}
