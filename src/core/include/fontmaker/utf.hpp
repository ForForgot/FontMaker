#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fontmaker {

[[nodiscard]] std::string utf8Encode(char32_t codepoint);
[[nodiscard]] std::u16string utf16Encode(char32_t codepoint);
[[nodiscard]] std::vector<char32_t> utf8Decode(std::string_view text);
[[nodiscard]] std::vector<char32_t> utf16Decode(
    std::span<const char16_t> units, bool replaceUnpaired = false);

/// Decode a legacy `.cst` stream as independent UTF-16 code units.
/// Surrogate units are replaced with U+FFFD and are never paired. This
/// preserves the one-record-per-unit layout of the original tool, including
/// UNICODE.cst which deliberately contains the surrogate range.
[[nodiscard]] std::vector<char32_t> utf16CodeUnits(std::span<const char16_t> units);

/// True when a code point can safely be shown inside a line comment.
[[nodiscard]] bool isCommentPrintable(char32_t codepoint) noexcept;

} // namespace fontmaker
