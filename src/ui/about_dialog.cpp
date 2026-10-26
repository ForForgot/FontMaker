#include "about_dialog.hpp"

#include "resource.h"
#include "win32_utils.hpp"

#include "fontmaker/version.hpp"

#include <commctrl.h>

#include <string>

namespace fontmaker::ui {
namespace {

INT_PTR CALLBACK aboutProc(HWND window, UINT message, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (message) {
    case WM_INITDIALOG: {
        const std::wstring version =
            L"FontMaker " FONTMAKER_VERSION_W L"\r\n\r\n"
            L"点阵字库生成工具\r\n\r\n"
            L"现代化 C++/Win32 重构版\r\n"
            L"输出格式与原版保持数据兼容。";
        SetDlgItemTextW(window, IDC_VERSION, version.c_str());

        const std::wstring license =
            L"本软件是免费软件，你可以随意使用和传播。\r\n\r\n"
            L"原版作者：星沉地动\r\n"
            L"重构：FontMaker contributors";
        SetDlgItemTextW(window, IDC_LICENSE, license.c_str());

        HBITMAP logo = static_cast<HBITMAP>(LoadImageW(
            reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window, GWLP_HINSTANCE)),
            MAKEINTRESOURCEW(IDB_LOGO), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR));
        if (logo != nullptr) {
            SendDlgItemMessageW(window, IDC_LOGO, STM_SETIMAGE,
                IMAGE_BITMAP, reinterpret_cast<LPARAM>(logo));
        }
        SetFocus(GetDlgItem(window, IDOK));
        return FALSE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(window, LOWORD(wParam));
            return TRUE;
        }
        break;
    case WM_DESTROY: {
        auto logo = reinterpret_cast<HBITMAP>(SendDlgItemMessageW(
            window, IDC_LOGO, STM_GETIMAGE, IMAGE_BITMAP, 0));
        if (logo != nullptr) {
            DeleteObject(logo);
        }
        return TRUE;
    }
    case WM_CLOSE:
        EndDialog(window, IDCANCEL);
        return TRUE;
    default:
        break;
    }
    return FALSE;
}

} // namespace

void showAboutDialog(HWND owner, HINSTANCE instance)
{
    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_ABOUT), owner, aboutProc, 0);
}

} // namespace fontmaker::ui
