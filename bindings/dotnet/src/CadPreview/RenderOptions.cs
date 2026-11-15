using System;

namespace CadPreview;

/// <summary>Options for <see cref="Previewer.Render(string, RenderOptions?)"/>.</summary>
public sealed class RenderOptions
{
    /// <summary>Smallest accepted <see cref="Size"/>.</summary>
    public const int MinSize = 16;

    /// <summary>Largest accepted <see cref="Size"/>.</summary>
    public const int MaxSize = 4096;

    /// <summary>Output edge length in pixels, 16 to 4096; 0 lets the library choose (512).</summary>
    public int Size { get; init; } = 512;

    /// <summary>Tier to force, or <see cref="Tier.Auto"/> to try them in order.</summary>
    public Tier Tier { get; init; } = Tier.Auto;

    /// <summary>Optional receiver for library log messages, called on the rendering thread.</summary>
    public Action<LogLevel, string>? Log { get; init; }
}
