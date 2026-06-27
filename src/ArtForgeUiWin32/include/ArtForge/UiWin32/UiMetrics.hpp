#pragma once

#include <windows.h>

namespace ArtForge::UiWin32 {

struct ShellUiMetrics {
    int margin{10};
    int gap{8};
    int splitterWidth{4};
    int navigationWidth{280};
    int inspectorWidth{300};
    int minimumDocumentWidth{220};
    int toolbarHeight{32};
    int tabHeight{28};
    int inspectorRowHeight{24};
    int outputPaneHeight{132};
};

[[nodiscard]] ShellUiMetrics DefaultShellUiMetrics();

void InitializeCommonControlsForShell();
void ApplyDefaultGuiFont(HWND window);

}
