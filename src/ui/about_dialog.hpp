#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

namespace fontmaker::ui {

void showAboutDialog(HWND owner, HINSTANCE instance);

} // namespace fontmaker::ui
