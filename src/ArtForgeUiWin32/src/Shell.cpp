#include "ArtForge/UiWin32/Shell.hpp"

#include "ArtForge/UiWin32/PaneLayout.hpp"

#include "ArtForge/Files/ProjectGraph.hpp"
#include "ArtForge/Files/ScopeFiles.hpp"
#include "ArtForge/History/EventLog.hpp"

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
constexpr int NavigationTreeId = 2003;
constexpr int DetailPaneId = 2004;

struct ScopeShellState {
    ArtForge::Core::ScopeShellDescriptor descriptor;
    std::wstring openedPath;
    std::wstring loadStatusText;
    std::wstring loadDetailText;
    HWND navigationTree{};
    HWND summaryControl{};
    HWND detailPane{};
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

HTREEITEM InsertTreeItem(HWND tree, HTREEITEM parent, std::wstring text)
{
    TVINSERTSTRUCTW insert{};
    insert.hParent = parent;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT;
    insert.item.pszText = text.data();
    return TreeView_InsertItem(tree, &insert);
}

std::wstring ReferenceText(std::wstring_view label, const ArtForge::Files::ScopeFileReference& reference)
{
    std::wstring text{label};
    text += L": ";
    text += Utf8ToWide(reference.id);
    if (!reference.path.empty()) {
        text += L" -> ";
        text += Utf8ToWide(reference.path);
    }
    return text;
}

ArtForge::Files::ScopeFileLoadStatus LoadScopeFileStatus(
    ArtForge::Core::ScopeKind scope,
    const std::filesystem::path& path)
{
    switch (scope) {
    case ArtForge::Core::ScopeKind::Solution: {
        const auto graph = ArtForge::Files::LoadSolutionProjectGraph(path);
        const auto graphIssues = ArtForge::Files::FlattenGraphIssues(graph);
        ArtForge::Files::ScopeFileLoadStatus status{graphIssues.empty(), {}};
        for (const auto& issue : graphIssues) {
            status.issues.push_back({issue.path.string() + ": " + issue.message});
        }
        return status;
    }
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

ArtForge::History::HistoryScope ToHistoryScope(ArtForge::Core::ScopeKind scope)
{
    switch (scope) {
    case ArtForge::Core::ScopeKind::Solution:
        return ArtForge::History::HistoryScope::Solution;
    case ArtForge::Core::ScopeKind::Artist:
        return ArtForge::History::HistoryScope::Artist;
    case ArtForge::Core::ScopeKind::Series:
        return ArtForge::History::HistoryScope::Series;
    case ArtForge::Core::ScopeKind::WorkItem:
        return ArtForge::History::HistoryScope::Work;
    case ArtForge::Core::ScopeKind::Fragment:
        return ArtForge::History::HistoryScope::Fragment;
    }

    return ArtForge::History::HistoryScope::Fragment;
}

void RecordFileOperation(
    const ScopeShellState& state,
    ArtForge::History::HistoryOperation operation,
    std::string_view summary,
    std::string_view detail)
{
    if (state.openedPath.empty()) {
        return;
    }

    const auto status = ArtForge::History::RecordFileOperationHistoryEvent(
        std::filesystem::path{state.openedPath},
        ToHistoryScope(state.descriptor.scope),
        operation,
        summary,
        detail);
    (void)status;
}

void PopulateSolutionNavigation(HWND tree, const std::filesystem::path& path)
{
    const auto graph = ArtForge::Files::LoadSolutionProjectGraph(path);
    const auto root = InsertTreeItem(tree, TVI_ROOT, L"Current scope: solution");
    InsertTreeItem(tree, root, L"Path: " + path.wstring());
    InsertTreeItem(tree, root, L"Load status: " + std::wstring{ArtForge::Files::FlattenGraphIssues(graph).empty() ? L"loaded" : L"has issues"});

    const auto artists = InsertTreeItem(tree, root, L"Artists");
    if (graph.artists.empty()) {
        InsertTreeItem(tree, artists, L"(none)");
    }
    for (const auto& artist : graph.artists) {
        const auto artistNode = InsertTreeItem(tree, artists, Utf8ToWide(artist.file.displayName.empty() ? artist.reference.id : artist.file.displayName));
        InsertTreeItem(tree, artistNode, L"Path: " + artist.path.wstring());
        for (const auto& project : artist.projects) {
            const auto projectNode = InsertTreeItem(tree, artistNode, Utf8ToWide(project.file.displayName.empty() ? project.reference.id : project.file.displayName));
            InsertTreeItem(tree, projectNode, L"Path: " + project.path.wstring());
            for (const auto& work : project.works) {
                InsertTreeItem(tree, projectNode, Utf8ToWide(work.file.displayName.empty() ? work.reference.id : work.file.displayName));
            }
        }
    }

    const auto projects = InsertTreeItem(tree, root, L"Solution projects");
    if (graph.projects.empty()) {
        InsertTreeItem(tree, projects, L"(none)");
    }
    for (const auto& project : graph.projects) {
        const auto projectNode = InsertTreeItem(tree, projects, Utf8ToWide(project.file.displayName.empty() ? project.reference.id : project.file.displayName));
        for (const auto& work : project.works) {
            InsertTreeItem(tree, projectNode, Utf8ToWide(work.file.displayName.empty() ? work.reference.id : work.file.displayName));
        }
    }

    TreeView_Expand(tree, root, TVE_EXPAND);
    TreeView_Expand(tree, artists, TVE_EXPAND);
    TreeView_Expand(tree, projects, TVE_EXPAND);
}

void PopulateDirectScopeNavigation(HWND tree, const ScopeShellState& state)
{
    const auto root = InsertTreeItem(tree, TVI_ROOT, L"Current scope: " + std::wstring{ArtForge::Core::ToDisplayName(state.descriptor.scope)});
    InsertTreeItem(tree, root, L"Path: " + (state.openedPath.empty() ? std::wstring{L"(none)"} : state.openedPath));
    InsertTreeItem(tree, root, L"Load status: " + state.loadStatusText);

    if (state.openedPath.empty()) {
        InsertTreeItem(tree, root, L"References: unavailable until a file is opened");
        TreeView_Expand(tree, root, TVE_EXPAND);
        return;
    }

    const std::filesystem::path path{state.openedPath};
    switch (state.descriptor.scope) {
    case ArtForge::Core::ScopeKind::Artist: {
        const auto result = ArtForge::Files::LoadArtistScopeFile(path);
        const auto projects = InsertTreeItem(tree, root, L"Projects");
        if (result.file.projects.empty()) {
            InsertTreeItem(tree, projects, L"(none)");
        }
        for (const auto& project : result.file.projects) {
            InsertTreeItem(tree, projects, ReferenceText(L"project", project));
        }
        TreeView_Expand(tree, projects, TVE_EXPAND);
        break;
    }
    case ArtForge::Core::ScopeKind::Series: {
        const auto result = ArtForge::Files::LoadSeriesScopeFile(path);
        const auto parent = InsertTreeItem(tree, root, L"Parent");
        if (result.file.artist) {
            InsertTreeItem(tree, parent, ReferenceText(L"artist", *result.file.artist));
        } else {
            InsertTreeItem(tree, parent, L"(none)");
        }
        const auto works = InsertTreeItem(tree, root, L"Ordered works");
        if (result.file.works.empty()) {
            InsertTreeItem(tree, works, L"(none)");
        }
        for (const auto& work : result.file.works) {
            InsertTreeItem(tree, works, ReferenceText(L"work", work));
        }
        TreeView_Expand(tree, parent, TVE_EXPAND);
        TreeView_Expand(tree, works, TVE_EXPAND);
        break;
    }
    case ArtForge::Core::ScopeKind::WorkItem: {
        const auto result = ArtForge::Files::LoadWorkScopeFile(path);
        const auto parent = InsertTreeItem(tree, root, L"Parent");
        if (result.file.series) {
            InsertTreeItem(tree, parent, ReferenceText(L"series", *result.file.series));
        } else {
            InsertTreeItem(tree, parent, L"(none)");
        }
        const auto sources = InsertTreeItem(tree, root, L"Sources");
        if (result.file.sources.empty()) {
            InsertTreeItem(tree, sources, L"(none)");
        }
        for (const auto& source : result.file.sources) {
            InsertTreeItem(tree, sources, ReferenceText(L"source", source));
        }
        if (result.file.historyPath) {
            InsertTreeItem(tree, root, L"History: " + Utf8ToWide(*result.file.historyPath));
        }
        TreeView_Expand(tree, parent, TVE_EXPAND);
        TreeView_Expand(tree, sources, TVE_EXPAND);
        break;
    }
    case ArtForge::Core::ScopeKind::Solution:
    case ArtForge::Core::ScopeKind::Fragment:
        InsertTreeItem(tree, root, L"References: not available for this scope");
        break;
    }

    TreeView_Expand(tree, root, TVE_EXPAND);
}

void PopulateNavigationTree(HWND tree, const ScopeShellState& state)
{
    TreeView_DeleteAllItems(tree);
    if (!state.openedPath.empty() && state.descriptor.scope == ArtForge::Core::ScopeKind::Solution) {
        PopulateSolutionNavigation(tree, std::filesystem::path{state.openedPath});
        return;
    }

    PopulateDirectScopeNavigation(tree, state);
}

void UpdateFileStatus(ScopeShellState& state)
{
    if (state.openedPath.empty()) {
        state.loadStatusText = L"No file path provided";
        state.loadDetailText = L"Launch with a scope file path to load status.";
        return;
    }

    RecordFileOperation(state, ArtForge::History::HistoryOperation::FileOpenAttempted, "File open attempted", "");

    const auto loadStatus = LoadScopeFileStatus(
        state.descriptor.scope,
        std::filesystem::path{state.openedPath});

    if (loadStatus.ok) {
        state.loadStatusText = L"File load OK";
        state.loadDetailText = L"Scope file parsed successfully.";
        RecordFileOperation(state, ArtForge::History::HistoryOperation::FileLoadSucceeded, "File load succeeded", "");
        return;
    }

    state.loadStatusText = L"File load failed";
    const auto issueText = FirstIssueText(loadStatus);
    state.loadDetailText = issueText.empty() ? L"Unknown load error." : issueText;
    const auto detail = loadStatus.issues.empty() ? std::string{"Unknown load error."} : loadStatus.issues.front().message;
    RecordFileOperation(state, ArtForge::History::HistoryOperation::FileLoadFailed, "File load failed", detail);
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
    if (state.descriptor.scope == ArtForge::Core::ScopeKind::WorkItem && !state.openedPath.empty()) {
        const auto result = ArtForge::Files::LoadWorkScopeFile(std::filesystem::path{state.openedPath});
        summary += L"\r\nWork domain: ";
        summary += result.file.workDomain.empty() ? L"(unspecified)" : Utf8ToWide(result.file.workDomain);
    }
    summary += L"\r\nArtForge bootstrap OK";
    return summary;
}

std::wstring WorkDomainWorkspaceText(std::string_view workDomain)
{
    if (workDomain == "lyrics") {
        return L"Lyrics/music workspace\r\n\r\nTechnical base placeholder\r\nLyric line table placeholder\r\nRule analytics placeholder";
    }
    if (workDomain == "visualArt") {
        return L"Visual art workspace\r\n\r\nViewer layers placeholder\r\nPaint layers placeholder\r\nLayer analytics placeholder";
    }
    if (workDomain == "scriptStoryboard") {
        return L"Script/storyboard workspace\r\n\r\nScene and block list placeholder\r\nDialogue/action placeholder\r\nStoryboard visualization placeholder";
    }
    if (workDomain.empty()) {
        return L"Unsupported work domain\r\n\r\nNo workDomain field was found in this work file.";
    }
    return L"Unsupported work domain\r\n\r\nDomain value is not supported by the placeholder workspace yet.";
}

std::wstring DetailPaneText(const ScopeShellState& state)
{
    if (state.descriptor.scope == ArtForge::Core::ScopeKind::WorkItem && !state.openedPath.empty()) {
        const auto result = ArtForge::Files::LoadWorkScopeFile(std::filesystem::path{state.openedPath});
        return WorkDomainWorkspaceText(result.file.workDomain);
    }

    std::wstring text;
    text += L"Properties and actions\r\n";
    text += L"\r\nScope: ";
    text += ArtForge::Core::ToDisplayName(state.descriptor.scope);
    text += L"\r\nPath status: ";
    text += state.loadStatusText;
    text += L"\r\n\r\nPlaceholder pane for selected-item details, actions, rule hits, and diagnostics.";
    return text;
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

    const auto rectangles = ArtForge::UiWin32::CalculateThreePaneLayout(client, statusHeight);
    ArtForge::UiWin32::ApplyThreePaneLayout(
        state->navigationTree,
        state->summaryControl,
        state->detailPane,
        rectangles);
}

LRESULT CALLBACK ShellWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE: {
        const auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        const auto state = reinterpret_cast<ScopeShellState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        state->navigationTree = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            WC_TREEVIEWW,
            L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
            0,
            0,
            0,
            0,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(NavigationTreeId)),
            create->hInstance,
            nullptr);
        PopulateNavigationTree(state->navigationTree, *state);

        const auto summary = SummaryText(*state);
        state->summaryControl = CreateWindowExW(
            WS_EX_CLIENTEDGE,
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

        const auto detail = DetailPaneText(*state);
        state->detailPane = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"STATIC",
            detail.c_str(),
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            0,
            0,
            0,
            0,
            window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(DetailPaneId)),
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
    commonControls.dwICC = ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_TREEVIEW_CLASSES;
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
