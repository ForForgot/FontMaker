#include "fontmaker/packing.hpp"

#include "fontmaker/error.hpp"

#include <algorithm>
#include <cstddef>
#include <utility>

namespace fontmaker {
namespace {

void validateCell(int width, int height)
{
    if (width <= 0 || height <= 0) {
        throw EncodeError("glyph cell dimensions must be positive");
    }
    if (width > kMaxGlyphDimension || height > kMaxGlyphDimension) {
        throw EncodeError("glyph cell dimensions are unreasonably large");
    }
}

std::uint8_t bitMask(BitOrder order, int index) noexcept
{
    return order == BitOrder::MsbFirst
        ? static_cast<std::uint8_t>(0x80u >> index)
        : static_cast<std::uint8_t>(1u << index);
}

bool glyphIsEmpty(const Glyph& glyph)
{
    return std::all_of(glyph.coverage.begin(), glyph.coverage.end(),
        [](std::uint8_t value) { return value == 0; });
}

bool glyphTouchesEdge(const Glyph& glyph)
{
    if (glyphIsEmpty(glyph)) {
        return false;
    }
    for (int x = 0; x < glyph.width; ++x) {
        if (glyph.pixel(x, 0) || glyph.pixel(x, glyph.height - 1)) {
            return true;
        }
    }
    for (int y = 0; y < glyph.height; ++y) {
        if (glyph.pixel(0, y) || glyph.pixel(glyph.width - 1, y)) {
            return true;
        }
    }
    return false;
}

} // namespace

const char* toString(ScanDirection value) noexcept
{
    return value == ScanDirection::Horizontal ? "horizontal" : "vertical";
}

const char* toString(BitOrder value) noexcept
{
    return value == BitOrder::MsbFirst ? "MSB first" : "LSB first";
}

std::size_t recordSizeBytes(int width, int height, const PackOptions& options)
{
    validateCell(width, height);
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    const std::size_t pixels = options.scan == ScanDirection::Horizontal
        ? h * ((w + 7) / 8)
        : w * ((h + 7) / 8);
    return pixels + (options.variableWidth ? 2 : 0);
}

std::vector<std::uint8_t> packGlyph(const Glyph& glyph, const PackOptions& options)
{
    if (!glyph.valid()) {
        throw EncodeError("rasterizer returned an invalid glyph");
    }
    if (options.variableWidth && (glyph.advance < 0 || glyph.advance > 0xFFFF)) {
        throw EncodeError("glyph advance does not fit in the 16-bit width prefix");
    }

    const auto width = static_cast<std::size_t>(glyph.width);
    const auto height = static_cast<std::size_t>(glyph.height);
    const auto dataSize = options.scan == ScanDirection::Horizontal
        ? height * ((width + 7) / 8)
        : width * ((height + 7) / 8);
    std::vector<std::uint8_t> bytes(dataSize, 0);

    if (options.scan == ScanDirection::Horizontal) {
        const std::size_t rowBytes = (width + 7) / 8;
        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                if (glyph.pixel(static_cast<int>(x), static_cast<int>(y))) {
                    bytes[y * rowBytes + x / 8] |= bitMask(options.bitOrder, static_cast<int>(x % 8));
                }
            }
        }
    } else {
        const std::size_t columnBytes = (height + 7) / 8;
        for (std::size_t x = 0; x < width; ++x) {
            for (std::size_t y = 0; y < height; ++y) {
                if (glyph.pixel(static_cast<int>(x), static_cast<int>(y))) {
                    bytes[x * columnBytes + y / 8]
                        |= bitMask(options.bitOrder, static_cast<int>(y % 8));
                }
            }
        }
    }

    if (options.variableWidth) {
        const auto prefix = static_cast<std::uint16_t>(glyph.advance);
        bytes.insert(bytes.begin(), {
            static_cast<std::uint8_t>(prefix >> 8),
            static_cast<std::uint8_t>(prefix & 0xFF),
        });
    }

    return bytes;
}

EncodedFont encodeFont(
    GlyphRasterizer& rasterizer,
    const Charset& charset,
    const PackOptions& options,
    const ProgressCallback& progress)
{
    if (charset.empty()) {
        throw EncodeError("the charset is empty");
    }

    EncodedFont result;
    result.options = options;
    result.glyphs.reserve(charset.size());

    for (std::size_t index = 0; index < charset.size(); ++index) {
        const char32_t codepoint = charset[index];
        const Glyph glyph = rasterizer.rasterize(codepoint);
        if (!glyph.valid()) {
            throw EncodeError("rasterizer returned an invalid glyph");
        }

        if (result.glyphs.empty()) {
            result.width = glyph.width;
            result.height = glyph.height;
            result.recordSize = static_cast<int>(recordSizeBytes(glyph.width, glyph.height, options));
            const std::size_t totalBytes = static_cast<std::size_t>(result.recordSize) * charset.size();
            if (totalBytes > kMaxOutputBytes) {
                throw EncodeError("output would exceed the 256 MiB safety limit");
            }
        } else if (glyph.width != result.width || glyph.height != result.height) {
            throw EncodeError("rasterizer returned inconsistent glyph dimensions");
        }

        if (glyphIsEmpty(glyph)) {
            ++result.stats.emptyGlyphs;
        } else if (glyphTouchesEdge(glyph)) {
            ++result.stats.edgeGlyphs;
        }

        result.glyphs.push_back({codepoint, packGlyph(glyph, options), charset.note(index)});
        if (progress) {
            progress(index + 1, charset.size());
        }
    }

    return result;
}

} // namespace fontmaker
