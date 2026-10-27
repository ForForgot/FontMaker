#include "test_framework.hpp"

#include "fontmaker/error.hpp"
#include "fontmaker/writer.hpp"

#include <cstdint>
#include <string>
#include <vector>

using namespace fontmaker;

namespace {

EncodedFont sampleFont()
{
    EncodedFont font;
    font.width = 8;
    font.height = 2;
    font.recordSize = 2;
    font.options = {};
    font.glyphs = {
        {U'0', {0x81, 0x40}, {}},
        {U'\n', {0x00, 0x00}, {}},
    };
    return font;
}

} // namespace

FM_TEST(c_source_layout)
{
    auto font = sampleFont();
    font.glyphs.push_back({U'\'', {0x00, 0x00}, "from surrogate U+D800"});
    const auto source = toCSource(font);
    FM_CHECK(source.find("static const unsigned char bits[][2] =\n{") != std::string::npos);
    FM_CHECK(source.find("{ // U+0030 '0'") != std::string::npos);
    FM_CHECK(source.find("{ // U+000A\n") != std::string::npos);
    FM_CHECK(source.find("{ // U+0027 (from surrogate U+D800)\n") != std::string::npos);
    FM_CHECK(source.find("'''") == std::string::npos);
    FM_CHECK(source.find("0x81, 0x40,") != std::string::npos);
    FM_CHECK(source.find("0x00, 0x00,") != std::string::npos);
    FM_CHECK(source.find("0x40,\n    },") != std::string::npos);
    FM_CHECK(source.back() == '\n');
}

FM_TEST(c_source_quote_comment_is_clean)
{
    auto font = sampleFont();
    font.glyphs = {{U'\'', {0x00, 0x00}, {}}};
    const auto source = toCSource(font);
    FM_CHECK(source.find("{ // U+0027\n") != std::string::npos);
    FM_CHECK(source.find("'''") == std::string::npos);
}

FM_TEST(c_source_custom_array_name)
{
    WriterOptions options;
    options.arrayName = "glyphs";
    const auto source = toCSource(sampleFont(), options);
    FM_CHECK(source.find("static const unsigned char glyphs[][2]") != std::string::npos);
}

FM_TEST(c_source_rejects_invalid_identifier)
{
    WriterOptions options;
    options.arrayName = "not valid";
    bool threw = false;
    try {
        static_cast<void>(toCSource(sampleFont(), options));
    } catch (const EncodeError&) {
        threw = true;
    }
    FM_CHECK(threw);
}

FM_TEST(binary_concatenates_records)
{
    const auto bytes = toBinary(sampleFont());
    const std::vector<std::uint8_t> expected = {0x81, 0x40, 0x00, 0x00};
    FM_CHECK(bytes == expected);
}

FM_TEST(writer_rejects_empty_font)
{
    bool threw = false;
    try {
        static_cast<void>(toCSource(EncodedFont{}));
    } catch (const EncodeError&) {
        threw = true;
    }
    FM_CHECK(threw);
}

FM_TEST(c_source_rejects_oversized_output)
{
    EncodedFont font;
    font.width = 4096;
    font.height = 8;
    font.recordSize = 4096;
    font.glyphs.resize(3000, {U'A', std::vector<std::uint8_t>(4096, 0), {}});

    bool threw = false;
    try {
        static_cast<void>(toCSource(font));
    } catch (const EncodeError&) {
        threw = true;
    }
    FM_CHECK(threw);
}
