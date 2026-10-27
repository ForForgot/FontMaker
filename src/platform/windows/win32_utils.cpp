#include "win32_utils.hpp"

#include "fontmaker/error.hpp"
#include "fontmaker/io.hpp"

#include <commdlg.h>

#include <algorithm>
#include <vector>

namespace fontmaker::win32 {
namespace {

std::wstring trimNewlines(std::wstring value)
{
    while (!value.empty() && (value.back() == L'\r' || value.back() == L'\n' || value.back() == L' ')) {
        value.pop_back();
    }
    return value;
}

std::optional<std::filesystem::path> runFileDialog(
    HWND owner,
    bool save,
    const wchar_t* filter,
    const wchar_t* title,
    const wchar_t* defaultExtension)
{
    std::vector<wchar_t> buffer(32768, L'\0');

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = title;
    dialog.lpstrDefExt = defaultExtension;
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (save) {
        dialog.Flags |= OFN_OVERWRITEPROMPT;
    }

    const BOOL accepted = save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog);
    if (accepted == FALSE) {
        const DWORD error = CommDlgExtendedError();
        if (error != 0) {
            throw Error("file dialog failed (error " + std::to_string(error) + ")");
        }
        return std::nullopt;
    }

    return std::filesystem::path(buffer.data());
}

} // namespace

std::filesystem::path executableDirectory()
{
    std::vector<wchar_t> buffer(1024, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            throw Error("GetModuleFileNameW failed");
        }
        if (length < buffer.size() - 1) {
            return std::filesystem::path(buffer.data()).parent_path();
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::wstring widen(std::string_view text)
{
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        throw Error("cannot convert UTF-8 text to UTF-16: invalid input");
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
            result.data(), length) <= 0) {
        throw Error("cannot convert UTF-8 text to UTF-16");
    }
    return result;
}

std::string narrow(std::wstring_view text)
{
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        throw Error("cannot convert UTF-16 text to UTF-8: unpaired surrogate");
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
            result.data(), length, nullptr, nullptr) <= 0) {
        throw Error("cannot convert UTF-16 text to UTF-8");
    }
    return result;
}

std::wstring formatSystemError(DWORD error)
{
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);
    if (length == 0 || buffer == nullptr) {
        return L"unknown Windows error " + std::to_wstring(error);
    }
    std::wstring result(buffer, length);
    LocalFree(buffer);
    return trimNewlines(std::move(result));
}

std::wstring windowText(HWND window, int controlId)
{
    HWND control = GetDlgItem(window, controlId);
    if (control == nullptr) {
        return {};
    }
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }
    std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(control, buffer.data(), static_cast<int>(buffer.size()));
    return std::wstring(buffer.data());
}

void setWindowText(HWND window, int controlId, std::wstring_view text)
{
    SetDlgItemTextW(window, controlId, std::wstring(text).c_str());
}

void showError(HWND owner, std::wstring_view title, std::wstring_view message)
{
    MessageBoxW(owner, std::wstring(message).c_str(), std::wstring(title).c_str(),
        MB_OK | MB_ICONERROR);
}

void showInfo(HWND owner, std::wstring_view title, std::wstring_view message)
{
    MessageBoxW(owner, std::wstring(message).c_str(), std::wstring(title).c_str(),
        MB_OK | MB_ICONINFORMATION);
}

void centerWindow(HWND window)
{
    RECT windowRect{};
    if (!GetWindowRect(window, &windowRect)) {
        return;
    }

    RECT workArea{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;
    const int x = workArea.left + ((workArea.right - workArea.left) - width) / 2;
    const int y = workArea.top + ((workArea.bottom - workArea.top) - height) / 2;
    SetWindowPos(window, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

std::optional<std::filesystem::path> openFileDialog(
    HWND owner,
    const wchar_t* filter,
    const wchar_t* title,
    const wchar_t* defaultExtension)
{
    return runFileDialog(owner, false, filter, title, defaultExtension);
}

std::optional<std::filesystem::path> saveFileDialog(
    HWND owner,
    const wchar_t* filter,
    const wchar_t* title,
    const wchar_t* defaultExtension)
{
    return runFileDialog(owner, true, filter, title, defaultExtension);
}

WaitCursor::WaitCursor()
    : previous_(SetCursor(LoadCursorW(nullptr, IDC_WAIT)))
{
    if (previous_ == nullptr) {
        previous_ = LoadCursorW(nullptr, IDC_ARROW);
    }
}

WaitCursor::~WaitCursor()
{
    SetCursor(previous_);
}

} // namespace fontmaker::win32
