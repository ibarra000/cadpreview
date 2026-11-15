namespace CadPreview;

/// <summary>Detected file format. New values are appended in later versions.</summary>
public enum CadFormat
{
    /// <summary>Not a supported format.</summary>
    Unknown = 0,

    /// <summary>AutoCAD DWG drawing.</summary>
    Dwg = 1,

    /// <summary>AutoCAD DXF drawing (ASCII or binary).</summary>
    Dxf = 2,
}
