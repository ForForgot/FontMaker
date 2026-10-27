#include "gdi_rasterizer.hpp"

#include "fontmaker/error.hpp"
#include "fontmaker/packing.hpp"
#include "fontmaker/utf.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace fontmaker::win32 {
namespace {

[[noreturn]] void throwGdiError(const char* operation)
{
    throw Error(std::string(operation) + " failed (GDI error "
        + std::to_string(static_cast<unsigned long>(GetLastError())) + ")");
}

std::wstring toWideText(char32_t codepoint)
{
    const std::u16string units = utf16Encode(codepoint);
    std::wstring text;
    text.reserve(units.size());
    for (const char16_t unit : units) {
        text.push_back(static_cast<wchar_t>(unit));
    }
    return text;
}

} // namespace

GdiGlyphRasterizer::GdiGlyphRasterizer(
    FontDescriptor font, int width, int height, int offsetX, int offsetY)
    : fontDescriptor_(std::move(font))
{
    try {
        dc_ = CreateCompatibleDC(nullptr);
        if (dc_ == nullptr) {
            throwGdiError("CreateCompatibleDC");
        }

        SetBkMode(dc_, TRANSPARENT);
        SetTextColor(dc_, RGB(255, 255, 255));
        SetTextAlign(dc_, TA_LEFT | TA_TOP);
        SetMapMode(dc_, MM_TEXT);

        ensureFont();
        setMetrics(width, height, offsetX, offsetY);
    } catch (...) {
        release();
        throw;
    }
}

GdiGlyphRasterizer::~GdiGlyphRasterizer()
{
    release();
}

void GdiGlyphRasterizer::release()
{
    if (dc_ != nullptr) {
        if (font_ != nullptr) {
            SelectObject(dc_, oldFont_);
            DeleteObject(font_);
            font_ = nullptr;
            oldFont_ = nullptr;
        }
        if (bitmap_ != nullptr) {
            SelectObject(dc_, oldBitmap_);
            DeleteObject(bitmap_);
            bitmap_ = nullptr;
            oldBitmap_ = nullptr;
        }
        DeleteDC(dc_);
        dc_ = nullptr;
    }
    pixels_ = nullptr;
    stride_ = 0;
}

void GdiGlyphRasterizer::ensureFont()
{
    if (fontDescriptor_.pixelHeight < kMinFontPixelHeight
        || fontDescriptor_.pixelHeight > kMaxFontPixelHeight) {
        throw Error("font pixel height is outside the supported range");
    }

    LOGFONTW logFont{};
    logFont.lfHeight = fontDescriptor_.pixelHeight;
    logFont.lfWeight = fontDescriptor_.weight;
    logFont.lfItalic = fontDescriptor_.italic ? TRUE : FALSE;
    logFont.lfCharSet = DEFAULT_CHARSET;
    logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
    logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    logFont.lfQuality = NONANTIALIASED_QUALITY;
    logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    const std::wstring family = fontDescriptor_.family.empty() ? L"Segoe UI" : fontDescriptor_.family;
    wcsncpy_s(logFont.lfFaceName, family.c_str(), _TRUNCATE);

    HFONT newFont = CreateFontIndirectW(&logFont);
    if (newFont == nullptr) {
        throwGdiError("CreateFontIndirectW");
    }

    HGDIOBJ previous = SelectObject(dc_, newFont);
    if (oldFont_ == nullptr) {
        oldFont_ = previous;
    }
    if (font_ != nullptr) {
        DeleteObject(font_);
    }
    font_ = newFont;

    wchar_t actualFace[LF_FACESIZE]{};
    if (GetTextFaceW(dc_, LF_FACESIZE, actualFace) > 0) {
        actualFamily_ = actualFace;
    } else {
        actualFamily_.clear();
    }
}

bool GdiGlyphRasterizer::substituted() const noexcept
{
    if (actualFamily_.empty() || fontDescriptor_.family.empty()) {
        return false;
    }
    return CompareStringOrdinal(
               fontDescriptor_.family.c_str(), static_cast<int>(fontDescriptor_.family.size()),
               actualFamily_.c_str(), static_cast<int>(actualFamily_.size()), TRUE)
        != CSTR_EQUAL;
}

void GdiGlyphRasterizer::ensureBitmap(int width, int height)
{
    if (width < kMinGlyphDimension || height < kMinGlyphDimension
        || width > kMaxGlyphDimension || height > kMaxGlyphDimension) {
        throw Error("glyph cell size is outside the supported range");
    }

    if (bitmap_ != nullptr && width == width_ && height == height_) {
        return;
    }

    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; // top-down: row 0 is the top row
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* pixels = nullptr;
    HBITMAP newBitmap = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (newBitmap == nullptr || pixels == nullptr) {
        if (newBitmap != nullptr) {
            DeleteObject(newBitmap);
        }
        throwGdiError("CreateDIBSection");
    }

    HGDIOBJ previous = SelectObject(dc_, newBitmap);
    if (oldBitmap_ == nullptr) {
        oldBitmap_ = previous;
    }
    if (bitmap_ != nullptr) {
        DeleteObject(bitmap_);
    }

    bitmap_ = newBitmap;
    pixels_ = pixels;
    stride_ = width * 4;
    width_ = width;
    height_ = height;
    std::memset(pixels_, 0, static_cast<std::size_t>(stride_) * static_cast<std::size_t>(height_));
}

void GdiGlyphRasterizer::setFont(const FontDescriptor& font)
{
    if (font.family == fontDescriptor_.family
        && font.pixelHeight == fontDescriptor_.pixelHeight
        && font.weight == fontDescriptor_.weight
        && font.italic == fontDescriptor_.italic) {
        return;
    }
    fontDescriptor_ = font;
    ensureFont();
}

void GdiGlyphRasterizer::setMetrics(int width, int height, int offsetX, int offsetY)
{
    if (offsetX < -kMaxGlyphOffset || offsetX > kMaxGlyphOffset
        || offsetY < -kMaxGlyphOffset || offsetY > kMaxGlyphOffset) {
        throw Error("glyph offset is outside the supported range");
    }
    ensureBitmap(width, height);
    offsetX_ = offsetX;
    offsetY_ = offsetY;
}

Glyph GdiGlyphRasterizer::rasterize(char32_t codepoint)
{
    const std::wstring text = toWideText(codepoint);
    if (text.empty()) {
        throw Error("cannot rasterize an empty code point");
    }

    if (!PatBlt(dc_, 0, 0, width_, height_, BLACKNESS)) {
        throwGdiError("PatBlt");
    }

    if (!TextOutW(dc_, offsetX_, offsetY_, text.c_str(), static_cast<int>(text.size()))) {
        throwGdiError("TextOutW");
    }

    SIZE extent{};
    if (!GetTextExtentPoint32W(dc_, text.c_str(), static_cast<int>(text.size()), &extent)) {
        throwGdiError("GetTextExtentPoint32W");
    }

    Glyph glyph;
    glyph.width = width_;
    glyph.height = height_;
    glyph.advance = extent.cx;
    glyph.coverage.resize(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_));

    const auto* base = static_cast<const std::uint8_t*>(pixels_);
    for (int y = 0; y < height_; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(base + static_cast<std::size_t>(y) * stride_);
        for (int x = 0; x < width_; ++x) {
            const std::uint32_t pixel = row[x];
            const auto blue = static_cast<std::uint8_t>(pixel & 0xFF);
            const auto green = static_cast<std::uint8_t>((pixel >> 8) & 0xFF);
            const auto red = static_cast<std::uint8_t>((pixel >> 16) & 0xFF);
            glyph.coverage[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_)
                + static_cast<std::size_t>(x)] = std::max({red, green, blue});
        }
    }

    return glyph;
}

void GdiGlyphRasterizer::draw(
    HDC target, int x, int y, int width, int height, char32_t codepoint)
{
    static_cast<void>(rasterize(codepoint));
    SetStretchBltMode(target, COLORONCOLOR);
    StretchBlt(target, x, y, width, height, dc_, 0, 0, width_, height_, SRCCOPY);
}

} // namespace fontmaker::win32
