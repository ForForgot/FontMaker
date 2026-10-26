#pragma once

#include "fontmaker/packing.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace fontmaker {

enum class OutputFormat {
    CSource,
    Binary,
};

struct WriterOptions {
    std::string arrayName = "bits";
    std::size_t bytesPerLine = 16;
};

/// Generate a UTF-8 C source file containing one row per glyph.
///
/// The generated array has the same layout as the legacy tool:
/// `static const unsigned char bits[][recordSize]`, one record per character
/// in charset order. Comments show U+XXXX and, when printable, the character.
[[nodiscard]] std::string toCSource(const EncodedFont& font, const WriterOptions& options = {});

/// Concatenate glyph records in charset order without any header.
[[nodiscard]] std::vector<std::uint8_t> toBinary(const EncodedFont& font);

void saveFont(
    const std::filesystem::path& path,
    const EncodedFont& font,
    OutputFormat format,
    const WriterOptions& options = {});

} // namespace fontmaker
