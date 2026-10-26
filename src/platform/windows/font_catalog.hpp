#pragma once

#include <string>
#include <vector>

namespace fontmaker::win32 {

/// Enumerate installed font families (not styles), excluding the vertical
/// `@font` variants that cannot be used for horizontal layout.
[[nodiscard]] std::vector<std::wstring> enumerateFontFamilies();

} // namespace fontmaker::win32
