#pragma once

#include "common/log.hpp"
#include "containers/source.hpp"
#include "formats/detect.hpp"
#include "formats/format.hpp"
#include "render/image.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cadpreview {

/// Strategy tiers. Values match the public CP_TIER_* constants.
enum class Tier : std::uint32_t {
    Auto = 0,
    Thumbnail = 1,
    Mesh = 2,
    Brep = 3,
    Vector = 4,
    Placeholder = 5
};

/// Why a preview is a placeholder. Values match the public CP_REASON_* constants.
enum class Reason : std::int32_t {
    None = 0,
    CannotOpen = 1,
    EmptyInput = 2,
    UnknownFormat = 3,
    TierUnavailable = 4,
    TiersFailed = 5,
    PlaceholderRequested = 6,
};

[[nodiscard]] std::string_view reasonId(Reason reason) noexcept;
[[nodiscard]] std::string_view tierName(Tier tier) noexcept;

inline constexpr std::uint32_t DEFAULT_SIZE = 512;
inline constexpr std::uint32_t MIN_SIZE = 16;
inline constexpr std::uint32_t MAX_SIZE = 4096;

struct RenderOptions {
    std::uint32_t size = DEFAULT_SIZE;
    Tier tier = Tier::Auto;
    std::filesystem::path dwgHelper; ///< empty: look for cadpreview-dwg next to the library
};

// A short-lived view handed to one tier call, so reference members are intended.
// NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
struct TierContext {
    Source& source;
    const Detection& detection;
    std::uint32_t size;
    const Logger& log;
    const std::filesystem::path& dwgHelper;
};
// NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)

/// Result of one tier: an image, or a display-safe failure message. A tier that cannot run at
/// all here throws TierUnavailable instead (see common/errors.hpp).
struct TierOutcome {
    std::optional<Image> image;
    std::string failure;
    std::vector<std::string> warnings;
};

using TierFunction = std::function<TierOutcome(const TierContext&)>;

struct TierEntry {
    TierEntry(Format entryFormat, Tier entryTier, TierFunction function,
              std::string_view unavailableReason = {})
        : format(entryFormat), tier(entryTier), run(std::move(function)),
          unavailable(unavailableReason) {}

    Format format;
    Tier tier;
    TierFunction run;
    /// Set when this build leaves the tier out (for example DWG vectors without LibreDWG). The
    /// ladder then skips it with a warning, and forcing it reports this text.
    std::string_view unavailable;
};

/// Immutable table of the tiers implemented for each format.
class TierRegistry {
public:
    TierRegistry() = default;
    explicit TierRegistry(std::vector<TierEntry> entries);

    /// Tiers for a format, in ladder order.
    [[nodiscard]] std::vector<const TierEntry*> tiersFor(Format format) const;
    [[nodiscard]] const TierEntry* find(Format format, Tier tier) const;

private:
    std::vector<TierEntry> entries_;
};

/// Tiers built into this library version.
[[nodiscard]] const TierRegistry& defaultRegistry();

/// A finished preview, as exposed through cp_preview.
struct Preview {
    std::vector<std::uint8_t> png;
    bool ok = false;
    Tier tier = Tier::Placeholder;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    Format format = Format::Unknown;
    std::string formatName;
    std::string version;
    Reason reason = Reason::None;
    std::string reasonText;
    std::string warnings; ///< Newline-separated.
    std::uint64_t renderTimeUs = 0;
};

/// Builds the tier-5 preview for a detection result.
[[nodiscard]] Preview makePlaceholder(const Detection& detection, Reason reason,
                                      std::string reasonText, std::uint32_t size,
                                      const std::vector<std::string>& warnings = {});

/// Runs the strategy ladder for an already detected input.
[[nodiscard]] Preview runLadder(Source& source, const Detection& detection,
                                const RenderOptions& options, const TierRegistry& registry,
                                const Logger& log);

} // namespace cadpreview
