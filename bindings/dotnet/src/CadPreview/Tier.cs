namespace CadPreview;

/// <summary>Strategy tier that produced, or should produce, a preview.</summary>
public enum Tier
{
    /// <summary>Try every tier in order (options only).</summary>
    Auto = 0,

    /// <summary>Preview image embedded in the file.</summary>
    Thumbnail = 1,

    /// <summary>Cached display mesh.</summary>
    Mesh = 2,

    /// <summary>B-rep geometry from the modeling kernel stream.</summary>
    BRep = 3,

    /// <summary>2D vector entities (DWG/DXF).</summary>
    Vector = 4,

    /// <summary>Neutral placeholder image.</summary>
    Placeholder = 5,
}
