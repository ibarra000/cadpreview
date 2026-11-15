#include "tiers/pipeline.hpp"

#include <chrono>
#include <format>
#include <string>

namespace cadpreview {

namespace {

using Clock = std::chrono::steady_clock;

Preview stamped(Preview preview, Clock::time_point start, const Logger& log) {
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
    preview.renderTimeUs = static_cast<std::uint64_t>(elapsed.count());
    if (!log.enabled()) {
        return preview;
    }
    if (preview.ok) {
        log.info(std::format("format {}: preview from tier {} ({})", preview.formatName,
                             static_cast<std::uint32_t>(preview.tier), tierName(preview.tier)));
    } else {
        log.info(std::format("format {}: placeholder ({}: {})", preview.formatName,
                             reasonId(preview.reason), preview.reasonText));
    }
    return preview;
}

Preview renderDetected(Source& source, const RenderOptions& options, const TierRegistry& registry,
                       const Logger& log) {
    if (source.size() == 0) {
        return makePlaceholder(Detection{}, Reason::EmptyInput, "empty input", options.size);
    }
    const Detection detection = detect(source);
    if (detection.readFailed) {
        return makePlaceholder(detection, Reason::CannotOpen, detection.detail, options.size);
    }
    if (log.enabled()) {
        log.debug(
            std::format("detected format {} ({})", formatId(detection.format), detection.detail));
    }
    return runLadder(source, detection, options, registry, log);
}

} // namespace

Preview renderSource(Source& source, const RenderOptions& options, const TierRegistry& registry,
                     const Logger& log) {
    const auto start = Clock::now();
    return stamped(renderDetected(source, options, registry, log), start, log);
}

Preview renderFile(const std::filesystem::path& path, const RenderOptions& options,
                   const TierRegistry& registry, const Logger& log) {
    const auto start = Clock::now();
    const FileOpenResult opened = openFile(path);
    if (!opened.source) {
        Detection detection;
        detection.readFailed = true;
        return stamped(
            makePlaceholder(detection, Reason::CannotOpen, std::string(opened.error), options.size),
            start, log);
    }
    return stamped(renderDetected(*opened.source, options, registry, log), start, log);
}

Preview renderMemory(std::span<const std::byte> data, const RenderOptions& options,
                     const TierRegistry& registry, const Logger& log) {
    MemorySource source(data);
    return renderSource(source, options, registry, log);
}

} // namespace cadpreview
