#include "ArtForge/UiWin32/UiMetrics.hpp"

#include <commctrl.h>

namespace ArtForge::UiWin32 {

namespace {

HFONT ShellFont()
{
    static HFONT font = [] {
        NONCLIENTMETRICSW metrics{};
        metrics.cbSize = sizeof(metrics);
        if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0) != 0) {
            return CreateFontIndirectW(&metrics.lfMessageFont);
        }
        return reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    }();
    return font;
}

}

ShellUiMetrics DefaultShellUiMetrics()
{
    return {};
}

void InitializeCommonControlsForShell()
{
    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_BAR_CLASSES
        | ICC_STANDARD_CLASSES
        | ICC_TREEVIEW_CLASSES
        | ICC_LISTVIEW_CLASSES
        | ICC_TAB_CLASSES;
    InitCommonControlsEx(&commonControls);
}

void ApplyDefaultGuiFont(HWND window)
{
    if (window == nullptr) {
        return;
    }
    SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(ShellFont()), TRUE);
}

}
