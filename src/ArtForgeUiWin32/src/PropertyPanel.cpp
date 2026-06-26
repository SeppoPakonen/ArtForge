#include "ArtForge/UiWin32/PropertyPanel.hpp"

namespace ArtForge::UiWin32 {

HWND PropertyPanel::Create(HWND parent, int controlId, HINSTANCE instance)
{
    const auto window = list_.Create(parent, controlId, instance);
    list_.AddColumn(0, L"Name", 112);
    list_.AddColumn(1, L"Value", 360);
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
    list_.AddRow({name, value});
}

void PropertyPanel::Move(const RECT& bounds)
{
    list_.Move(bounds);
}

}
