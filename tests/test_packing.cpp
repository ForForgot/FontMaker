#include "test_framework.hpp"

#include "fontmaker/charset.hpp"
#include "fontmaker/error.hpp"
#include "fontmaker/packing.hpp"

#include <cstdint>
#include <vector>

using namespace fontmaker;

namespace {

Glyph makeGlyph(int width, int height, int advance, std::vector<std::pair<int, int>> pixels)
{
    Glyph glyph;
    glyph.width = width;
    glyph.height = height;
    glyph.advance = advance;
    glyph.coverage.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
    for (const auto& [x, y] : pixels) {
        glyph.coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
            + static_cast<std::size_t>(x)] = 0xFF;
    }
    return glyph;
}

class FakeRasterizer final : public GlyphRasterizer {
public:
    explicit FakeRasterizer(Glyph glyph) : glyph_(std::move(glyph)) {}

    Glyph rasterize(char32_t codepoint) override
    {
        Glyph result = glyph_;
        result.advance = static_cast<int>(codepoint % 17);
        return result;
    }

private:
    Glyph glyph_;
};

} // namespace

FM_TEST(record_size_formulas)
{
    FM_CHECK_EQ(recordSizeBytes(16, 16, {}), std::size_t{32});
    FM_CHECK_EQ(recordSizeBytes(16, 16, {.variableWidth = true}), std::size_t{34});
    FM_CHECK_EQ(recordSizeBytes(10, 3, {.scan = ScanDirection::Vertical}), std::size_t{10});
}

FM_TEST(horizontal_msb_and_lsb)
{
    const Glyph glyph = makeGlyph(8, 2, 5, {{0, 0}, {7, 0}, {1, 1}});

    const auto msb = packGlyph(glyph, {.scan = ScanDirection::Horizontal, .bitOrder = BitOrder::MsbFirst});
    FM_CHECK_EQ(msb.size(), std::size_t{2});
    FM_CHECK_EQ(msb[0], std::uint8_t{0x81});
    FM_CHECK_EQ(msb[1], std::uint8_t{0x40});

    const auto lsb = packGlyph(glyph, {.scan = ScanDirection::Horizontal, .bitOrder = BitOrder::LsbFirst});
    FM_CHECK_EQ(lsb[0], std::uint8_t{0x81});
    FM_CHECK_EQ(lsb[1], std::uint8_t{0x02});
}

FM_TEST(horizontal_padding_is_zero)
{
    const Glyph glyph = makeGlyph(10, 2, 10, {{9, 0}});
    const auto bytes = packGlyph(glyph, {.scan = ScanDirection::Horizontal, .bitOrder = BitOrder::MsbFirst});
    FM_CHECK_EQ(bytes.size(), std::size_t{4});
    FM_CHECK_EQ(bytes[0], std::uint8_t{0x00});
    FM_CHECK_EQ(bytes[1], std::uint8_t{0x40});
    FM_CHECK_EQ(bytes[2], std::uint8_t{0x00});
    FM_CHECK_EQ(bytes[3], std::uint8_t{0x00});
}

FM_TEST(vertical_ordering)
{
    const Glyph glyph = makeGlyph(2, 10, 2, {{0, 9}, {1, 0}});
    const auto bytes = packGlyph(glyph, {.scan = ScanDirection::Vertical, .bitOrder = BitOrder::MsbFirst});
    FM_CHECK_EQ(bytes.size(), std::size_t{4});
    FM_CHECK_EQ(bytes[0], std::uint8_t{0x00});
    FM_CHECK_EQ(bytes[1], std::uint8_t{0x40});
    FM_CHECK_EQ(bytes[2], std::uint8_t{0x80});
    FM_CHECK_EQ(bytes[3], std::uint8_t{0x00});
}

FM_TEST(variable_width_prefix_is_big_endian)
{
    const Glyph glyph = makeGlyph(8, 1, 300, {{0, 0}});
    const auto bytes = packGlyph(glyph, {
        .scan = ScanDirection::Horizontal,
        .bitOrder = BitOrder::MsbFirst,
        .variableWidth = true,
    });
    FM_CHECK_EQ(bytes.size(), std::size_t{3});
    FM_CHECK_EQ(bytes[0], std::uint8_t{0x01});
    FM_CHECK_EQ(bytes[1], std::uint8_t{0x2C});
    FM_CHECK_EQ(bytes[2], std::uint8_t{0x80});
}

FM_TEST(encode_font_reports_progress)
{
    const Glyph glyph = makeGlyph(8, 8, 8, {{3, 3}});
    FakeRasterizer rasterizer(glyph);
    const Charset charset = Charset::fromUtf8("AB");

    std::vector<std::size_t> progress;
    const auto font = encodeFont(rasterizer, charset, {}, [&](std::size_t done, std::size_t total) {
        FM_CHECK_EQ(total, std::size_t{2});
        progress.push_back(done);
    });

    FM_CHECK_EQ(font.glyphs.size(), std::size_t{2});
    FM_CHECK_EQ(font.width, 8);
    FM_CHECK_EQ(font.height, 8);
    FM_CHECK_EQ(font.recordSize, 8);
    FM_CHECK_EQ(progress.size(), std::size_t{2});
    FM_CHECK_EQ(progress[0], std::size_t{1});
    FM_CHECK_EQ(progress[1], std::size_t{2});
}

FM_TEST(encode_rejects_empty_charset)
{
    FakeRasterizer rasterizer(makeGlyph(8, 8, 8, {}));
    bool threw = false;
    try {
        static_cast<void>(encodeFont(rasterizer, Charset{}, {}));
    } catch (const EncodeError&) {
        threw = true;
    }
    FM_CHECK(threw);
}

FM_TEST(encode_counts_empty_and_edge_glyphs)
{
    FakeRasterizer emptyRasterizer(makeGlyph(8, 8, 8, {}));
    const auto emptyFont = encodeFont(emptyRasterizer, Charset::fromUtf8("A"), {});
    FM_CHECK_EQ(emptyFont.stats.emptyGlyphs, std::size_t{1});
    FM_CHECK_EQ(emptyFont.stats.edgeGlyphs, std::size_t{0});

    FakeRasterizer edgeRasterizer(makeGlyph(8, 8, 8, {{0, 0}}));
    const auto edgeFont = encodeFont(edgeRasterizer, Charset::fromUtf8("A"), {});
    FM_CHECK_EQ(edgeFont.stats.emptyGlyphs, std::size_t{0});
    FM_CHECK_EQ(edgeFont.stats.edgeGlyphs, std::size_t{1});
}

class ThrowingRasterizer final : public GlyphRasterizer {
public:
    Glyph rasterize(char32_t) override
    {
        throw EncodeError("renderer failure");
    }
};

FM_TEST(encode_propagates_rasterizer_exceptions)
{
    ThrowingRasterizer rasterizer;
    bool threw = false;
    try {
        static_cast<void>(encodeFont(rasterizer, Charset::fromUtf8("A"), {}));
    } catch (const EncodeError&) {
        threw = true;
    }
    FM_CHECK(threw);
}
