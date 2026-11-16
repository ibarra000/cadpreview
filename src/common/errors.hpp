#pragma once

#include <stdexcept>

namespace cadpreview {

/// Thrown by format parsers for malformed or unsupported input. Its message becomes placeholder
/// text and log output, so it must never contain file paths or file content. Any other
/// exception escaping a tier is reported only as "internal error".
class FormatError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Thrown by a tier that cannot run in this environment, for example because a helper program
/// is not installed. The ladder reports it like a tier the build leaves out: a warning in auto
/// mode, CP_REASON_TIER_UNAVAILABLE when the tier is forced. Same message rules as FormatError.
class TierUnavailable : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

} // namespace cadpreview
