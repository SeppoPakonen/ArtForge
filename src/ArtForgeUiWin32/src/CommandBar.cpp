#include "ArtForge/UiWin32/CommandBar.hpp"

#include "ArtForge/UiWin32/UiMetrics.hpp"

namespace ArtForge::UiWin32 {

namespace {

int ButtonWidth(std::wstring_view label)
{
    const int width = static_cast<int>(label.size()) * 8 + 26;
    return width < 72 ? 72 : width;
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
    for (const auto& decoration : decorations_) {
        if (decoration.labelWindow != nullptr) {
            DestroyWindow(decoration.labelWindow);
        }
        if (decoration.separatorWindow != nullptr) {
            DestroyWindow(decoration.separatorWindow);
        }
    }
    buttons_.clear();
    decorations_.clear();

    std::wstring currentGroup;
    bool firstGroup = true;

    for (const auto& spec : buttons) {
        if (spec.group != currentGroup) {
            currentGroup = spec.group;
            HWND labelWindow = nullptr;
            HWND separatorWindow = nullptr;
            if (!currentGroup.empty()) {
                labelWindow = CreateWindowExW(
                    0,
                    L"STATIC",
                    currentGroup.c_str(),
                    WS_CHILD | WS_VISIBLE | SS_LEFT,
                    0,
                    0,
                    0,
                    0,
                    parent_,
                    nullptr,
                    instance,
                    nullptr);
                separatorWindow = CreateWindowExW(
                    0,
                    L"STATIC",
                    L"",
                    WS_CHILD | (firstGroup ? 0 : WS_VISIBLE) | SS_ETCHEDVERT,
                    0,
                    0,
                    0,
                    0,
                    parent_,
                    nullptr,
                    instance,
                    nullptr);
                ApplyDefaultGuiFont(labelWindow);
                decorations_.push_back({currentGroup, labelWindow, separatorWindow});
            }
            firstGroup = false;
        }

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
        buttons_.push_back({spec.commandId, spec.label, spec.group, window});
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
    const int groupTop = bounds.top + 2;
    const int buttonTop = bounds.top + 13;
    const int buttonHeight = bounds.bottom - buttonTop - 3;
    std::wstring currentGroup;
    std::size_t decorationIndex = 0;
    for (const auto& button : buttons_) {
        if (button.window == nullptr || !IsWindowVisible(button.window)) {
            continue;
        }
        if (button.group != currentGroup) {
            currentGroup = button.group;
            if (decorationIndex < decorations_.size()) {
                const auto& decoration = decorations_[decorationIndex++];
                if (decoration.separatorWindow != nullptr && IsWindowVisible(decoration.separatorWindow)) {
                    MoveWindow(decoration.separatorWindow, left, buttonTop, 2, buttonHeight, TRUE);
                    left += metrics.gap;
                }
                if (decoration.labelWindow != nullptr) {
                    const int labelWidth = static_cast<int>(decoration.group.size()) * 7 + 18;
                    MoveWindow(decoration.labelWindow, left, groupTop, labelWidth, 11, TRUE);
                }
            }
        }
        const int width = ButtonWidth(button.label);
        MoveWindow(button.window, left, buttonTop, width, buttonHeight, TRUE);
        left += width + metrics.gap;
    }
}

}
