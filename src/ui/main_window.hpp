#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include "fontmaker/charset.hpp"
#include "fontmaker/packing.hpp"

#include "gdi_rasterizer.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fontmaker::ui {

class MainWindow {
public:
    explicit MainWindow(HINSTANCE instance);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    int run();

private:
    static INT_PTR CALLBACK dialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool onInitDialog();

    void onCommand(int controlId, int notification);
    void onNotify(const NMHDR& header);
    void onDrawItem(const DRAWITEMSTRUCT& item);
    void onSave();
    void onFontChanged();
    void onMetricsChanged();
    void onOffsetChanged();
    void onCharacterIndexChanged(int delta);
    void onStandardCharset();
    void onCustomCharset();
    void onCharsetSelectionChanged();
    void onEditTable();

    void populateFonts();
    void populateCharsets();
    void loadSelectedCharset();
    void applyCustomCharset();
    void updateIndexLabel();
    void invalidatePreview();
    void setStatus(const std::wstring& text);
    void normalizeIntegerControl(int controlId, int minimum, int maximum, bool signedValue);

    [[nodiscard]] int integerControl(int controlId, int fallback, int minimum, int maximum, bool signedValue) const;
    [[nodiscard]] std::wstring selectedFontFamily() const;
    [[nodiscard]] int selectedStyleIndex() const;
    [[nodiscard]] bool checked(int controlId) const;

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    bool initializing_ = true;

    std::vector<std::wstring> fontFamilies_;
    std::wstring fontFamily_ = L"Segoe UI";
    int fontSize_ = 16;
    int fontWeight_ = FW_NORMAL;
    bool fontItalic_ = false;

    int glyphWidth_ = 16;
    int glyphHeight_ = 16;
    int offsetX_ = 0;
    int offsetY_ = 0;

    std::unique_ptr<win32::GdiGlyphRasterizer> rasterizer_;

    Charset charset_;
    std::string customText_ = "0";
    std::filesystem::path charsetDirectory_;
    std::vector<std::filesystem::path> charsetFiles_;
    int characterIndex_ = 0;
};

} // namespace fontmaker::ui
