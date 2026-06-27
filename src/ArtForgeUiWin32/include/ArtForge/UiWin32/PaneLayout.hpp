#pragma once

#include "ArtForge/UiWin32/UiMetrics.hpp"

#include <windows.h>

namespace ArtForge::UiWin32 {

struct PaneLayoutMetrics {
    int margin{12};
    int gap{8};
    int leftWidth{280};
    int rightWidth{240};
    int minimumCenterWidth{180};
};

struct PaneLayoutRectangles {
    RECT left{};
    RECT center{};
    RECT right{};
};

PaneLayoutRectangles CalculateThreePaneLayout(
    const RECT& client,
    int bottomReservedHeight,
    const PaneLayoutMetrics& metrics = {});

PaneLayoutMetrics ToPaneLayoutMetrics(const ShellUiMetrics& metrics);

void ApplyThreePaneLayout(
    HWND leftPane,
    HWND centerPane,
    HWND rightPane,
    const PaneLayoutRectangles& rectangles);

}
