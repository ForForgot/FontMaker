#include "main_window.hpp"

#include "about_dialog.hpp"
#include "charset_editor.hpp"
#include "font_catalog.hpp"
#include "resource.h"
#include "win32_utils.hpp"

#include "fontmaker/error.hpp"
#include "fontmaker/io.hpp"
#include "fontmaker/metadata.hpp"
#include "fontmaker/sha256.hpp"
#include "fontmaker/utf.hpp"
#include "fontmaker/version.hpp"
#include "fontmaker/writer.hpp"

#include <commctrl.h>

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

namespace fontmaker::ui {
namespace {

constexpr int kMinFontSize = kMinFontPixelHeight;
constexpr int kMaxFontSize = kMaxFontPixelHeight;
constexpr int kMinCellSize = kMinGlyphDimension;
constexpr int kMaxCellSize = kMaxGlyphDimension;
constexpr int kMinOffset = -kMaxGlyphOffset;
constexpr int kMaxOffset = kMaxGlyphOffset;

std::wstring fileNameOf(const std::filesystem::path& path)
{
    return path.filename().wstring();
}

} // namespace

MainWindow::MainWindow(HINSTANCE instance)
    : instance_(instance)
{
}

MainWindow::~MainWindow() = default;

int MainWindow::run()
{
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_WIN95_CLASSES | ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&controls);

    const INT_PTR result = DialogBoxParamW(
        instance_, MAKEINTRESOURCEW(IDD_MAIN), nullptr, dialogProc,
        reinterpret_cast<LPARAM>(this));
    if (result == -1) {
        const DWORD error = GetLastError();
        win32::showError(nullptr, L"启动失败",
            L"无法创建主窗口。\n" + win32::formatSystemError(error));
        return 1;
    }
    return static_cast<int>(result);
}

INT_PTR CALLBACK MainWindow::dialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG) {
        auto* self = reinterpret_cast<MainWindow*>(lParam);
        self->window_ = window;
        SetWindowLongPtrW(window, DWLP_USER, reinterpret_cast<LONG_PTR>(self));
        try {
            return self->onInitDialog() ? TRUE : FALSE;
        } catch (const std::exception& error) {
            win32::showError(window, L"初始化失败", win32::widen(error.what()));
            EndDialog(window, IDCANCEL);
            return FALSE;
        }
    }

    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, DWLP_USER));
    if (self == nullptr) {
        return FALSE;
    }

    try {
        return self->handleMessage(message, wParam, lParam);
    } catch (const std::exception& error) {
        win32::showError(window, L"操作失败", win32::widen(error.what()));
        return TRUE;
    }
}

bool MainWindow::onInitDialog()
{
    initializing_ = true;
    SetWindowTextW(window_, L"FontMaker " FONTMAKER_VERSION_W L" - 点阵字库生成工具");

    settings_ = loadSettings();
    fontSize_ = std::clamp(settings_.fontSize, kMinFontSize, kMaxFontSize);
    glyphWidth_ = std::clamp(settings_.width, kMinCellSize, kMaxCellSize);
    glyphHeight_ = std::clamp(settings_.height, kMinCellSize, kMaxCellSize);
    offsetX_ = std::clamp(settings_.offsetX, kMinOffset, kMaxOffset);
    offsetY_ = std::clamp(settings_.offsetY, kMinOffset, kMaxOffset);
    const int style = std::clamp(settings_.style, 0, 3);
    fontWeight_ = (style == 1 || style == 3) ? FW_BOLD : FW_NORMAL;
    fontItalic_ = (style == 2 || style == 3);

    const HICON icon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_APP));
    if (icon != nullptr) {
        SendMessageW(window_, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon));
        SendMessageW(window_, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
    }

    SendDlgItemMessageW(window_, IDC_FONT_STYLE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"常规"));
    SendDlgItemMessageW(window_, IDC_FONT_STYLE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"加粗"));
    SendDlgItemMessageW(window_, IDC_FONT_STYLE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"倾斜"));
    SendDlgItemMessageW(window_, IDC_FONT_STYLE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"加粗倾斜"));
    SendDlgItemMessageW(window_, IDC_FONT_STYLE, CB_SETCURSEL,
        static_cast<WPARAM>(style), 0);

    SendDlgItemMessageW(window_, IDC_SPIN_SIZE, UDM_SETRANGE32, kMinFontSize, kMaxFontSize);
    SendDlgItemMessageW(window_, IDC_SPIN_WIDTH, UDM_SETRANGE32, kMinCellSize, kMaxCellSize);
    SendDlgItemMessageW(window_, IDC_SPIN_HEIGHT, UDM_SETRANGE32, kMinCellSize, kMaxCellSize);
    SendDlgItemMessageW(window_, IDC_SPIN_OFFSET_X, UDM_SETRANGE32,
        static_cast<WPARAM>(static_cast<INT_PTR>(kMinOffset)),
        static_cast<LPARAM>(kMaxOffset));
    SendDlgItemMessageW(window_, IDC_SPIN_OFFSET_Y, UDM_SETRANGE32,
        static_cast<WPARAM>(static_cast<INT_PTR>(kMinOffset)),
        static_cast<LPARAM>(kMaxOffset));
    SendDlgItemMessageW(window_, IDC_SPIN_INDEX, UDM_SETRANGE32, 0, 0);

    SetDlgItemInt(window_, IDC_FONT_SIZE, fontSize_, FALSE);
    SetDlgItemInt(window_, IDC_WIDTH, glyphWidth_, FALSE);
    SetDlgItemInt(window_, IDC_HEIGHT, glyphHeight_, FALSE);
    SetDlgItemInt(window_, IDC_OFFSET_X, static_cast<UINT>(offsetX_), TRUE);
    SetDlgItemInt(window_, IDC_OFFSET_Y, static_cast<UINT>(offsetY_), TRUE);

    CheckRadioButton(window_, IDC_SCAN_H, IDC_SCAN_V,
        settings_.scanVertical ? IDC_SCAN_V : IDC_SCAN_H);
    CheckRadioButton(window_, IDC_MSB, IDC_LSB,
        settings_.lsbFirst ? IDC_LSB : IDC_MSB);
    CheckRadioButton(window_, IDC_FIXED, IDC_VARIABLE,
        settings_.variableWidth ? IDC_VARIABLE : IDC_FIXED);
    CheckRadioButton(window_, IDC_FORMAT_C, IDC_FORMAT_BIN,
        settings_.binaryOutput ? IDC_FORMAT_BIN : IDC_FORMAT_C);
    CheckDlgButton(window_, IDC_METADATA,
        settings_.metadata ? BST_CHECKED : BST_UNCHECKED);

    populateFonts();

    setStatus(L"就绪");

    rasterizer_ = std::make_unique<win32::GdiGlyphRasterizer>(
        win32::FontDescriptor{fontFamily_, fontSize_, fontWeight_, fontItalic_},
        glyphWidth_, glyphHeight_, offsetX_, offsetY_);
    updateFontWarning();
    updateMetricsWarning();

    populateCharsets();

    initializing_ = false;
    updateIndexLabel();
    invalidatePreview();
    win32::centerWindow(window_);
    SetFocus(GetDlgItem(window_, IDC_FONT_LIST));
    return true;
}

INT_PTR MainWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_COMMAND:
        onCommand(LOWORD(wParam), HIWORD(wParam));
        return TRUE;
    case WM_NOTIFY:
        onNotify(*reinterpret_cast<const NMHDR*>(lParam));
        return TRUE;
    case WM_DRAWITEM:
        onDrawItem(*reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_HELP:
        showAboutDialog(window_, instance_);
        return TRUE;
    case WM_CLOSE:
        saveUiSettings();
        EndDialog(window_, IDCANCEL);
        return TRUE;
    default:
        return FALSE;
    }
}

void MainWindow::onCommand(int controlId, int notification)
{
    if (controlId == IDCANCEL) {
        saveUiSettings();
        EndDialog(window_, IDCANCEL);
        return;
    }
    if (controlId == IDOK) {
        onSave();
        return;
    }
    if (controlId == IDHELP) {
        showAboutDialog(window_, instance_);
        return;
    }

    switch (controlId) {
    case IDC_SAVE:
        if (notification == BN_CLICKED) {
            onSave();
        }
        break;
    case IDC_STD:
        if (notification == BN_CLICKED) {
            onStandardCharset();
        }
        break;
    case IDC_USER:
        if (notification == BN_CLICKED) {
            onCustomCharset();
        }
        break;
    case IDC_EDIT_TABLE:
        if (notification == BN_CLICKED) {
            onEditTable();
        }
        break;
    case IDC_ABOUT:
        if (notification == BN_CLICKED) {
            showAboutDialog(window_, instance_);
        }
        break;
    case IDC_FONT_LIST:
    case IDC_FONT_STYLE:
        if (notification == CBN_SELCHANGE) {
            onFontChanged();
        }
        break;
    case IDC_CHARSET_LIST:
        if (notification == CBN_SELCHANGE) {
            onCharsetSelectionChanged();
        }
        break;
    case IDC_FONT_SIZE:
        if (notification == EN_CHANGE && !initializing_) {
            onFontChanged();
        } else if (notification == EN_KILLFOCUS) {
            normalizeIntegerControl(IDC_FONT_SIZE, kMinFontSize, kMaxFontSize, false);
            onFontChanged();
        }
        break;
    case IDC_WIDTH:
    case IDC_HEIGHT:
        if (notification == EN_CHANGE && !initializing_) {
            onMetricsChanged();
        } else if (notification == EN_KILLFOCUS) {
            normalizeIntegerControl(controlId, kMinCellSize, kMaxCellSize, false);
            onMetricsChanged();
        }
        break;
    case IDC_OFFSET_X:
    case IDC_OFFSET_Y:
        if (notification == EN_CHANGE && !initializing_) {
            onOffsetChanged();
        } else if (notification == EN_KILLFOCUS) {
            normalizeIntegerControl(controlId, kMinOffset, kMaxOffset, true);
            onOffsetChanged();
        }
        break;
    default:
        break;
    }
}

void MainWindow::onNotify(const NMHDR& header)
{
    if (header.idFrom == IDC_SPIN_INDEX && header.code == UDN_DELTAPOS) {
        const auto* updown = reinterpret_cast<const NMUPDOWN*>(&header);
        onCharacterIndexChanged(updown->iDelta);
    }
}

void MainWindow::onDrawItem(const DRAWITEMSTRUCT& item)
{
    if (item.CtlID != IDC_PREVIEW || item.hDC == nullptr) {
        return;
    }

    HDC dc = item.hDC;
    RECT rect = item.rcItem;
    const int clientWidth = rect.right - rect.left;
    const int clientHeight = rect.bottom - rect.top;
    if (clientWidth <= 0 || clientHeight <= 0) {
        return;
    }

    const HBRUSH background = CreateSolidBrush(RGB(24, 26, 30));
    FillRect(dc, &rect, background);
    DeleteObject(background);

    SetBkMode(dc, TRANSPARENT);
    if (charset_.empty() || characterIndex_ < 0
        || static_cast<std::size_t>(characterIndex_) >= charset_.size()) {
        SetTextColor(dc, RGB(150, 152, 158));
        DrawTextW(dc, L"无字符", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        FrameRect(dc, &rect, GetSysColorBrush(COLOR_BTNSHADOW));
        return;
    }

    const int availableWidth = (std::max)(1, clientWidth - 20);
    const int availableHeight = (std::max)(1, clientHeight - 20);
    int scale = (std::min)(availableWidth / glyphWidth_, availableHeight / glyphHeight_);
    scale = std::clamp(scale, 1, 16);

    const int drawWidth = glyphWidth_ * scale;
    const int drawHeight = glyphHeight_ * scale;
    const int left = rect.left + (clientWidth - drawWidth) / 2;
    const int top = rect.top + (clientHeight - drawHeight) / 2;

    try {
        rasterizer_->draw(dc, left, top, drawWidth, drawHeight, charset_[static_cast<std::size_t>(characterIndex_)]);
    } catch (...) {
        SetTextColor(dc, RGB(220, 120, 120));
        DrawTextW(dc, L"渲染失败", -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        FrameRect(dc, &rect, GetSysColorBrush(COLOR_BTNSHADOW));
        return;
    }

    if (scale >= 4) {
        HPEN gridPen = CreatePen(PS_SOLID, 1, RGB(48, 51, 58));
        HGDIOBJ oldPen = SelectObject(dc, gridPen);
        for (int x = 1; x < glyphWidth_; ++x) {
            const int lineX = left + x * scale;
            MoveToEx(dc, lineX, top, nullptr);
            LineTo(dc, lineX, top + drawHeight);
        }
        for (int y = 1; y < glyphHeight_; ++y) {
            const int lineY = top + y * scale;
            MoveToEx(dc, left, lineY, nullptr);
            LineTo(dc, left + drawWidth, lineY);
        }
        SelectObject(dc, oldPen);
        DeleteObject(gridPen);

        std::wstring zoom = L"x" + std::to_wstring(scale);
        SetTextColor(dc, RGB(150, 152, 158));
        RECT zoomRect = rect;
        zoomRect.left += 8;
        zoomRect.top += 6;
        DrawTextW(dc, zoom.c_str(), -1, &zoomRect, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }

    RECT frame = {left - 1, top - 1, left + drawWidth + 1, top + drawHeight + 1};
    FrameRect(dc, &frame, GetSysColorBrush(COLOR_BTNSHADOW));
}

void MainWindow::onSave()
{
    if (rasterizer_ == nullptr) {
        return;
    }
    if (charset_.empty()) {
        win32::showError(window_, L"无法保存", L"当前字符集为空，请先选择或编辑码表。");
        return;
    }

    const bool binary = checked(IDC_FORMAT_BIN);
    const wchar_t* filter = binary
        ? L"二进制字库 (*.bin)\0*.bin\0所有文件 (*.*)\0*.*\0\0"
        : L"C 源文件 (*.c)\0*.c\0所有文件 (*.*)\0*.*\0\0";
    const auto path = win32::saveFileDialog(
        window_, filter, L"保存字库", binary ? L"bin" : L"c");
    if (!path) {
        return;
    }

    PackOptions packing;
    packing.scan = checked(IDC_SCAN_V) ? ScanDirection::Vertical : ScanDirection::Horizontal;
    packing.bitOrder = checked(IDC_LSB) ? BitOrder::LsbFirst : BitOrder::MsbFirst;
    packing.variableWidth = checked(IDC_VARIABLE);

    try {
        win32::WaitCursor waitCursor;
        setStatus(L"正在生成字库...");
        UpdateWindow(window_);

        const auto encoded = encodeFont(
            *rasterizer_,
            charset_,
            packing,
            [&](std::size_t done, std::size_t total) {
                if (done == total || done % 64 == 0) {
                    setStatus(L"正在生成... " + std::to_wstring(done) + L"/"
                        + std::to_wstring(total));
                    UpdateWindow(window_);
                }
            });

        if (encoded.stats.emptyGlyphs == encoded.glyphs.size()) {
            setStatus(L"所有字形为空，未保存");
            win32::showError(window_, L"未保存",
                L"所有字形都是空白的。请检查字体、字号、偏移和点阵尺寸。");
            return;
        }

        saveFont(*path, encoded, binary ? OutputFormat::Binary : OutputFormat::CSource);
        std::filesystem::path metadataPath;
        if (checked(IDC_METADATA)) {
            metadataPath = path->parent_path() / (path->stem().wstring() + L".json");
            const auto binaryPayload = toBinary(encoded);
            FontMetadata metadata;
            metadata.generatorVersion = FONTMAKER_VERSION_A;
            metadata.requestedFont = win32::narrow(fontFamily_);
            metadata.actualFont = win32::narrow(rasterizer_->actualFamily());
            metadata.fontPixelHeight = fontSize_;
            metadata.cellWidth = glyphWidth_;
            metadata.cellHeight = glyphHeight_;
            metadata.offsetX = offsetX_;
            metadata.offsetY = offsetY_;
            metadata.packing = packing;
            metadata.glyphCount = encoded.glyphs.size();
            metadata.emptyGlyphs = encoded.stats.emptyGlyphs;
            metadata.edgeGlyphs = encoded.stats.edgeGlyphs;
            metadata.recordSize = encoded.recordSize;
            if (checked(IDC_STD)) {
                const LRESULT selection = SendDlgItemMessageW(
                    window_, IDC_CHARSET_LIST, CB_GETCURSEL, 0, 0);
                if (selection != CB_ERR && static_cast<std::size_t>(selection) < charsetFiles_.size()) {
                    metadata.charsetSource = pathToUtf8(
                        charsetFiles_[static_cast<std::size_t>(selection)]);
                }
            } else {
                metadata.charsetSource = "user-defined";
            }
            metadata.outputFormat = binary ? "bin" : "c";
            metadata.payloadSha256 = sha256Hex(binaryPayload);
            const std::string json = toJson(metadata);
            writeBinaryFile(metadataPath, std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(json.data()), json.size()));
        }

        setStatus(L"已保存 " + fileNameOf(*path) + L"（"
            + std::to_wstring(encoded.glyphs.size()) + L" 个字符）");

        std::wstring message = L"字库已保存：\n" + path->wstring()
            + L"\n\n字符数：" + std::to_wstring(encoded.glyphs.size())
            + L"\n每字符：" + std::to_wstring(encoded.recordSize) + L" 字节";
        if (!metadataPath.empty()) {
            message += L"\n元数据：" + metadataPath.wstring();
        }
        if (encoded.stats.emptyGlyphs > 0) {
            message += L"\n空字形：" + std::to_wstring(encoded.stats.emptyGlyphs);
        }
        if (encoded.stats.edgeGlyphs > 0 && fontSize_ > glyphHeight_) {
            message += L"\n贴边字形：" + std::to_wstring(encoded.stats.edgeGlyphs)
                + L"（可能被裁剪）";
        }
        if (!fontWarning_.empty()) {
            message += L"\n\n注意：" + fontWarning_;
        }
        win32::showInfo(window_, L"完成", message);
    } catch (const std::exception& error) {
        setStatus(L"保存失败");
        win32::showError(window_, L"保存失败", win32::widen(error.what()));
    }
}

void MainWindow::onFontChanged()
{
    if (rasterizer_ == nullptr || initializing_) {
        return;
    }

    const std::wstring family = selectedFontFamily();
    if (family.empty()) {
        return;
    }

    const int size = integerControl(IDC_FONT_SIZE, fontSize_, kMinFontSize, kMaxFontSize, false);
    const int style = selectedStyleIndex();
    int weight = FW_NORMAL;
    bool italic = false;
    switch (style) {
    case 1:
        weight = FW_BOLD;
        break;
    case 2:
        italic = true;
        break;
    case 3:
        weight = FW_BOLD;
        italic = true;
        break;
    default:
        break;
    }

    if (family == fontFamily_ && size == fontSize_
        && weight == fontWeight_ && italic == fontItalic_) {
        return;
    }

    fontFamily_ = family;
    fontSize_ = size;
    fontWeight_ = weight;
    fontItalic_ = italic;
    rasterizer_->setFont({fontFamily_, fontSize_, fontWeight_, fontItalic_});
    updateFontWarning();
    updateMetricsWarning();
    invalidatePreview();
}

void MainWindow::onMetricsChanged()
{
    if (rasterizer_ == nullptr || initializing_) {
        return;
    }

    const int width = integerControl(IDC_WIDTH, glyphWidth_, kMinCellSize, kMaxCellSize, false);
    const int height = integerControl(IDC_HEIGHT, glyphHeight_, kMinCellSize, kMaxCellSize, false);
    if (width == glyphWidth_ && height == glyphHeight_) {
        return;
    }

    glyphWidth_ = width;
    glyphHeight_ = height;
    rasterizer_->setMetrics(glyphWidth_, glyphHeight_, offsetX_, offsetY_);
    updateMetricsWarning();
    invalidatePreview();
}

void MainWindow::onOffsetChanged()
{
    if (rasterizer_ == nullptr || initializing_) {
        return;
    }

    const int offsetX = integerControl(IDC_OFFSET_X, offsetX_, kMinOffset, kMaxOffset, true);
    const int offsetY = integerControl(IDC_OFFSET_Y, offsetY_, kMinOffset, kMaxOffset, true);
    if (offsetX == offsetX_ && offsetY == offsetY_) {
        return;
    }

    offsetX_ = offsetX;
    offsetY_ = offsetY;
    rasterizer_->setMetrics(glyphWidth_, glyphHeight_, offsetX_, offsetY_);
    invalidatePreview();
}

void MainWindow::onCharacterIndexChanged(int delta)
{
    if (charset_.empty() || delta == 0) {
        return;
    }

    const int maximum = static_cast<int>(charset_.size()) - 1;
    const int next = std::clamp(characterIndex_ + delta, 0, maximum);
    if (next == characterIndex_) {
        return;
    }

    characterIndex_ = next;
    updateIndexLabel();
    invalidatePreview();
}

void MainWindow::onStandardCharset()
{
    if (charsetFiles_.empty()) {
        return;
    }
    EnableWindow(GetDlgItem(window_, IDC_CHARSET_LIST), TRUE);
    EnableWindow(GetDlgItem(window_, IDC_EDIT_TABLE), FALSE);
    loadSelectedCharset();
}

void MainWindow::onCustomCharset()
{
    EnableWindow(GetDlgItem(window_, IDC_CHARSET_LIST), FALSE);
    EnableWindow(GetDlgItem(window_, IDC_EDIT_TABLE), TRUE);
    applyCustomCharset();
}

void MainWindow::onCharsetSelectionChanged()
{
    if (checked(IDC_STD)) {
        loadSelectedCharset();
    }
}

void MainWindow::onEditTable()
{
    CharsetEditorDialog editor(customText_);
    const auto text = editor.show(window_, instance_);
    if (!text) {
        return;
    }

    customText_ = *text;
    CheckRadioButton(window_, IDC_STD, IDC_USER, IDC_USER);
    onCustomCharset();
}

void MainWindow::populateFonts()
{
    fontFamilies_ = win32::enumerateFontFamilies();
    if (fontFamilies_.empty()) {
        fontFamilies_.push_back(L"Segoe UI");
    }

    for (const auto& family : fontFamilies_) {
        SendDlgItemMessageW(window_, IDC_FONT_LIST, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(family.c_str()));
    }

    const auto select = [&](const wchar_t* name) {
        const LRESULT index = SendDlgItemMessageW(
            window_, IDC_FONT_LIST, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1),
            reinterpret_cast<LPARAM>(name));
        if (index != CB_ERR) {
            SendDlgItemMessageW(window_, IDC_FONT_LIST, CB_SETCURSEL,
                static_cast<WPARAM>(index), 0);
            return true;
        }
        return false;
    };

    if ((!settings_.fontFamily.empty() && select(settings_.fontFamily.c_str()))
        || select(L"Microsoft YaHei UI")
        || select(L"Segoe UI")
        || !fontFamilies_.empty()) {
        if (SendDlgItemMessageW(window_, IDC_FONT_LIST, CB_GETCURSEL, 0, 0) == CB_ERR) {
            SendDlgItemMessageW(window_, IDC_FONT_LIST, CB_SETCURSEL, 0, 0);
        }
    }

    fontFamily_ = selectedFontFamily();
    if (fontFamily_.empty()) {
        fontFamily_ = fontFamilies_.front();
    }
}

void MainWindow::populateCharsets()
{
    charsetDirectory_ = win32::executableDirectory() / L"charset";

    std::error_code error;
    for (std::filesystem::directory_iterator iterator(charsetDirectory_, error), end;
         !error && iterator != end; iterator.increment(error)) {
        if (!iterator->is_regular_file(error)) {
            continue;
        }
        const auto path = iterator->path();
        if (path.extension() == L".cst") {
            charsetFiles_.push_back(path);
        }
    }

    std::sort(charsetFiles_.begin(), charsetFiles_.end(),
        [](const auto& left, const auto& right) {
            return left.filename().wstring() < right.filename().wstring();
        });

    for (const auto& path : charsetFiles_) {
        const std::wstring name = path.stem().wstring();
        SendDlgItemMessageW(window_, IDC_CHARSET_LIST, CB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(name.c_str()));
    }

    if (charsetFiles_.empty()) {
        EnableWindow(GetDlgItem(window_, IDC_STD), FALSE);
        CheckRadioButton(window_, IDC_STD, IDC_USER, IDC_USER);
        onCustomCharset();
        setStatus(L"未找到标准字符集，已切换到用户码表");
    } else {
        CheckRadioButton(window_, IDC_STD, IDC_USER, IDC_STD);
        SendDlgItemMessageW(window_, IDC_CHARSET_LIST, CB_SETCURSEL, 0, 0);
        onStandardCharset();
    }
}

void MainWindow::loadSelectedCharset()
{
    const LRESULT selection = SendDlgItemMessageW(window_, IDC_CHARSET_LIST, CB_GETCURSEL, 0, 0);
    if (selection == CB_ERR || static_cast<std::size_t>(selection) >= charsetFiles_.size()) {
        return;
    }

    try {
        charset_ = Charset::loadCst(charsetFiles_[static_cast<std::size_t>(selection)]);
        characterIndex_ = 0;
        setStatus(L"已加载 " + charsetFiles_[static_cast<std::size_t>(selection)].filename().wstring()
            + L"（" + std::to_wstring(charset_.size()) + L" 个字符）");
        updateIndexLabel();
        invalidatePreview();
    } catch (const std::exception& error) {
        charset_ = Charset{};
        characterIndex_ = 0;
        setStatus(L"字符集加载失败");
        updateIndexLabel();
        invalidatePreview();
        win32::showError(window_, L"字符集加载失败", win32::widen(error.what()));
    }
}

void MainWindow::applyCustomCharset()
{
    try {
        charset_ = Charset::fromUtf8(customText_);
        characterIndex_ = 0;
        setStatus(charset_.empty()
                ? L"用户码表为空"
                : L"用户码表：" + std::to_wstring(charset_.size()) + L" 个字符");
        updateIndexLabel();
        invalidatePreview();
    } catch (const std::exception& error) {
        charset_ = Charset{};
        characterIndex_ = 0;
        updateIndexLabel();
        invalidatePreview();
        win32::showError(window_, L"码表无效", win32::widen(error.what()));
    }
}

void MainWindow::updateIndexLabel()
{
    std::wostringstream label;
    if (charset_.empty()) {
        label << L"当前字符: - (0/0)";
        SetDlgItemTextW(window_, IDC_INDEX, label.str().c_str());
        SendDlgItemMessageW(window_, IDC_SPIN_INDEX, UDM_SETRANGE32, 0, 0);
        return;
    }

    characterIndex_ = std::clamp(
        characterIndex_, 0, static_cast<int>(charset_.size()) - 1);
    const char32_t codepoint = charset_[static_cast<std::size_t>(characterIndex_)];

    const int digits = codepoint > 0xFFFF ? 5 : 4;
    label << L"当前字符: U+" << std::uppercase << std::hex << std::setw(digits)
          << std::setfill(L'0') << static_cast<std::uint32_t>(codepoint);

    if (isCommentPrintable(codepoint)) {
        const std::u16string units = utf16Encode(codepoint);
        std::wstring shown;
        shown.reserve(units.size());
        for (const char16_t unit : units) {
            shown.push_back(static_cast<wchar_t>(unit));
        }
        label << L" '" << shown << L"'";
    }

    label << L" (" << std::dec << (characterIndex_ + 1) << L"/" << charset_.size() << L")";
    SetDlgItemTextW(window_, IDC_INDEX, label.str().c_str());

    SendDlgItemMessageW(window_, IDC_SPIN_INDEX, UDM_SETRANGE32, 0,
        static_cast<LPARAM>(charset_.size() - 1));
    SendDlgItemMessageW(window_, IDC_SPIN_INDEX, UDM_SETPOS32, 0,
        static_cast<LPARAM>(characterIndex_));
}

void MainWindow::saveUiSettings()
{
    if (window_ == nullptr) {
        return;
    }

    const std::wstring family = selectedFontFamily();
    if (!family.empty()) {
        settings_.fontFamily = family;
    }
    settings_.fontSize = fontSize_;
    settings_.width = glyphWidth_;
    settings_.height = glyphHeight_;
    settings_.offsetX = offsetX_;
    settings_.offsetY = offsetY_;
    settings_.style = selectedStyleIndex();
    settings_.scanVertical = checked(IDC_SCAN_V);
    settings_.lsbFirst = checked(IDC_LSB);
    settings_.variableWidth = checked(IDC_VARIABLE);
    settings_.binaryOutput = checked(IDC_FORMAT_BIN);
    settings_.metadata = checked(IDC_METADATA);
    saveSettings(settings_);
}

void MainWindow::invalidatePreview()
{
    if (window_ != nullptr) {
        InvalidateRect(GetDlgItem(window_, IDC_PREVIEW), nullptr, FALSE);
    }
}

void MainWindow::updateFontWarning()
{
    if (rasterizer_ == nullptr) {
        return;
    }
    if (rasterizer_->substituted()) {
        fontWarning_ = L"字体被替换为 " + rasterizer_->actualFamily();
    } else {
        fontWarning_.clear();
    }
    renderStatus();
}

void MainWindow::updateMetricsWarning()
{
    if (fontSize_ > glyphHeight_) {
        metricsWarning_ = L"字号大于点阵高度，字形可能被裁剪";
    } else {
        metricsWarning_.clear();
    }
    renderStatus();
}

void MainWindow::renderStatus()
{
    if (window_ == nullptr) {
        return;
    }

    std::wstring text;
    const auto append = [&](const std::wstring& part) {
        if (part.empty()) {
            return;
        }
        if (!text.empty()) {
            text += L" | ";
        }
        text += part;
    };
    if (!fontWarning_.empty()) {
        append(L"⚠ " + fontWarning_);
    }
    if (!metricsWarning_.empty()) {
        append(L"⚠ " + metricsWarning_);
    }
    append(statusText_);
    win32::setWindowText(window_, IDC_STATUS,
        text.empty() ? L"就绪" : text);
}

void MainWindow::setStatus(const std::wstring& text)
{
    statusText_ = text;
    renderStatus();
}

void MainWindow::normalizeIntegerControl(
    int controlId, int minimum, int maximum, bool signedValue)
{
    int fallback = 0;
    switch (controlId) {
    case IDC_FONT_SIZE:
        fallback = fontSize_;
        break;
    case IDC_WIDTH:
        fallback = glyphWidth_;
        break;
    case IDC_HEIGHT:
        fallback = glyphHeight_;
        break;
    case IDC_OFFSET_X:
        fallback = offsetX_;
        break;
    case IDC_OFFSET_Y:
        fallback = offsetY_;
        break;
    default:
        return;
    }

    const int value = integerControl(controlId, fallback, minimum, maximum, signedValue);
    SetDlgItemInt(window_, controlId, static_cast<UINT>(value), signedValue ? TRUE : FALSE);
}

int MainWindow::integerControl(
    int controlId, int fallback, int minimum, int maximum, bool signedValue) const
{
    BOOL success = FALSE;
    const UINT raw = GetDlgItemInt(window_, controlId, &success, signedValue ? TRUE : FALSE);
    if (!success) {
        return fallback;
    }
    return std::clamp(static_cast<int>(raw), minimum, maximum);
}

std::wstring MainWindow::selectedFontFamily() const
{
    return win32::windowText(window_, IDC_FONT_LIST);
}

int MainWindow::selectedStyleIndex() const
{
    const LRESULT selection = SendDlgItemMessageW(window_, IDC_FONT_STYLE, CB_GETCURSEL, 0, 0);
    return selection == CB_ERR ? 0 : static_cast<int>(selection);
}

bool MainWindow::checked(int controlId) const
{
    return IsDlgButtonChecked(window_, controlId) == BST_CHECKED;
}

} // namespace fontmaker::ui
