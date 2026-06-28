#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace ArtForge::UiWin32 {

struct CommandBarButtonSpec {
    int commandId{};
    std::wstring label;
    std::wstring group;
    bool enabled{true};
    bool visible{true};
    std::wstring disabledReason;
};

class CommandBar {
public:
    HWND Create(HWND parent, int controlId, HINSTANCE instance);

    [[nodiscard]] HWND Window() const noexcept;

    void SetButtons(const std::vector<CommandBarButtonSpec>& buttons, HINSTANCE instance);
    void SetButtonEnabled(int commandId, bool enabled);
    void SetButtonVisible(int commandId, bool visible);
    void Move(const RECT& bounds);

private:
    struct Button {
        int commandId{};
        std::wstring label;
        std::wstring group;
        HWND window{};
    };

    struct GroupDecoration {
        std::wstring group;
        HWND labelWindow{};
        HWND separatorWindow{};
    };

    HWND parent_{};
    HWND bar_{};
    std::vector<Button> buttons_;
    std::vector<GroupDecoration> decorations_;
};

}
