namespace CadPreview;

/// <summary>Why a preview is a placeholder.</summary>
public enum FailureReason
{
    /// <summary>Not a placeholder.</summary>
    None = 0,

    /// <summary>The input could not be opened or read.</summary>
    CannotOpen = 1,

    /// <summary>The input has no bytes.</summary>
    EmptyInput = 2,

    /// <summary>The content matches no supported format.</summary>
    UnknownFormat = 3,

    /// <summary>The forced tier does not exist for this format.</summary>
    TierUnavailable = 4,

    /// <summary>Every applicable tier failed.</summary>
    TiersFailed = 5,

    /// <summary>The caller forced the placeholder tier.</summary>
    PlaceholderRequested = 6,
}
