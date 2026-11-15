using System.Runtime.InteropServices;
using Xunit;

namespace CadPreview.Tests;

public sealed class InteropLayoutTests
{
    [Fact]
    public unsafe void NativeStructs_MatchTheVersion1CLayout()
    {
        Assert.Equal(32, sizeof(NativeOptions));
        Assert.Equal(72, sizeof(NativePreviewInfo));
        Assert.Equal(104, sizeof(NativeFormatInfo));
        Assert.Equal(16, (int)Marshal.OffsetOf<NativeOptions>(nameof(NativeOptions.Log)));
        Assert.Equal(32, (int)Marshal.OffsetOf<NativePreviewInfo>(nameof(NativePreviewInfo.RenderTimeUs)));
    }

    [Fact]
    public void Enums_MatchTheCConstants()
    {
        Assert.Equal(5, (int)Tier.Placeholder);
        Assert.Equal(6, (int)FailureReason.PlaceholderRequested);
        Assert.Equal(3, (int)LogLevel.Error);
    }
}
