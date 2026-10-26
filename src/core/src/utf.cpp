#include "fontmaker/utf.hpp"

#include "fontmaker/error.hpp"

#include <string>

namespace fontmaker {
namespace {

[[noreturn]] void invalidUtf8(std::size_t offset)
{
    throw CharsetError("invalid UTF-8 sequence at byte " + std::to_string(offset));
}

void validateScalar(char32_t codepoint)
{
    if (codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        throw CharsetError("invalid Unicode scalar value");
    }
}

} // namespace

std::string utf8Encode(char32_t codepoint)
{
    validateScalar(codepoint);
    std::string result;
    if (codepoint <= 0x7F) {
        result.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        result.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        result.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        result.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return result;
}

std::u16string utf16Encode(char32_t codepoint)
{
    validateScalar(codepoint);
    if (codepoint <= 0xFFFF) {
        return std::u16string(1, static_cast<char16_t>(codepoint));
    }

    const char32_t value = codepoint - 0x10000;
    std::u16string result;
    result.push_back(static_cast<char16_t>(0xD800 + (value >> 10)));
    result.push_back(static_cast<char16_t>(0xDC00 + (value & 0x3FF)));
    return result;
}

std::vector<char32_t> utf8Decode(std::string_view text)
{
    std::vector<char32_t> result;
    result.reserve(text.size());

    std::size_t index = 0;
    while (index < text.size()) {
        const auto first = static_cast<unsigned char>(text[index]);
        if (first < 0x80) {
            result.push_back(static_cast<char32_t>(first));
            ++index;
            continue;
        }

        std::size_t length = 0;
        char32_t value = 0;
        if ((first & 0xE0) == 0xC0) {
            length = 2;
            value = first & 0x1F;
        } else if ((first & 0xF0) == 0xE0) {
            length = 3;
            value = first & 0x0F;
        } else if ((first & 0xF8) == 0xF0) {
            length = 4;
            value = first & 0x07;
        } else {
            invalidUtf8(index);
        }

        if (index + length > text.size()) {
            invalidUtf8(index);
        }

        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto continuation = static_cast<unsigned char>(text[index + offset]);
            if ((continuation & 0xC0) != 0x80) {
                invalidUtf8(index + offset);
            }
            value = (value << 6) | (continuation & 0x3F);
        }

        const bool overlong = (length == 2 && value < 0x80)
            || (length == 3 && value < 0x800)
            || (length == 4 && value < 0x10000);
        if (overlong || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
            invalidUtf8(index);
        }

        result.push_back(value);
        index += length;
    }

    return result;
}

std::vector<char32_t> utf16Decode(std::span<const char16_t> units, bool replaceUnpaired)
{
    std::vector<char32_t> result;
    result.reserve(units.size());

    for (std::size_t index = 0; index < units.size(); ++index) {
        const auto unit = static_cast<std::uint16_t>(units[index]);
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            if (index + 1 >= units.size()) {
                if (replaceUnpaired) {
                    result.push_back(U'\uFFFD');
                    continue;
                }
                throw CharsetError("unpaired high surrogate at UTF-16 index " + std::to_string(index));
            }
            const auto low = static_cast<std::uint16_t>(units[index + 1]);
            if (low < 0xDC00 || low > 0xDFFF) {
                if (replaceUnpaired) {
                    result.push_back(U'\uFFFD');
                    continue;
                }
                throw CharsetError("unpaired high surrogate at UTF-16 index " + std::to_string(index));
            }
            const char32_t value = 0x10000
                + ((static_cast<char32_t>(unit - 0xD800) << 10) | (low - 0xDC00));
            result.push_back(value);
            ++index;
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            if (replaceUnpaired) {
                result.push_back(U'\uFFFD');
                continue;
            }
            throw CharsetError("unpaired low surrogate at UTF-16 index " + std::to_string(index));
        } else {
            result.push_back(static_cast<char32_t>(unit));
        }
    }

    return result;
}

std::vector<char32_t> utf16CodeUnits(std::span<const char16_t> units)
{
    std::vector<char32_t> result;
    result.reserve(units.size());
    for (const char16_t unit : units) {
        const auto value = static_cast<std::uint16_t>(unit);
        if (value >= 0xD800 && value <= 0xDFFF) {
            result.push_back(U'\uFFFD');
        } else {
            result.push_back(static_cast<char32_t>(value));
        }
    }
    return result;
}

bool isCommentPrintable(char32_t codepoint) noexcept
{
    if (codepoint < 0x20 || codepoint == 0x7F || (codepoint >= 0x80 && codepoint <= 0x9F)) {
        return false;
    }
    // U+2028/U+2029 are line separators and would visually break a line comment
    // in some editors, even though C itself does not treat them as newlines.
    return codepoint != 0x2028 && codepoint != 0x2029;
}

} // namespace fontmaker
