#include "ArtForge/UiWin32/PaneLayout.hpp"

namespace ArtForge::UiWin32 {

namespace {

int Width(const RECT& rect)
{
    return rect.right - rect.left;
}

int Height(const RECT& rect)
{
    return rect.bottom - rect.top;
}

void MovePane(HWND pane, const RECT& rect)
{
    if (pane == nullptr) {
        return;
    }

    MoveWindow(
        pane,
        rect.left,
        rect.top,
        Width(rect) < 0 ? 0 : Width(rect),
        Height(rect) < 0 ? 0 : Height(rect),
        TRUE);
}

int MinInt(int left, int right)
{
    return left < right ? left : right;
}

int MaxInt(int left, int right)
{
    return left > right ? left : right;
}

}

PaneLayoutRectangles CalculateThreePaneLayout(
    const RECT& client,
    int bottomReservedHeight,
    const PaneLayoutMetrics& metrics)
{
    const int availableWidth = MaxInt(0, Width(client) - (metrics.margin * 2));
    const int availableHeight = MaxInt(0, Height(client) - bottomReservedHeight - (metrics.margin * 2));
    const int top = client.top + metrics.margin;
    const int bottom = top + availableHeight;

    int leftWidth = MinInt(metrics.leftWidth, availableWidth);
    int rightWidth = availableWidth >= metrics.leftWidth + metrics.gap + metrics.minimumCenterWidth + metrics.gap
        ? MinInt(metrics.rightWidth, availableWidth - metrics.leftWidth - metrics.gap - metrics.minimumCenterWidth - metrics.gap)
        : 0;

    int centerWidth = availableWidth - leftWidth - rightWidth;
    if (rightWidth > 0) {
        centerWidth -= metrics.gap * 2;
    } else if (leftWidth > 0) {
        centerWidth -= metrics.gap;
    }

    if (centerWidth < metrics.minimumCenterWidth && leftWidth > metrics.minimumCenterWidth) {
        const int giveBack = MinInt(leftWidth - metrics.minimumCenterWidth, metrics.minimumCenterWidth - centerWidth);
        leftWidth -= giveBack;
        centerWidth += giveBack;
    }

    const int leftX = client.left + metrics.margin;
    const int centerX = leftX + leftWidth + metrics.gap;
    const int rightX = rightWidth > 0 ? centerX + MaxInt(0, centerWidth) + metrics.gap : client.right - metrics.margin;

    return {
        {leftX, top, leftX + leftWidth, bottom},
        {centerX, top, centerX + MaxInt(0, centerWidth), bottom},
        {rightX, top, rightX + rightWidth, bottom},
    };
}

void ApplyThreePaneLayout(
    HWND leftPane,
    HWND centerPane,
    HWND rightPane,
    const PaneLayoutRectangles& rectangles)
{
    MovePane(leftPane, rectangles.left);
    MovePane(centerPane, rectangles.center);
    MovePane(rightPane, rectangles.right);
}

}
