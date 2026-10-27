#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "fontmaker/glyph.hpp"

#include <windows.h>

#include <string>

namespace fontmaker::win32 {

struct FontDescriptor {
    std::wstring family = L"Segoe UI";
    int pixelHeight = 16;
    int weight = FW_NORMAL;
    bool italic = false;
};

/// GDI implementation of GlyphRasterizer.
///
/// The renderer owns a 32-bit top-down DIB and a memory DC. It keeps the
/// positive lfHeight convention of the original FontMaker so existing
/// 16x16 projects keep the same glyph placement.
class GdiGlyphRasterizer final : public GlyphRasterizer {
public:
    GdiGlyphRasterizer(FontDescriptor font, int width, int height, int offsetX, int offsetY);
    ~GdiGlyphRasterizer() override;

    GdiGlyphRasterizer(const GdiGlyphRasterizer&) = delete;
    GdiGlyphRasterizer& operator=(const GdiGlyphRasterizer&) = delete;

    [[nodiscard]] Glyph rasterize(char32_t codepoint) override;

    void setFont(const FontDescriptor& font);
    void setMetrics(int width, int height, int offsetX, int offsetY);

    /// Render and stretch the current glyph onto a target DC. Used by the
    /// preview window; export always goes through rasterize().
    void draw(HDC target, int x, int y, int width, int height, char32_t codepoint);

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }

    /// Face name that GDI actually selected. May differ from the requested
    /// family when the font is missing (GDI silently substitutes).
    [[nodiscard]] const std::wstring& actualFamily() const noexcept { return actualFamily_; }
    [[nodiscard]] bool substituted() const;

private:
    void ensureFont();
    void ensureBitmap(int width, int height);
    void release();

    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    HFONT font_ = nullptr;
    HGDIOBJ oldFont_ = nullptr;
    void* pixels_ = nullptr;
    int stride_ = 0;
    int width_ = 0;
    int height_ = 0;
    int dibWidth_ = 0;
    int dibHeight_ = 0;
    int offsetX_ = 0;
    int offsetY_ = 0;
    std::wstring actualFamily_;
    FontDescriptor fontDescriptor_;
};

} // namespace fontmaker::win32
