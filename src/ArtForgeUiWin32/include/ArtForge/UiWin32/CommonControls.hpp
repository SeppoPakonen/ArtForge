#pragma once

#include <windows.h>

#include <string_view>
#include <vector>

namespace ArtForge::UiWin32 {

class ListViewReport {
public:
    HWND Create(HWND parent, int controlId, HINSTANCE instance);

    [[nodiscard]] HWND Window() const noexcept;

    void AddColumn(int index, std::wstring_view title, int width);
    void AddRow(const std::vector<std::wstring_view>& cells);
    void ClearRows();
    void Clear();
    void Move(const RECT& bounds);

private:
    HWND window_{};
};

class TabControl {
public:
    HWND Create(HWND parent, int controlId, HINSTANCE instance);

    [[nodiscard]] HWND Window() const noexcept;

    void AddTab(int index, std::wstring_view title);
    [[nodiscard]] int SelectedIndex() const;
    void Move(const RECT& bounds);
    [[nodiscard]] RECT DisplayArea() const;

private:
    HWND window_{};
};

}
