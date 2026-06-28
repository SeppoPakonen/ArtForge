#include "ArtForge/UiWin32/CommandBar.hpp"

#include "ArtForge/UiWin32/UiMetrics.hpp"

namespace ArtForge::UiWin32 {

namespace {

int ButtonWidth(std::wstring_view label)
{
    return static_cast<int>(label.size()) * 8 + 22;
}

}

HWND CommandBar::Create(HWND parent, int controlId, HINSTANCE instance)
{
    parent_ = parent;
    bar_ = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD | WS_VISIBLE,
        0,
        0,
        0,
        0,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)),
        instance,
        nullptr);
    return bar_;
}

HWND CommandBar::Window() const noexcept
{
    return bar_;
}

void CommandBar::SetButtons(const std::vector<CommandBarButtonSpec>& buttons, HINSTANCE instance)
{
    for (const auto& button : buttons_) {
        if (button.window != nullptr) {
            DestroyWindow(button.window);
        }
    }
    buttons_.clear();

    for (const auto& spec : buttons) {
        auto window = CreateWindowExW(
            0,
            L"BUTTON",
            spec.label.c_str(),
            WS_CHILD | (spec.visible ? WS_VISIBLE : 0) | WS_TABSTOP | BS_PUSHBUTTON,
            0,
            0,
            0,
            0,
            parent_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(spec.commandId)),
            instance,
            nullptr);
        ApplyDefaultGuiFont(window);
        EnableWindow(window, spec.enabled ? TRUE : FALSE);
        buttons_.push_back({spec.commandId, spec.label, window});
    }
}

void CommandBar::SetButtonEnabled(int commandId, bool enabled)
{
    for (const auto& button : buttons_) {
        if (button.commandId == commandId && button.window != nullptr) {
            EnableWindow(button.window, enabled ? TRUE : FALSE);
        }
    }
}

void CommandBar::SetButtonVisible(int commandId, bool visible)
{
    for (const auto& button : buttons_) {
        if (button.commandId == commandId && button.window != nullptr) {
            ShowWindow(button.window, visible ? SW_SHOW : SW_HIDE);
        }
    }
}

void CommandBar::Move(const RECT& bounds)
{
    if (bar_ != nullptr) {
        MoveWindow(
            bar_,
            bounds.left,
            bounds.top,
            bounds.right - bounds.left,
            bounds.bottom - bounds.top,
            TRUE);
    }

    const auto metrics = DefaultShellUiMetrics();
    int left = bounds.left + metrics.gap;
    const int top = bounds.top + 3;
    const int height = bounds.bottom - bounds.top - 6;
    for (const auto& button : buttons_) {
        if (button.window == nullptr || !IsWindowVisible(button.window)) {
            continue;
        }
        const int width = ButtonWidth(button.label);
        MoveWindow(button.window, left, top, width, height, TRUE);
        left += width + metrics.gap;
    }
}

}
