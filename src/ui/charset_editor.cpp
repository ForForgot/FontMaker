#include "charset_editor.hpp"

#include "resource.h"
#include "win32_utils.hpp"

#include "fontmaker/charset.hpp"
#include "fontmaker/error.hpp"
#include "fontmaker/io.hpp"

#include <commctrl.h>

#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace fontmaker::ui {
namespace {

constexpr const wchar_t kTextFilter[] =
    L"文本文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0\0";

} // namespace

CharsetEditorDialog::CharsetEditorDialog(std::string text)
    : text_(std::move(text))
{
}

std::optional<std::string> CharsetEditorDialog::show(HWND owner, HINSTANCE instance)
{
    instance_ = instance;
    const INT_PTR result = DialogBoxParamW(
        instance_, MAKEINTRESOURCEW(IDD_CHARSET_EDITOR), owner, dialogProc,
        reinterpret_cast<LPARAM>(this));
    if (result == -1) {
        win32::showError(owner, L"打开失败",
            L"无法创建码表编辑窗口。\n" + win32::formatSystemError(GetLastError()));
        return std::nullopt;
    }
    if (result != IDOK) {
        return std::nullopt;
    }
    return text_;
}

INT_PTR CALLBACK CharsetEditorDialog::dialogProc(
    HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_INITDIALOG) {
        auto* self = reinterpret_cast<CharsetEditorDialog*>(lParam);
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

    auto* self = reinterpret_cast<CharsetEditorDialog*>(GetWindowLongPtrW(window, DWLP_USER));
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

bool CharsetEditorDialog::onInitDialog()
{
    SetWindowTextW(window_, L"自定义码表");
    CheckDlgButton(window_, IDC_DEDUPE, dedupe_ ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(window_, IDC_FILTER_CONTROLS, filterControls_ ? BST_CHECKED : BST_UNCHECKED);
    setEditorText(win32::widen(text_));
    SetFocus(GetDlgItem(window_, IDC_TABLE_TEXT));
    return false;
}

INT_PTR CharsetEditorDialog::handleMessage(UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (message) {
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            onAccept();
            return TRUE;
        case IDCANCEL:
            EndDialog(window_, IDCANCEL);
            return TRUE;
        case IDC_LOAD:
            onLoad();
            return TRUE;
        case IDC_SAVE_TEXT:
            onSave();
            return TRUE;
        case IDC_INSERT_DIGITS:
            insertText(L"0123456789");
            return TRUE;
        case IDC_INSERT_LETTERS:
            insertText(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
            return TRUE;
        case IDC_TABLE_TEXT:
            if (HIWORD(wParam) == EN_CHANGE) {
                updateCount();
            }
            return TRUE;
        case IDC_DEDUPE:
            dedupe_ = IsDlgButtonChecked(window_, IDC_DEDUPE) == BST_CHECKED;
            updateCount();
            return TRUE;
        case IDC_FILTER_CONTROLS:
            filterControls_ = IsDlgButtonChecked(window_, IDC_FILTER_CONTROLS) == BST_CHECKED;
            updateCount();
            return TRUE;
        default:
            break;
        }
        break;
    case WM_CLOSE:
        EndDialog(window_, IDCANCEL);
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

void CharsetEditorDialog::onLoad()
{
    const auto path = win32::openFileDialog(window_, kTextFilter, L"载入 UTF-8 码表");
    if (!path) {
        return;
    }

    try {
        const Charset charset = Charset::loadUtf8File(*path);
        text_ = charset.toUtf8();
        setEditorText(win32::widen(text_));
        updateCount();
        SetFocus(GetDlgItem(window_, IDC_TABLE_TEXT));
    } catch (const std::exception& error) {
        win32::showError(window_, L"载入失败", win32::widen(error.what()));
    }
}

void CharsetEditorDialog::onSave()
{
    try {
        const Charset cleaned = currentCharset();
        if (cleaned.empty()) {
            win32::showError(window_, L"无法保存", L"清理后的码表不能为空。");
            return;
        }

        const std::string utf8 = cleaned.toUtf8();
        const auto path = win32::saveFileDialog(window_, kTextFilter, L"保存 UTF-8 码表", L"txt");
        if (!path) {
            return;
        }

        writeBinaryFile(*path, std::span<const std::uint8_t>(
            reinterpret_cast<const std::uint8_t*>(utf8.data()), utf8.size()));
        win32::showInfo(window_, L"保存成功", L"码表已保存为 UTF-8 文本。");
    } catch (const std::exception& error) {
        win32::showError(window_, L"保存失败", win32::widen(error.what()));
    }
}

void CharsetEditorDialog::onAccept()
{
    try {
        const Charset cleaned = currentCharset();
        if (cleaned.empty()) {
            win32::showError(window_, L"码表为空", L"清理后的码表不能为空，请至少输入一个非控制字符。");
            SetFocus(GetDlgItem(window_, IDC_TABLE_TEXT));
            return;
        }
        text_ = cleaned.toUtf8();
        EndDialog(window_, IDOK);
    } catch (const std::exception& error) {
        win32::showError(window_, L"码表无效", win32::widen(error.what()));
        SetFocus(GetDlgItem(window_, IDC_TABLE_TEXT));
    }
}

Charset CharsetEditorDialog::currentCharset() const
{
    Charset charset = Charset::fromUtf8(win32::narrow(editorText()));
    CharsetCleanup cleanup;
    cleanup.removeControls = filterControls_;
    cleanup.dedupe = dedupe_;
    return charset.cleaned(cleanup);
}

void CharsetEditorDialog::updateCount()
{
    try {
        const std::size_t count = currentCharset().size();
        win32::setWindowText(window_, IDC_CHAR_COUNT,
            std::to_wstring(count) + L" 个字符");
    } catch (const std::exception&) {
        win32::setWindowText(window_, IDC_CHAR_COUNT, L"码表无效");
    }
}

void CharsetEditorDialog::insertText(std::wstring_view text)
{
    HWND edit = GetDlgItem(window_, IDC_TABLE_TEXT);
    if (edit == nullptr) {
        return;
    }
    SendMessageW(edit, EM_REPLACESEL, TRUE,
        reinterpret_cast<LPARAM>(std::wstring(text).c_str()));
    updateCount();
    SetFocus(edit);
}

std::wstring CharsetEditorDialog::editorText() const
{
    HWND edit = GetDlgItem(window_, IDC_TABLE_TEXT);
    if (edit == nullptr) {
        return {};
    }
    const int length = GetWindowTextLengthW(edit);
    if (length <= 0) {
        return {};
    }
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(edit, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return text;
}

void CharsetEditorDialog::setEditorText(std::wstring_view text)
{
    SetDlgItemTextW(window_, IDC_TABLE_TEXT, std::wstring(text).c_str());
    updateCount();
}

} // namespace fontmaker::ui
