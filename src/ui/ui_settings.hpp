#pragma once

#include <string>

namespace fontmaker::ui {

struct UiSettings {
    std::wstring fontFamily;
    int fontSize = 16;
    int width = 16;
    int height = 16;
    int offsetX = 0;
    int offsetY = 0;
    int style = 0;
    bool scanVertical = false;
    bool lsbFirst = false;
    bool variableWidth = false;
    bool binaryOutput = true;
    bool metadata = false;
    bool standardDedupe = false;
};

[[nodiscard]] UiSettings loadSettings();
void saveSettings(const UiSettings& settings);

} // namespace fontmaker::ui
