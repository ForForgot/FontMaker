#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fontmaker {

struct CharsetCleanup {
    bool removeControls = false;
    bool dedupe = false;
};

/// Ordered list of Unicode code points that defines the output glyph order.
///
/// The legacy `.cst` format is a headerless UTF-16LE stream. The order is
/// significant: it becomes the order of records in the generated C array or
/// binary font.
class Charset {
public:
    Charset() = default;
    explicit Charset(std::vector<char32_t> codepoints);

    [[nodiscard]] static Charset fromUtf8(std::string_view text);
    [[nodiscard]] static Charset fromUtf16Le(std::span<const std::uint8_t> bytes, bool skipBom = true);
    [[nodiscard]] static Charset loadCst(const std::filesystem::path& path);
    [[nodiscard]] static Charset loadUtf8File(const std::filesystem::path& path);

    [[nodiscard]] std::string toUtf8() const;

    /// Remove C0/C1 control characters and/or duplicate code points while
    /// preserving the original order.
    [[nodiscard]] Charset cleaned(const CharsetCleanup& cleanup) const;

    [[nodiscard]] const std::vector<char32_t>& codepoints() const noexcept { return codepoints_; }
    [[nodiscard]] bool empty() const noexcept { return codepoints_.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return codepoints_.size(); }
    [[nodiscard]] char32_t operator[](std::size_t index) const;

    /// Optional provenance note for a record, e.g. a surrogate source for a
    /// U+FFFD replacement. Empty when the code point came from normal text.
    [[nodiscard]] const std::string& note(std::size_t index) const;

private:
    std::vector<char32_t> codepoints_;
    std::vector<std::string> notes_;
};

} // namespace fontmaker
