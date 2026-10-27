#include "fontmaker/charset.hpp"

#include "fontmaker/error.hpp"
#include "fontmaker/io.hpp"
#include "fontmaker/utf.hpp"

#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace fontmaker {
namespace {

bool isControl(char32_t codepoint) noexcept
{
    return codepoint < 0x20 || (codepoint >= 0x7F && codepoint <= 0x9F);
}

} // namespace

Charset::Charset(std::vector<char32_t> codepoints)
    : codepoints_(std::move(codepoints))
{
}

Charset Charset::fromUtf8(std::string_view text)
{
    return Charset(utf8Decode(text));
}

Charset Charset::fromUtf16Le(std::span<const std::uint8_t> bytes, bool skipBom)
{
    if (bytes.size() >= 2 && skipBom && bytes[0] == 0xFF && bytes[1] == 0xFE) {
        bytes = bytes.subspan(2);
    }
    if (bytes.size() >= 2 && bytes[0] == 0xFE && bytes[1] == 0xFF) {
        throw CharsetError("UTF-16BE charset files are not supported; save as UTF-16LE");
    }
    if (bytes.size() % 2 != 0) {
        throw CharsetError("UTF-16LE charset data has an odd number of bytes");
    }

    std::vector<char16_t> units;
    units.reserve(bytes.size() / 2);
    for (std::size_t index = 0; index < bytes.size(); index += 2) {
        units.push_back(static_cast<char16_t>(bytes[index]
            | (static_cast<std::uint16_t>(bytes[index + 1]) << 8)));
    }
    // `.cst` is a legacy code-unit stream, not a modern Unicode text file.
    // Keep one output slot per unit and replace surrogate units with U+FFFD;
    // UNICODE.cst intentionally contains the entire surrogate range.
    Charset result;
    result.codepoints_.reserve(units.size());
    result.notes_.reserve(units.size());
    for (const char16_t unit : units) {
        const auto value = static_cast<std::uint16_t>(unit);
        if (value >= 0xD800 && value <= 0xDFFF) {
            result.codepoints_.push_back(U'\uFFFD');
            constexpr char digits[] = "0123456789ABCDEF";
            std::string note = "from surrogate U+";
            note.push_back(digits[(value >> 12) & 0x0F]);
            note.push_back(digits[(value >> 8) & 0x0F]);
            note.push_back(digits[(value >> 4) & 0x0F]);
            note.push_back(digits[value & 0x0F]);
            result.notes_.push_back(std::move(note));
        } else {
            result.codepoints_.push_back(static_cast<char32_t>(value));
            result.notes_.push_back({});
        }
    }
    return result;
}

Charset Charset::loadCst(const std::filesystem::path& path)
{
    return fromUtf16Le(readBinaryFile(path));
}

Charset Charset::loadUtf8File(const std::filesystem::path& path)
{
    auto bytes = readBinaryFile(path);
    if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
        bytes.erase(bytes.begin(), bytes.begin() + 3);
    }
    return fromUtf8(std::string_view(
        reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

std::string Charset::toUtf8() const
{
    std::string result;
    result.reserve(codepoints_.size());
    for (const char32_t codepoint : codepoints_) {
        result += utf8Encode(codepoint);
    }
    return result;
}

Charset Charset::cleaned(const CharsetCleanup& cleanup) const
{
    std::vector<char32_t> points;
    std::vector<std::string> notes;
    points.reserve(codepoints_.size());
    notes.reserve(codepoints_.size());
    std::unordered_set<char32_t> seen;

    for (std::size_t index = 0; index < codepoints_.size(); ++index) {
        const char32_t codepoint = codepoints_[index];
        if (cleanup.removeControls && isControl(codepoint)) {
            continue;
        }
        if (cleanup.dedupe && !seen.insert(codepoint).second) {
            continue;
        }
        points.push_back(codepoint);
        notes.push_back(note(index));
    }

    Charset result(std::move(points));
    result.notes_ = std::move(notes);
    return result;
}

char32_t Charset::operator[](std::size_t index) const
{
    if (index >= codepoints_.size()) {
        throw std::out_of_range("charset index out of range");
    }
    return codepoints_[index];
}

const std::string& Charset::note(std::size_t index) const
{
    static const std::string empty;
    if (index >= notes_.size()) {
        return empty;
    }
    return notes_[index];
}

} // namespace fontmaker
