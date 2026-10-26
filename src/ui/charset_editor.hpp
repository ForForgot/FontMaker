#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <optional>
#include <string>
#include <string_view>

namespace fontmaker::ui {

class CharsetEditorDialog {
public:
    explicit CharsetEditorDialog(std::string text);

    [[nodiscard]] std::optional<std::string> show(HWND owner, HINSTANCE instance);

private:
    static INT_PTR CALLBACK dialogProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    bool onInitDialog();
    void onLoad();
    void onSave();
    void onAccept();
    void insertText(std::wstring_view text);
    [[nodiscard]] std::wstring editorText() const;
    void setEditorText(std::wstring_view text);

    std::string text_;
    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
};

} // namespace fontmaker::ui
