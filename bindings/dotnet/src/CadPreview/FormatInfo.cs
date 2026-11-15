namespace CadPreview;

/// <summary>Result of format detection. New properties may be added in later versions.</summary>
public sealed record FormatInfo
{
    internal FormatInfo()
    {
    }

    /// <summary>Detected format.</summary>
    public CadFormat Format { get; init; }

    /// <summary>Stable short format id, such as "unknown".</summary>
    public string Name { get; init; } = string.Empty;

    /// <summary>Detected format version, or null.</summary>
    public string? Version { get; init; }
}
