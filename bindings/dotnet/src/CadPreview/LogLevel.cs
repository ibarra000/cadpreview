namespace CadPreview;

/// <summary>Severity of a library log message.</summary>
public enum LogLevel
{
    /// <summary>Diagnostic detail.</summary>
    Debug = 0,

    /// <summary>Normal progress.</summary>
    Info = 1,

    /// <summary>Something was skipped or degraded.</summary>
    Warning = 2,

    /// <summary>An operation failed.</summary>
    Error = 3,
}
