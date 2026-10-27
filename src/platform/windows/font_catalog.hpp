#pragma once

#include <string>
#include <vector>

namespace fontmaker::win32 {

/// Enumerate installed font families (not styles), excluding the vertical
/// `@font` variants that cannot be used for horizontal layout.
[[nodiscard]] std::vector<std::wstring> enumerateFontFamilies();

/// True if GDI can enumerate at least one face for the requested family.
/// This is used instead of comparing localized display names returned by
/// GetTextFaceW, which produce false positives for aliases such as SimSun.
[[nodiscard]] bool fontFamilyExists(const std::wstring& family);

} // namespace fontmaker::win32
