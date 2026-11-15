#include "render/compose.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace cadpreview {

namespace {

constexpr Rgba WHITE{255, 255, 255, 255};

using Accumulator = std::array<double, 3>;

// Adds a pixel to an accumulator after blending it onto white.
void accumulate(Accumulator& sum, Rgba c) {
    const double alpha = c.a / 255.0;
    sum[0] += c.r * alpha + 255.0 * (1.0 - alpha);
    sum[1] += c.g * alpha + 255.0 * (1.0 - alpha);
    sum[2] += c.b * alpha + 255.0 * (1.0 - alpha);
}

Rgba toColor(const Accumulator& sum, double weight) {
    const auto channel = [&](double v) {
        return static_cast<std::uint8_t>(std::clamp(std::lround(v / weight), 0L, 255L));
    };
    return {channel(sum[0]), channel(sum[1]), channel(sum[2]), 255};
}

struct Fit {
    double scale = 1.0;
    std::uint32_t width = 1;
    std::uint32_t height = 1;
    std::uint32_t left = 0;
    std::uint32_t top = 0;
};

Fit fitInside(const Image& source, std::uint32_t size) {
    const auto inner =
        static_cast<double>(size - 2 * static_cast<std::uint32_t>(canvasMargin(size)));
    Fit fit;
    fit.scale = std::min(inner / source.width(), inner / source.height());
    fit.width = std::max<std::uint32_t>(
        1, static_cast<std::uint32_t>(std::lround(source.width() * fit.scale)));
    fit.height = std::max<std::uint32_t>(
        1, static_cast<std::uint32_t>(std::lround(source.height() * fit.scale)));
    fit.left = (size - fit.width) / 2;
    fit.top = (size - fit.height) / 2;
    return fit;
}

// Averages every source pixel the destination pixel covers.
Rgba areaAverage(const Image& source, double x0, double y0, double x1, double y1) {
    const auto first = [](double v) {
        return static_cast<std::uint32_t>(std::floor(v));
    };
    const auto last = [](double v, std::uint32_t limit) {
        return std::min(limit, static_cast<std::uint32_t>(std::ceil(v)));
    };
    Accumulator sum{};
    double count = 0;
    for (std::uint32_t y = first(y0); y < last(y1, source.height()); ++y) {
        for (std::uint32_t x = first(x0); x < last(x1, source.width()); ++x) {
            accumulate(sum, source.pixel(x, y));
            count += 1;
        }
    }
    return count > 0 ? toColor(sum, count) : WHITE;
}

Rgba bilinear(const Image& source, double x, double y) {
    const double cx = std::clamp(x, 0.0, static_cast<double>(source.width() - 1));
    const double cy = std::clamp(y, 0.0, static_cast<double>(source.height() - 1));
    const auto x0 = static_cast<std::uint32_t>(cx);
    const auto y0 = static_cast<std::uint32_t>(cy);
    const std::uint32_t x1 = std::min(x0 + 1, source.width() - 1);
    const std::uint32_t y1 = std::min(y0 + 1, source.height() - 1);
    const double fx = cx - x0;
    const double fy = cy - y0;
    std::array<Accumulator, 4> corners{}; // top-left, top-right, bottom-left, bottom-right
    accumulate(corners[0], source.pixel(x0, y0));
    accumulate(corners[1], source.pixel(x1, y0));
    accumulate(corners[2], source.pixel(x0, y1));
    accumulate(corners[3], source.pixel(x1, y1));
    Accumulator result{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        const double upper = corners[0][i] + (corners[1][i] - corners[0][i]) * fx;
        const double lower = corners[2][i] + (corners[3][i] - corners[2][i]) * fx;
        result[i] = upper + (lower - upper) * fy;
    }
    return toColor(result, 1.0);
}

} // namespace

int canvasMargin(std::uint32_t size) noexcept {
    return std::max(1, static_cast<int>((static_cast<std::uint64_t>(size) * 5 + 50) / 100));
}

Image composeOnCanvas(const Image& source, std::uint32_t size) {
    Image canvas(size, size, WHITE);
    const Fit fit = fitInside(source, size);
    for (std::uint32_t dy = 0; dy < fit.height; ++dy) {
        for (std::uint32_t dx = 0; dx < fit.width; ++dx) {
            const Rgba color =
                fit.scale < 1.0
                    ? areaAverage(source, dx / fit.scale, dy / fit.scale, (dx + 1) / fit.scale,
                                  (dy + 1) / fit.scale)
                    : bilinear(source, (dx + 0.5) / fit.scale - 0.5, (dy + 0.5) / fit.scale - 0.5);
            canvas.setPixel(fit.left + dx, fit.top + dy, color);
        }
    }
    return canvas;
}

} // namespace cadpreview
