#include "ArtForge/UiWin32/CommonControls.hpp"

#include <commctrl.h>

namespace ArtForge::UiWin32 {

HWND ListViewReport::Create(HWND parent, int controlId, HINSTANCE instance)
{
    window_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_LISTVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        instance,
        nullptr);

    if (window_ != nullptr) {
        ListView_SetExtendedListViewStyle(window_, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    }

    return window_;
}

HWND ListViewReport::Window() const noexcept
{
    return window_;
}

void ListViewReport::AddColumn(int index, std::wstring_view title, int width)
{
    if (window_ == nullptr) {
        return;
    }

    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.iSubItem = index;
    column.cx = width;
    column.pszText = const_cast<wchar_t*>(title.data());
    ListView_InsertColumn(window_, index, &column);
}

void ListViewReport::AddRow(const std::vector<std::wstring_view>& cells)
{
    if (window_ == nullptr || cells.empty()) {
        return;
    }

    const auto rowIndex = ListView_GetItemCount(window_);
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = rowIndex;
    item.iSubItem = 0;
    item.pszText = const_cast<wchar_t*>(cells.front().data());
    ListView_InsertItem(window_, &item);

    for (std::size_t index = 1; index < cells.size(); ++index) {
        ListView_SetItemText(
            window_,
            rowIndex,
            static_cast<int>(index),
            const_cast<wchar_t*>(cells[index].data()));
    }
}

void ListViewReport::ClearRows()
{
    if (window_ != nullptr) {
        ListView_DeleteAllItems(window_);
    }
}

void ListViewReport::Move(const RECT& bounds)
{
    if (window_ != nullptr) {
        MoveWindow(
            window_,
            bounds.left,
            bounds.top,
            bounds.right - bounds.left,
            bounds.bottom - bounds.top,
            TRUE);
    }
}

HWND TabControl::Create(HWND parent, int controlId, HINSTANCE instance)
{
    window_ = CreateWindowExW(
        0,
        WC_TABCONTROLW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | TCS_TABS,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        instance,
        nullptr);
    return window_;
}

HWND TabControl::Window() const noexcept
{
    return window_;
}

void TabControl::AddTab(int index, std::wstring_view title)
{
    if (window_ == nullptr) {
        return;
    }

    TCITEMW item{};
    item.mask = TCIF_TEXT;
    item.pszText = const_cast<wchar_t*>(title.data());
    TabCtrl_InsertItem(window_, index, &item);
}

int TabControl::SelectedIndex() const
{
    if (window_ == nullptr) {
        return -1;
    }
    return TabCtrl_GetCurSel(window_);
}

void TabControl::Move(const RECT& bounds)
{
    if (window_ != nullptr) {
        MoveWindow(
            window_,
            bounds.left,
            bounds.top,
            bounds.right - bounds.left,
            bounds.bottom - bounds.top,
            TRUE);
    }
}

RECT TabControl::DisplayArea() const
{
    RECT bounds{};
    if (window_ == nullptr) {
        return bounds;
    }

    GetClientRect(window_, &bounds);
    TabCtrl_AdjustRect(window_, FALSE, &bounds);
    POINT topLeft{bounds.left, bounds.top};
    POINT bottomRight{bounds.right, bounds.bottom};
    MapWindowPoints(window_, GetParent(window_), &topLeft, 1);
    MapWindowPoints(window_, GetParent(window_), &bottomRight, 1);
    return {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y};
}

}
