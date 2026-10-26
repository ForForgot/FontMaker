#include "main_window.hpp"
#include "win32_utils.hpp"

#include <windows.h>

#include <exception>

int WINAPI WinMain(HINSTANCE instance, HINSTANCE /*previous*/, LPSTR /*commandLine*/, int /*show*/)
{
    try {
        fontmaker::ui::MainWindow window(instance);
        return window.run();
    } catch (const std::exception& error) {
        MessageBoxW(nullptr, fontmaker::win32::widen(error.what()).c_str(),
            L"FontMaker 启动失败", MB_OK | MB_ICONERROR);
        return 1;
    }
}
