#include "font_catalog.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace fontmaker::win32 {
namespace {

struct CaseInsensitiveLess {
    bool operator()(const std::wstring& left, const std::wstring& right) const noexcept
    {
        return CompareStringOrdinal(
                   left.c_str(), static_cast<int>(left.size()),
                   right.c_str(), static_cast<int>(right.size()), TRUE)
            == CSTR_LESS_THAN;
    }
};

int CALLBACK enumFontCallback(
    const LOGFONTW* logFont,
    const TEXTMETRICW* /*metrics*/,
    DWORD /*fontType*/,
    LPARAM userData)
{
    auto* families = reinterpret_cast<std::set<std::wstring, CaseInsensitiveLess>*>(userData);
    if (logFont != nullptr && logFont->lfFaceName[0] != L'@' && logFont->lfFaceName[0] != L'\0') {
        families->insert(logFont->lfFaceName);
    }
    return 1;
}

} // namespace

std::vector<std::wstring> enumerateFontFamilies()
{
    std::set<std::wstring, CaseInsensitiveLess> families;
    HDC dc = GetDC(nullptr);
    if (dc != nullptr) {
        LOGFONTW filter{};
        filter.lfCharSet = DEFAULT_CHARSET;
        EnumFontFamiliesExW(dc, &filter, enumFontCallback,
            reinterpret_cast<LPARAM>(&families), 0);
        ReleaseDC(nullptr, dc);
    }

    return {families.begin(), families.end()};
}

} // namespace fontmaker::win32
