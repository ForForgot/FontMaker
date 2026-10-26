#include "test_framework.hpp"

#include "fontmaker/error.hpp"
#include "fontmaker/utf.hpp"

#include <cstdint>
#include <string>

using namespace fontmaker;

FM_TEST(utf8_round_trip)
{
    for (const char32_t codepoint : {U'A', U'0', U'\u00E9', U'\u4E2D', U'\U0001F600'}) {
        const std::string encoded = utf8Encode(codepoint);
        const auto decoded = utf8Decode(encoded);
        FM_CHECK_EQ(decoded.size(), std::size_t{1});
        FM_CHECK_EQ(decoded.front(), codepoint);
    }
}

FM_TEST(utf16_surrogate_pair)
{
    const auto units = utf16Encode(U'\U0001F600');
    FM_CHECK_EQ(units.size(), std::size_t{2});
    const auto decoded = utf16Decode(units);
    FM_CHECK_EQ(decoded.size(), std::size_t{1});
    FM_CHECK_EQ(decoded.front(), U'\U0001F600');
}

FM_TEST(utf16_rejects_unpaired_surrogate_by_default)
{
    const std::u16string units = {static_cast<char16_t>(0xD800), u'A'};
    bool threw = false;
    try {
        static_cast<void>(utf16Decode(units));
    } catch (const CharsetError&) {
        threw = true;
    }
    FM_CHECK(threw);

    const auto replaced = utf16Decode(units, true);
    FM_CHECK_EQ(replaced.size(), std::size_t{2});
    FM_CHECK_EQ(replaced[0], U'\uFFFD');
    FM_CHECK_EQ(replaced[1], U'A');
}

FM_TEST(utf8_rejects_overlong_and_invalid)
{
    bool threw = false;
    try {
        static_cast<void>(utf8Decode(std::string("\xC0\xAF", 2)));
    } catch (const CharsetError&) {
        threw = true;
    }
    FM_CHECK(threw);

    threw = false;
    try {
        static_cast<void>(utf8Decode(std::string("\xED\xA0\x80", 3)));
    } catch (const CharsetError&) {
        threw = true;
    }
    FM_CHECK(threw);
}

FM_TEST(utf_comment_printability)
{
    FM_CHECK(!isCommentPrintable(U'\n'));
    FM_CHECK(!isCommentPrintable(U'\r'));
    FM_CHECK(!isCommentPrintable(U'\0'));
    FM_CHECK(!isCommentPrintable(U'\u2028'));
    FM_CHECK(isCommentPrintable(U' '));
    FM_CHECK(isCommentPrintable(U'中'));
}
