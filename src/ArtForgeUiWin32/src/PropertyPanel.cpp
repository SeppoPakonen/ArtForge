#include "ArtForge/UiWin32/PropertyPanel.hpp"

#include "ArtForge/UiWin32/UiMetrics.hpp"

#include <commctrl.h>

#include <string>

namespace ArtForge::UiWin32 {

PropertyPanel::~PropertyPanel()
{
    if (rowHeightImage_ != nullptr) {
        ImageList_Destroy(rowHeightImage_);
    }
}

HWND PropertyPanel::Create(HWND parent, int controlId, HINSTANCE instance)
{
    const auto window = list_.Create(parent, controlId, instance);
    list_.AddColumn(0, L"Property", 132);
    list_.AddColumn(1, L"Value", 420);

    const auto metrics = DefaultShellUiMetrics();
    rowHeightImage_ = ImageList_Create(1, metrics.inspectorRowHeight, ILC_COLOR32, 1, 1);
    if (rowHeightImage_ != nullptr) {
        ListView_SetImageList(window, rowHeightImage_, LVSIL_SMALL);
    }
    return window;
}

HWND PropertyPanel::Window() const noexcept
{
    return list_.Window();
}

void PropertyPanel::Clear()
{
    list_.ClearRows();
}

void PropertyPanel::AddGroup(std::wstring_view title)
{
    list_.AddRow({title, L""});
}

void PropertyPanel::AddProperty(std::wstring_view name, std::wstring_view value)
{
    list_.AddRow({name, value.empty() ? std::wstring_view{L"(not set)"} : value});
}

void PropertyPanel::Move(const RECT& bounds)
{
    list_.Move(bounds);
    const int width = bounds.right - bounds.left;
    if (width > 0) {
        const int propertyWidth = width < 360 ? 118 : 142;
        list_.SetColumnWidth(0, propertyWidth);
        list_.SetColumnWidth(1, width - propertyWidth - 28);
    }
}

}
