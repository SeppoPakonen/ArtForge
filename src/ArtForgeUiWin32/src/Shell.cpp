#include "ArtForge/UiWin32/Shell.hpp"

#include "ArtForge/Files/ScopeFiles.hpp"

#include <commctrl.h>
#include <shellapi.h>

#include <filesystem>
#include <memory>
#include <string>

namespace ArtForge::UiWin32 {

namespace {

constexpr wchar_t ShellWindowClassName[] = L"ArtForge.ScopeShell.Window";
constexpr int FileExitCommand = 1001;
constexpr int StatusBarId = 2001;
constexpr int SummaryControlId = 2002;

struct ScopeShellState {
    ArtForge::Core::ScopeShellDescriptor descriptor;
    std::wstring openedPath;
    std::wstring loadStatusText;
    std::wstring loadDetailText;
    HWND summaryControl{};
    HWND statusBar{};
};

std::wstring Utf8ToWide(std::string_view value)
{
    if (value.empty()) {
        return {};
    }

    const auto requiredLength = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (requiredLength <= 0) {
        return std::wstring{value.begin(), value.end()};
    }

    std::wstring converted(static_cast<std::size_t>(requiredLength), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        converted.data(),
        requiredLength);
    return converted;
}

std::wstring FirstCommandLinePath(wchar_t* commandLine)
{
    if (commandLine == nullptr || commandLine[0] == L'\0') {
        return {};
    }

    int argumentCount = 0;
    const auto arguments = CommandLineToArgvW(commandLine, &argumentCount);
    if (arguments == nullptr) {
        return {};
    }

    std::wstring path;
    if (argumentCount > 0) {
        path = arguments[0];
    }

    LocalFree(arguments);
    return path;
}

std::wstring WindowTitle(const ArtForge::Core::ScopeShellDescriptor& descriptor)
{
    std::wstring title{descriptor.applicationName};
    title += L" - ";
    title += descriptor.expectedScope;
    return title;
}

std::wstring FirstIssueText(const ArtForge::Files::ScopeFileLoadStatus& status)
{
    if (status.issues.empty()) {
        return {};
    }

    return Utf8ToWide(status.issues.front().message);
}

ArtForge::Files::ScopeFileLoadStatus LoadScopeFileStatus(
    ArtForge::Core::ScopeKind scope,
    const std::filesystem::path& path)
{
    switch (scope) {
    case ArtForge::Core::ScopeKind::Solution:
        return ArtForge::Files::LoadSolutionScopeFile(path).status;
    case ArtForge::Core::ScopeKind::Artist:
        return ArtForge::Files::LoadArtistScopeFile(path).status;
    case ArtForge::Core::ScopeKind::Series:
        return ArtForge::Files::LoadSeriesScopeFile(path).status;
    case ArtForge::Core::ScopeKind::WorkItem:
        return ArtForge::Files::LoadWorkScopeFile(path).status;
    case ArtForge::Core::ScopeKind::Fragment:
        return {false, {{"fragment scope files are not loadable in this shell yet"}}};
    }

    return {false, {{"unknown scope type"}}};
}

void UpdateFileStatus(ScopeShellState& state)
{
    if (state.openedPath.empty()) {
        state.loadStatusText = L"No file path provided";
        state.loadDetailText = L"Launch with a scope file path to load status.";
        return;
    }

    const auto loadStatus = LoadScopeFileStatus(
        state.descriptor.scope,
        std::filesystem::path{state.openedPath});

    if (loadStatus.ok) {
        state.loadStatusText = L"File load OK";
        state.loadDetailText = L"Scope file parsed successfully.";
        return;
    }

    state.loadStatusText = L"File load failed";
    const auto issueText = FirstIssueText(loadStatus);
    state.loadDetailText = issueText.empty() ? L"Unknown load error." : issueText;
}

std::wstring SummaryText(const ScopeShellState& state)
{
    std::wstring summary;
    summary += L"Application: ";
    summary += state.descriptor.applicationName;
    summary += L"\r\nScope type: ";
    summary += ArtForge::Core::ToDisplayName(state.descriptor.scope);
    summary += L"\r\nExpected scope: ";
    summary += state.descriptor.expectedScope;
    summary += L"\r\nPath: ";
    summary += state.openedPath.empty() ? L"(none)" : state.openedPath;
    summary += L"\r\nLoad status: ";
    summary += state.loadStatusText;
    summary += L"\r\nLoad detail: ";
    summary += state.loadDetailText;
    summary += L"\r\nArtForge bootstrap OK";
    return summary;
}

HMENU CreateShellMenu()
{
    const auto menu = CreateMenu();
    const auto fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, FileExitCommand, L"Exit");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");
    return menu;
}

void LayoutChildren(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);

    const auto state = reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (state == nullptr) {
        return;
    }

    int statusHeight = 0;
    if (state->statusBar != nullptr) {
        SendMessageW(state->statusBar, WM_SIZE, 0, 0);
        RECT statusRect{};
        GetWindowRect(state->statusBar, &statusRect);
        statusHeight = statusRect.bottom - statusRect.top;
    }

    if (state->summaryControl != nullptr) {
        MoveWindow(
            state->summaryControl,
            12,
            12,
            client.right - client.left - 24,
            client.bottom - client.top - statusHeight - 24,
            TRUE);
    }
}

LRESULT CALLBACK ShellWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE: {
        const auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        const auto state = reinterpret_cast<ScopeShellState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        const auto summary = SummaryText(*state);
        state->summaryControl = CreateWindowExW(
            0,
            L"STATIC",
            summary.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0,
            0,
            0,
            0,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(SummaryControlId)),
            create->hInstance,
            nullptr);

        state->statusBar = CreateWindowExW(
            0,
            STATUSCLASSNAMEW,
            state->loadStatusText.c_str(),
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0,
            0,
            0,
            0,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(StatusBarId)),
            create->hInstance,
            nullptr);

        LayoutChildren(window);
        return 0;
    }
    case WM_SIZE:
        LayoutChildren(window);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == FileExitCommand) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(window, message, wParam, lParam);
}

ATOM RegisterShellWindowClass(HINSTANCE instance)
{
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = ShellWindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = ShellWindowClassName;
    windowClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    return RegisterClassExW(&windowClass);
}

HWND CreateShellWindow(HINSTANCE instance, int showCommand, ScopeShellState& state)
{
    const auto title = WindowTitle(state.descriptor);
    const auto window = CreateWindowExW(
        0,
        ShellWindowClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        720,
        420,
        nullptr,
        CreateShellMenu(),
        instance,
        &state);

    if (window != nullptr) {
        ShowWindow(window, showCommand);
        UpdateWindow(window);
    }

    return window;
}

int RunMessageLoop()
{
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}

}

int RunScopeShell(
    HINSTANCE instance,
    int showCommand,
    ArtForge::Core::ScopeShellDescriptor descriptor,
    wchar_t* commandLine)
{
    INITCOMMONCONTROLSEX commonControls{};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&commonControls);

    RegisterShellWindowClass(instance);

    ScopeShellState state{
        descriptor,
        FirstCommandLinePath(commandLine),
    };
    UpdateFileStatus(state);

    if (CreateShellWindow(instance, showCommand, state) == nullptr) {
        return 1;
    }

    return RunMessageLoop();
}

}
