#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace fontmaker::win32 {

[[nodiscard]] std::filesystem::path executableDirectory();
[[nodiscard]] std::wstring widen(std::string_view text);
[[nodiscard]] std::string narrow(std::wstring_view text);
[[nodiscard]] std::wstring formatSystemError(DWORD error);
[[nodiscard]] std::wstring windowText(HWND window, int controlId);
void setWindowText(HWND window, int controlId, std::wstring_view text);
void showError(HWND owner, std::wstring_view title, std::wstring_view message);
void showInfo(HWND owner, std::wstring_view title, std::wstring_view message);
void centerWindow(HWND window);

[[nodiscard]] std::optional<std::filesystem::path> openFileDialog(
    HWND owner,
    const wchar_t* filter,
    const wchar_t* title,
    const wchar_t* defaultExtension = nullptr);

[[nodiscard]] std::optional<std::filesystem::path> saveFileDialog(
    HWND owner,
    const wchar_t* filter,
    const wchar_t* title,
    const wchar_t* defaultExtension = nullptr);

/// RAII wait cursor. Restores the previous cursor in its destructor.
class WaitCursor {
public:
    WaitCursor();
    ~WaitCursor();
    WaitCursor(const WaitCursor&) = delete;
    WaitCursor& operator=(const WaitCursor&) = delete;

private:
    HCURSOR previous_ = nullptr;
};

} // namespace fontmaker::win32
