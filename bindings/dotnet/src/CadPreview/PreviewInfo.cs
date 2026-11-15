using System;
using System.Collections.Generic;
using System.Linq;

namespace CadPreview;

/// <summary>Facts about a rendered preview. New properties may be added in later versions.</summary>
public sealed record PreviewInfo
{
    internal PreviewInfo()
    {
    }

    /// <summary>True for a real preview, false for a placeholder.</summary>
    public bool Ok { get; init; }

    /// <summary>Tier that produced the image.</summary>
    public Tier Tier { get; init; }

    /// <summary>Image width in pixels.</summary>
    public int Width { get; init; }

    /// <summary>Image height in pixels.</summary>
    public int Height { get; init; }

    /// <summary>Detected format.</summary>
    public CadFormat Format { get; init; }

    /// <summary>Stable short format id, such as "unknown".</summary>
    public string FormatName { get; init; } = string.Empty;

    /// <summary>Detected format version, or null.</summary>
    public string? Version { get; init; }

    /// <summary>Why the preview is a placeholder.</summary>
    public FailureReason Reason { get; init; }

    /// <summary>Human-readable reason, or an empty string.</summary>
    public string ReasonText { get; init; } = string.Empty;

    /// <summary>Warnings raised while rendering.</summary>
    public IReadOnlyList<string> Warnings { get; init; } = [];

    /// <summary>Time spent rendering.</summary>
    public TimeSpan RenderTime { get; init; }

    /// <summary>Compares every property, including the warnings element by element.</summary>
    public bool Equals(PreviewInfo? other) =>
        other is not null
        && Ok == other.Ok
        && Tier == other.Tier
        && Width == other.Width
        && Height == other.Height
        && Format == other.Format
        && FormatName == other.FormatName
        && Version == other.Version
        && Reason == other.Reason
        && ReasonText == other.ReasonText
        && RenderTime == other.RenderTime
        && Warnings.SequenceEqual(other.Warnings);

    /// <summary>Hash consistent with <see cref="Equals(PreviewInfo?)"/>.</summary>
    public override int GetHashCode()
    {
        var hash = new HashCode();
        hash.Add(Ok);
        hash.Add(Tier);
        hash.Add(Width);
        hash.Add(Height);
        hash.Add(Format);
        hash.Add(FormatName);
        hash.Add(Version);
        hash.Add(Reason);
        hash.Add(ReasonText);
        hash.Add(RenderTime);
        foreach (var warning in Warnings)
        {
            hash.Add(warning);
        }

        return hash.ToHashCode();
    }
}
