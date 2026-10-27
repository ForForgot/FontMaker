#pragma once

#include "fontmaker/packing.hpp"

#include <cstddef>
#include <string>

namespace fontmaker {

/// Sidecar metadata that makes a generated font reproducible.
///
/// The payload hash is always computed over the BIN payload, regardless of
/// whether the user requested C source or BIN output.
struct FontMetadata {
    std::string generatorVersion;
    std::string requestedFont;
    std::string actualFont;
    int fontPixelHeight = 0;
    int cellWidth = 0;
    int cellHeight = 0;
    int offsetX = 0;
    int offsetY = 0;
    PackOptions packing;
    std::size_t glyphCount = 0;
    std::size_t emptyGlyphs = 0;
    std::size_t clippedGlyphs = 0;
    int recordSize = 0;
    std::string charsetSource;
    std::string outputFormat;
    std::string payloadSha256;
};

[[nodiscard]] std::string toJson(const FontMetadata& metadata);

} // namespace fontmaker
