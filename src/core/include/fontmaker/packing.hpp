#pragma once

#include "fontmaker/charset.hpp"
#include "fontmaker/glyph.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace fontmaker {

inline constexpr int kMinGlyphDimension = 1;
inline constexpr int kMaxGlyphDimension = 4096;
inline constexpr int kMinFontPixelHeight = 1;
inline constexpr int kMaxFontPixelHeight = 1024;
inline constexpr int kMaxGlyphOffset = 4096;
inline constexpr std::size_t kMaxOutputBytes = 256u * 1024u * 1024u;

enum class ScanDirection {
    Horizontal,
    Vertical,
};

enum class BitOrder {
    MsbFirst,
    LsbFirst,
};

struct PackOptions {
    ScanDirection scan = ScanDirection::Horizontal;
    BitOrder bitOrder = BitOrder::MsbFirst;
    bool variableWidth = false;
};

[[nodiscard]] const char* toString(ScanDirection value) noexcept;
[[nodiscard]] const char* toString(BitOrder value) noexcept;

/// Number of bytes in one glyph record, including the 2-byte width prefix
/// when variable-width output is enabled.
[[nodiscard]] std::size_t recordSizeBytes(int width, int height, const PackOptions& options);

/// Pack one glyph. The returned vector always has exactly
/// recordSizeBytes(glyph.width, glyph.height, options) bytes.
[[nodiscard]] std::vector<std::uint8_t> packGlyph(const Glyph& glyph, const PackOptions& options);

struct EncodedGlyph {
    char32_t codepoint = 0;
    std::vector<std::uint8_t> bytes;
    std::string note;
};

struct EncodeStats {
    std::size_t emptyGlyphs = 0;
    std::size_t clippedGlyphs = 0;
};

struct EncodedFont {
    int width = 0;
    int height = 0;
    int recordSize = 0;
    PackOptions options;
    EncodeStats stats;
    std::vector<EncodedGlyph> glyphs;
};

using ProgressCallback = std::function<void(std::size_t done, std::size_t total)>;

/// Rasterize and pack every code point in `charset`.
///
/// The callback is invoked after each glyph and can be used by a UI to report
/// progress. It must not throw.
[[nodiscard]] EncodedFont encodeFont(
    GlyphRasterizer& rasterizer,
    const Charset& charset,
    const PackOptions& options,
    const ProgressCallback& progress = {});

} // namespace fontmaker
