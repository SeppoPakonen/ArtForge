#pragma once

#include "ArtForge/UiWin32/CommonControls.hpp"

#include <windows.h>

#include <string_view>

namespace ArtForge::UiWin32 {

class PropertyPanel {
public:
    HWND Create(HWND parent, int controlId, HINSTANCE instance);

    [[nodiscard]] HWND Window() const noexcept;

    void Clear();
    void AddGroup(std::wstring_view title);
    void AddProperty(std::wstring_view name, std::wstring_view value);
    void Move(const RECT& bounds);

private:
    ListViewReport list_;
};

}
