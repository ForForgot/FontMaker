#pragma once

#include <cstdint>
#include <vector>

namespace fontmaker {

/// A rasterized glyph in a fixed width x height cell.
///
/// `coverage` stores one byte per pixel, row-major from the top-left.
/// Any non-zero value means the pixel is part of the glyph. This keeps the
/// packer independent from the Windows GDI renderer and lets tests feed
/// deterministic bitmaps.
struct Glyph {
    int width = 0;
    int height = 0;
    int advance = 0;
    std::vector<std::uint8_t> coverage;

    [[nodiscard]] bool valid() const noexcept
    {
        return width > 0 && height > 0
            && coverage.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    }

    [[nodiscard]] bool pixel(int x, int y) const noexcept
    {
        if (x < 0 || y < 0 || x >= width || y >= height) {
            return false;
        }
        const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x);
        return coverage[index] != 0;
    }
};

class GlyphRasterizer {
public:
    virtual ~GlyphRasterizer() = default;

    /// Rasterize one Unicode code point into a fixed cell.
    ///
    /// Implementations must return a glyph whose width/height match the
    /// configured cell. The advance value is the font's horizontal advance
    /// in pixels and is only emitted when variable-width output is selected.
    [[nodiscard]] virtual Glyph rasterize(char32_t codepoint) = 0;
};

} // namespace fontmaker
