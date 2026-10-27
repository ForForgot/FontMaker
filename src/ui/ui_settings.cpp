#include "ui_settings.hpp"

#include "fontmaker/packing.hpp"

#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

namespace fontmaker::ui {
namespace {

constexpr const wchar_t kRegistryPath[] = L"Software\\FontMaker\\2.0";

DWORD readDword(HKEY key, const wchar_t* name, DWORD fallback)
{
    DWORD value = fallback;
    DWORD type = 0;
    DWORD size = sizeof(value);
    if (RegQueryValueExW(key, name, nullptr, &type,
            reinterpret_cast<LPBYTE>(&value), &size) != ERROR_SUCCESS
        || type != REG_DWORD) {
        return fallback;
    }
    return value;
}

std::wstring readString(HKEY key, const wchar_t* name)
{
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS
        || type != REG_SZ || size < sizeof(wchar_t)) {
        return {};
    }

    std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
    if (RegQueryValueExW(key, name, nullptr, &type,
            reinterpret_cast<LPBYTE>(buffer.data()), &size) != ERROR_SUCCESS) {
        return {};
    }
    return std::wstring(buffer.data());
}

void writeDword(HKEY key, const wchar_t* name, DWORD value)
{
    RegSetValueExW(key, name, 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));
}

void writeString(HKEY key, const wchar_t* name, const std::wstring& value)
{
    RegSetValueExW(key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
}

} // namespace

UiSettings loadSettings()
{
    UiSettings settings;
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        return settings;
    }

    settings.fontFamily = readString(key, L"FontFamily");
    settings.fontSize = std::clamp(static_cast<int>(readDword(key, L"FontSize", 16)),
        kMinFontPixelHeight, kMaxFontPixelHeight);
    settings.width = std::clamp(static_cast<int>(readDword(key, L"Width", 16)),
        kMinGlyphDimension, kMaxGlyphDimension);
    settings.height = std::clamp(static_cast<int>(readDword(key, L"Height", 16)),
        kMinGlyphDimension, kMaxGlyphDimension);
    settings.offsetX = std::clamp(static_cast<int>(readDword(key, L"OffsetX", 0)),
        -kMaxGlyphOffset, kMaxGlyphOffset);
    settings.offsetY = std::clamp(static_cast<int>(readDword(key, L"OffsetY", 0)),
        -kMaxGlyphOffset, kMaxGlyphOffset);
    settings.style = std::clamp(static_cast<int>(readDword(key, L"Style", 0)), 0, 3);
    settings.scanVertical = readDword(key, L"ScanVertical", 0) != 0;
    settings.lsbFirst = readDword(key, L"LsbFirst", 0) != 0;
    settings.variableWidth = readDword(key, L"VariableWidth", 0) != 0;
    settings.binaryOutput = readDword(key, L"BinaryOutput", 1) != 0;
    settings.metadata = readDword(key, L"Metadata", 0) != 0;

    RegCloseKey(key);
    return settings;
}

void saveSettings(const UiSettings& settings)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegistryPath, 0, nullptr, 0,
            KEY_WRITE, nullptr, &key, nullptr) != ERROR_SUCCESS) {
        return;
    }

    writeString(key, L"FontFamily", settings.fontFamily);
    writeDword(key, L"FontSize", static_cast<DWORD>(settings.fontSize));
    writeDword(key, L"Width", static_cast<DWORD>(settings.width));
    writeDword(key, L"Height", static_cast<DWORD>(settings.height));
    writeDword(key, L"OffsetX", static_cast<DWORD>(settings.offsetX));
    writeDword(key, L"OffsetY", static_cast<DWORD>(settings.offsetY));
    writeDword(key, L"Style", static_cast<DWORD>(settings.style));
    writeDword(key, L"ScanVertical", settings.scanVertical ? 1 : 0);
    writeDword(key, L"LsbFirst", settings.lsbFirst ? 1 : 0);
    writeDword(key, L"VariableWidth", settings.variableWidth ? 1 : 0);
    writeDword(key, L"BinaryOutput", settings.binaryOutput ? 1 : 0);
    writeDword(key, L"Metadata", settings.metadata ? 1 : 0);
    RegCloseKey(key);
}

} // namespace fontmaker::ui
