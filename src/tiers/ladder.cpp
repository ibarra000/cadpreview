#include "tiers/ladder.hpp"

#include "common/errors.hpp"
#include "render/png.hpp"
#include "tiers/builtin_tiers.hpp"
#include "tiers/placeholder.hpp"

#include <algorithm>
#include <exception>
#include <format>
#include <new>
#include <utility>

namespace cadpreview {

namespace {

std::string join(const std::vector<std::string>& parts, std::string_view separator) {
    std::string joined;
    for (const std::string& part : parts) {
        if (!joined.empty()) {
            joined.append(separator);
        }
        joined.append(part);
    }
    return joined;
}

Preview baseFor(const Detection& detection) {
    Preview preview;
    preview.format = detection.format;
    preview.formatName = std::string(formatId(detection.format));
    preview.version = detection.version;
    return preview;
}

Preview finishImage(const Detection& detection, Tier tier, const Image& image,
                    const std::vector<std::string>& warnings) {
    Preview preview = baseFor(detection);
    preview.png = encodePng(image);
    preview.ok = true;
    preview.tier = tier;
    preview.width = image.width();
    preview.height = image.height();
    preview.warnings = join(warnings, "\n");
    return preview;
}

// One tier's outcome, or the reason it could not run here at all.
struct Attempt {
    TierOutcome outcome;
    std::string unavailable;
};

Attempt runTier(const TierEntry& entry, const TierContext& context) {
    try {
        return {entry.run(context), {}};
    } catch (const TierUnavailable& error) {
        return {{}, error.what()};
    } catch (const FormatError& error) {
        return {{std::nullopt, error.what(), {}}, {}};
    } catch (const std::bad_alloc&) {
        return {{std::nullopt, "out of memory", {}}, {}};
    } catch (const std::exception& error) {
        // Library exceptions (filesystem, iostream) can quote paths, so they stay out of the
        // placeholder text and go only to the caller's log.
        if (context.log.enabled()) {
            context.log.debug(std::format("tier {} raised: {}",
                                          static_cast<std::uint32_t>(entry.tier), error.what()));
        }
        return {{std::nullopt, "internal error", {}}, {}};
    } catch (...) {
        return {{std::nullopt, "internal error", {}}, {}};
    }
}

std::vector<const TierEntry*> candidates(const Detection& detection, const RenderOptions& options,
                                         const TierRegistry& registry) {
    if (options.tier == Tier::Auto) {
        return registry.tiersFor(detection.format);
    }
    const TierEntry* entry = registry.find(detection.format, options.tier);
    return entry != nullptr ? std::vector<const TierEntry*>{entry}
                            : std::vector<const TierEntry*>{};
}

std::string unavailableText(const Detection& detection, Tier tier) {
    if (tier == Tier::Auto) {
        return std::format("no tiers are implemented for format {}", formatId(detection.format));
    }
    return std::format("tier {} ({}) is not available for format {}",
                       static_cast<std::uint32_t>(tier), tierName(tier),
                       formatId(detection.format));
}

} // namespace

std::string_view reasonId(Reason reason) noexcept {
    switch (reason) {
    case Reason::None:
        return "none";
    case Reason::CannotOpen:
        return "cannot_open";
    case Reason::EmptyInput:
        return "empty_input";
    case Reason::UnknownFormat:
        return "unknown_format";
    case Reason::TierUnavailable:
        return "tier_unavailable";
    case Reason::TiersFailed:
        return "tiers_failed";
    case Reason::PlaceholderRequested:
        return "placeholder_requested";
    }
    return "unrecognized";
}

std::string_view tierName(Tier tier) noexcept {
    switch (tier) {
    case Tier::Auto:
        return "auto";
    case Tier::Thumbnail:
        return "thumbnail";
    case Tier::Mesh:
        return "mesh";
    case Tier::Brep:
        return "brep";
    case Tier::Vector:
        return "vector";
    case Tier::Placeholder:
        return "placeholder";
    }
    return "unrecognized";
}

TierRegistry::TierRegistry(std::vector<TierEntry> entries) : entries_(std::move(entries)) {
    std::ranges::stable_sort(entries_, {}, &TierEntry::tier);
}

std::vector<const TierEntry*> TierRegistry::tiersFor(Format format) const {
    std::vector<const TierEntry*> matches;
    for (const TierEntry& entry : entries_) {
        if (entry.format == format) {
            matches.push_back(&entry);
        }
    }
    return matches;
}

const TierEntry* TierRegistry::find(Format format, Tier tier) const {
    const auto match = std::ranges::find_if(
        entries_, [&](const TierEntry& e) { return e.format == format && e.tier == tier; });
    return match != entries_.end() ? &*match : nullptr;
}

const TierRegistry& defaultRegistry() {
    static const TierRegistry DEFAULT_REGISTRY([] {
        std::vector<TierEntry> entries = dxfTiers();
        std::vector<TierEntry> dwg = dwgTiers();
        entries.insert(entries.end(), dwg.begin(), dwg.end());
        return entries;
    }());
    return DEFAULT_REGISTRY;
}

Preview makePlaceholder(const Detection& detection, Reason reason, std::string reasonText,
                        std::uint32_t size, const std::vector<std::string>& warnings) {
    const Image image = drawPlaceholder(formatDisplayName(detection.format), reasonText, size);
    Preview preview = baseFor(detection);
    preview.png = encodePng(image);
    preview.width = image.width();
    preview.height = image.height();
    preview.reason = reason;
    preview.reasonText = std::move(reasonText);
    preview.warnings = join(warnings, "\n");
    return preview;
}

Preview runLadder(Source& source, const Detection& detection, const RenderOptions& options,
                  const TierRegistry& registry, const Logger& log) {
    if (options.tier == Tier::Placeholder) {
        return makePlaceholder(detection, Reason::PlaceholderRequested, "placeholder requested",
                               options.size);
    }
    if (detection.format == Format::Unknown) {
        return makePlaceholder(detection, Reason::UnknownFormat, detection.detail, options.size);
    }
    const auto tiers = candidates(detection, options, registry);
    if (tiers.empty()) {
        return makePlaceholder(detection, Reason::TierUnavailable,
                               unavailableText(detection, options.tier), options.size);
    }
    if (options.tier != Tier::Auto && !tiers.front()->unavailable.empty()) {
        return makePlaceholder(detection, Reason::TierUnavailable,
                               std::string(tiers.front()->unavailable), options.size);
    }

    const TierContext context{source, detection, options.size, log, options.dwgHelper};
    std::vector<std::string> failures;
    std::vector<std::string> warnings;
    for (const TierEntry* entry : tiers) {
        if (!entry->unavailable.empty()) {
            warnings.emplace_back(entry->unavailable);
            continue;
        }
        Attempt attempt = runTier(*entry, context);
        if (!attempt.unavailable.empty()) {
            if (options.tier != Tier::Auto) {
                return makePlaceholder(detection, Reason::TierUnavailable,
                                       std::move(attempt.unavailable), options.size, warnings);
            }
            warnings.push_back(std::move(attempt.unavailable));
            continue;
        }
        TierOutcome& outcome = attempt.outcome;
        warnings.insert(warnings.end(), outcome.warnings.begin(), outcome.warnings.end());
        const auto number = static_cast<std::uint32_t>(entry->tier);
        if (outcome.image) {
            if (log.enabled()) {
                log.debug(std::format("tier {} produced the preview", number));
            }
            return finishImage(detection, entry->tier, *outcome.image, warnings);
        }
        failures.push_back(std::format("tier {}: {}", number, outcome.failure));
        log.debug(failures.back());
    }
    return makePlaceholder(detection, Reason::TiersFailed, join(failures, "; "), options.size,
                           warnings);
}

} // namespace cadpreview
