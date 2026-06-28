#include "ArtForge/UiWin32/Shell.hpp"

#include "ArtForge/UiWin32/CommandBar.hpp"
#include "ArtForge/UiWin32/CommonControls.hpp"
#include "ArtForge/UiWin32/PaneLayout.hpp"
#include "ArtForge/UiWin32/PropertyPanel.hpp"
#include "ArtForge/UiWin32/UiMetrics.hpp"

#include "ArtForge/Files/ProjectGraph.hpp"
#include "ArtForge/Files/RecentFiles.hpp"
#include "ArtForge/Files/ScopeFiles.hpp"
#include "ArtForge/History/EventLog.hpp"
#include "ArtForge/Presentation/ScopeNavigationAdapter.hpp"
#include "ArtForge/Presentation/WorkAppPresentationAdapter.hpp"
#include "ArtForge/Prompting/PromptPackage.hpp"
#include "ArtForge/Services/EditCommandService.hpp"
#include "ArtForge/Services/PromptCommandService.hpp"

#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <windowsx.h>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "Comdlg32.lib")

#include <charconv>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <ctime>

namespace ArtForge::UiWin32 {

namespace {

constexpr wchar_t ShellWindowClassName[] = L"ArtForge.ScopeShell.Window";
constexpr int FileOpenCommand = 1000;
constexpr int FileSaveCommand = 1001;
constexpr int FileExitCommand = 1002;
constexpr int FileQueueManualAiTaskCommand = 1003;
constexpr int FileRefreshCommand = 1004;
constexpr int FileBuildPromptCommand = 1005;
constexpr int FileAcceptSuggestionCommand = 1006;
constexpr int FileRejectSuggestionCommand = 1007;
constexpr int ManualQueuePollTimerId = 3001;
constexpr int StatusBarId = 2001;
constexpr int SummaryControlId = 2002;
constexpr int NavigationTreeId = 2003;
constexpr int DetailTabId = 2004;
constexpr int DetailListId = 2005;
constexpr int CommandBarId = 2006;
constexpr int DocumentTabId = 2007;
constexpr int BottomTabId = 2008;
constexpr int BottomListId = 2009;

enum class ActiveSplitter {
    None,
    Left,
    Right,
    Bottom,
};

struct StartPageAction {
    std::filesystem::path path;
    ArtForge::Files::RecentScopeType scope{ArtForge::Files::RecentScopeType::Unknown};
    std::wstring label;
    std::wstring source;
};

struct ScopeShellState {
    ArtForge::Core::ScopeShellDescriptor descriptor;
    ShellUiMetrics metrics{DefaultShellUiMetrics()};
    std::wstring openedPath;
    std::wstring loadStatusText;
    std::wstring loadDetailText;
    HWND navigationTree{};
    HWND summaryControl{};
    ListViewReport domainList;
    TabControl documentTabs;
    TabControl detailTabs;
    TabControl bottomTabs;
    ListViewReport bottomList;
    PropertyPanel propertyPanel;
    std::vector<StartPageAction> startActions;
    ArtForge::Presentation::SelectionModel workSelection;
    ArtForge::Presentation::DirtyStateModel dirtyState;
    std::optional<ArtForge::Prompting::AiExecutionRequest> activeManualAiRequest;
    CommandBar commandBar;
    HWND statusBar{};
    ActiveSplitter activeSplitter{ActiveSplitter::None};
    RECT leftSplitter{};
    RECT rightSplitter{};
    RECT bottomSplitter{};
    std::filesystem::path settingsPath;
    RECT restoredWindowRect{CW_USEDEFAULT, CW_USEDEFAULT, 720, 420};
    bool hasRestoredWindowRect{false};
    bool restoreMaximized{false};
    int restoredBottomTab{0};
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

std::string WideToNarrowAscii(std::wstring_view value)
{
    std::string narrowed;
    narrowed.reserve(value.size());
    for (const auto character : value) {
        narrowed.push_back(static_cast<char>(character));
    }
    return narrowed;
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

int ClampInt(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

std::wstring WindowTitle(const ArtForge::Core::ScopeShellDescriptor& descriptor)
{
    std::wstring title{descriptor.applicationName};
    title += L" - ";
    title += descriptor.expectedScope;
    return title;
}

std::string WideToUtf8(std::wstring_view value)
{
    if (value.empty()) {
        return {};
    }

    const auto requiredLength = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (requiredLength <= 0) {
        return WideToNarrowAscii(value);
    }

    std::string converted(static_cast<std::size_t>(requiredLength), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        converted.data(),
        requiredLength,
        nullptr,
        nullptr);
    return converted;
}

std::wstring SanitizedFileStem(std::wstring_view value)
{
    std::wstring stem;
    for (const auto character : value) {
        if ((character >= L'a' && character <= L'z')
            || (character >= L'A' && character <= L'Z')
            || (character >= L'0' && character <= L'9')
            || character == L'-'
            || character == L'_') {
            stem.push_back(character);
        } else if (character == L' ') {
            stem.push_back(L'-');
        }
    }
    return stem.empty() ? L"shell" : stem;
}

std::filesystem::path UserShellLayoutSettingsPath(const ArtForge::Core::ScopeShellDescriptor& descriptor)
{
    wchar_t* appData = nullptr;
    std::size_t appDataLength = 0;
    if (_wdupenv_s(&appData, &appDataLength, L"APPDATA") != 0 || appData == nullptr) {
        return {};
    }

    std::filesystem::path path{appData};
    free(appData);
    path /= L"ArtForge";
    path /= L"settings";
    path /= L"shell-layout-" + SanitizedFileStem(descriptor.applicationName) + L".json";
    return path;
}

std::optional<int> ReadIntSetting(std::string_view json, std::string_view key)
{
    const std::string field = "\"" + std::string{key} + "\"";
    const auto fieldPosition = json.find(field);
    if (fieldPosition == std::string_view::npos) {
        return std::nullopt;
    }
    const auto colonPosition = json.find(':', fieldPosition + field.size());
    if (colonPosition == std::string_view::npos) {
        return std::nullopt;
    }
    auto valuePosition = colonPosition + 1;
    while (valuePosition < json.size() && std::isspace(static_cast<unsigned char>(json[valuePosition])) != 0) {
        ++valuePosition;
    }
    auto endPosition = valuePosition;
    if (endPosition < json.size() && json[endPosition] == '-') {
        ++endPosition;
    }
    while (endPosition < json.size() && std::isdigit(static_cast<unsigned char>(json[endPosition])) != 0) {
        ++endPosition;
    }
    if (endPosition == valuePosition) {
        return std::nullopt;
    }
    int value = 0;
    const auto result = std::from_chars(json.data() + valuePosition, json.data() + endPosition, value);
    if (result.ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

bool ReadBoolSetting(std::string_view json, std::string_view key)
{
    const std::string field = "\"" + std::string{key} + "\"";
    const auto fieldPosition = json.find(field);
    if (fieldPosition == std::string_view::npos) {
        return false;
    }
    const auto colonPosition = json.find(':', fieldPosition + field.size());
    if (colonPosition == std::string_view::npos) {
        return false;
    }
    auto valuePosition = colonPosition + 1;
    while (valuePosition < json.size() && std::isspace(static_cast<unsigned char>(json[valuePosition])) != 0) {
        ++valuePosition;
    }
    return json.substr(valuePosition, 4) == "true";
}

void LoadShellLayoutSettings(ScopeShellState& state)
{
    state.settingsPath = UserShellLayoutSettingsPath(state.descriptor);
    if (state.settingsPath.empty()) {
        return;
    }

    std::ifstream input{state.settingsPath};
    if (!input) {
        return;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const auto json = buffer.str();

    state.metrics.navigationWidth = ClampInt(ReadIntSetting(json, "explorerWidth").value_or(state.metrics.navigationWidth), 160, 700);
    state.metrics.inspectorWidth = ClampInt(ReadIntSetting(json, "inspectorWidth").value_or(state.metrics.inspectorWidth), 160, 700);
    state.metrics.outputPaneHeight = ClampInt(ReadIntSetting(json, "bottomPanelHeight").value_or(state.metrics.outputPaneHeight), 72, 400);
    state.restoredBottomTab = ClampInt(ReadIntSetting(json, "selectedBottomTab").value_or(0), 0, 3);
    state.restoreMaximized = ReadBoolSetting(json, "maximized");

    const auto left = ReadIntSetting(json, "windowLeft");
    const auto top = ReadIntSetting(json, "windowTop");
    const auto width = ReadIntSetting(json, "windowWidth");
    const auto height = ReadIntSetting(json, "windowHeight");
    if (left && top && width && height && *width >= 480 && *height >= 320) {
        state.restoredWindowRect = {*left, *top, *width, *height};
        state.hasRestoredWindowRect = true;
    }
}

void SaveShellLayoutSettings(HWND window, const ScopeShellState& state)
{
    if (state.settingsPath.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(state.settingsPath.parent_path(), error);
    if (error) {
        return;
    }

    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    GetWindowPlacement(window, &placement);

    RECT normal = placement.rcNormalPosition;
    std::ofstream output{state.settingsPath, std::ios::trunc};
    if (!output) {
        return;
    }

    output << "{\n";
    output << "  \"format\": \"artforge.user.shell-layout\",\n";
    output << "  \"application\": \"" << WideToUtf8(state.descriptor.applicationName) << "\",\n";
    output << "  \"windowLeft\": " << normal.left << ",\n";
    output << "  \"windowTop\": " << normal.top << ",\n";
    output << "  \"windowWidth\": " << (normal.right - normal.left) << ",\n";
    output << "  \"windowHeight\": " << (normal.bottom - normal.top) << ",\n";
    output << "  \"maximized\": " << (placement.showCmd == SW_SHOWMAXIMIZED ? "true" : "false") << ",\n";
    output << "  \"explorerWidth\": " << state.metrics.navigationWidth << ",\n";
    output << "  \"inspectorWidth\": " << state.metrics.inspectorWidth << ",\n";
    output << "  \"bottomPanelHeight\": " << state.metrics.outputPaneHeight << ",\n";
    output << "  \"selectedBottomTab\": " << state.bottomTabs.SelectedIndex() << "\n";
    output << "}\n";
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

ArtForge::Files::RecentScopeType ToRecentScopeType(ArtForge::Core::ScopeKind scope)
{
    switch (scope) {
    case ArtForge::Core::ScopeKind::Solution:
        return ArtForge::Files::RecentScopeType::Solution;
    case ArtForge::Core::ScopeKind::Artist:
        return ArtForge::Files::RecentScopeType::Artist;
    case ArtForge::Core::ScopeKind::Series:
        return ArtForge::Files::RecentScopeType::Series;
    case ArtForge::Core::ScopeKind::WorkItem:
        return ArtForge::Files::RecentScopeType::Work;
    case ArtForge::Core::ScopeKind::Fragment:
        return ArtForge::Files::RecentScopeType::Unknown;
    }
    return ArtForge::Files::RecentScopeType::Unknown;
}

const wchar_t* OpenDialogFilter(ArtForge::Core::ScopeKind scope)
{
    switch (scope) {
    case ArtForge::Core::ScopeKind::Solution:
        return L"ArtForge solution files (*.afsolution.json)\0*.afsolution.json\0JSON files (*.json)\0*.json\0All files (*.*)\0*.*\0";
    case ArtForge::Core::ScopeKind::Artist:
        return L"ArtForge artist files (*.afartist.json)\0*.afartist.json\0JSON files (*.json)\0*.json\0All files (*.*)\0*.*\0";
    case ArtForge::Core::ScopeKind::Series:
        return L"ArtForge series files (*.afseries.json)\0*.afseries.json\0JSON files (*.json)\0*.json\0All files (*.*)\0*.*\0";
    case ArtForge::Core::ScopeKind::WorkItem:
        return L"ArtForge work files (*.afwork.json)\0*.afwork.json\0JSON files (*.json)\0*.json\0All files (*.*)\0*.*\0";
    case ArtForge::Core::ScopeKind::Fragment:
        return L"ArtForge files (*.json)\0*.json\0All files (*.*)\0*.*\0";
    }
    return L"ArtForge files (*.json)\0*.json\0All files (*.*)\0*.*\0";
}

std::string CurrentUtcTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time);
    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
    return buffer;
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

void RecordPresentationCommand(
    const ScopeShellState& state,
    ArtForge::History::HistoryOperation operation,
    std::string commandName,
    std::string resultStatus)
{
    if (state.openedPath.empty()) {
        return;
    }

    const auto work = ArtForge::Files::LoadWorkScopeFile(std::filesystem::path{state.openedPath});
    const auto status = ArtForge::History::RecordPresentationCommandHistoryEvent(
        std::filesystem::path{state.openedPath},
        operation,
        {
            work.file.id,
            std::filesystem::path{state.openedPath}.generic_string(),
            state.workSelection.domain,
            state.workSelection.itemType,
            state.workSelection.itemId,
            state.workSelection.itemIndex,
            std::move(commandName),
            std::move(resultStatus),
        });
    (void)status;
}

void RecordChangeSetCommand(
    const ScopeShellState& state,
    ArtForge::History::HistoryOperation operation,
    std::string commandId,
    int changeCount,
    std::string summary)
{
    if (state.openedPath.empty()) {
        return;
    }

    const auto work = ArtForge::Files::LoadWorkScopeFile(std::filesystem::path{state.openedPath});
    const auto status = ArtForge::History::RecordChangeSetHistoryEvent(
        std::filesystem::path{state.openedPath},
        operation,
        {
            work.file.id,
            std::filesystem::path{state.openedPath}.generic_string(),
            state.workSelection.domain,
            state.workSelection.itemType,
            state.workSelection.itemId,
            state.workSelection.itemIndex,
            std::move(commandId),
            changeCount,
            std::move(summary),
        });
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

    const auto model = ArtForge::Presentation::BuildScopeNavigationModel(
        state.descriptor.scope,
        std::filesystem::path{state.openedPath},
        WideToNarrowAscii(state.loadStatusText),
        WideToNarrowAscii(state.loadDetailText));

    const auto renderNode = [&](const ArtForge::Presentation::NavigationTreeNodeModel& node, HTREEITEM parent, const auto& renderRef) -> HTREEITEM {
        const auto item = InsertTreeItem(tree, parent, Utf8ToWide(node.label));
        for (const auto& child : node.children) {
            renderRef(child, item, renderRef);
        }
        TreeView_Expand(tree, item, TVE_EXPAND);
        return item;
    };

    for (const auto& root : model.roots) {
        renderNode(root, TVI_ROOT, renderNode);
    }
}

void UpdateFileStatus(ScopeShellState& state)
{
    if (state.openedPath.empty()) {
        state.loadStatusText = L"No file loaded";
        state.loadDetailText = L"Use Open or choose a compatible Recent/Example row to load a scope file.";
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
    summary += state.descriptor.applicationName;
    summary += L"\r\n";
    summary += L"\r\nApplication: ";
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

std::wstring StartPageText(const ScopeShellState& state)
{
    std::wstring text;
    text += state.descriptor.applicationName;
    text += L"\r\n";
    text += state.descriptor.expectedScope;
    text += L"\r\n";
    text += L"\r\nCurrent scope: ";
    text += ArtForge::Core::ToDisplayName(state.descriptor.scope);
    text += L"\r\nPath: ";
    text += state.openedPath.empty() ? L"(none)" : state.openedPath;
    text += L"\r\nLoad status: ";
    text += state.loadStatusText;
    text += L"\r\nLoad detail: ";
    text += state.loadDetailText;
    text += L"\r\n";
    if (state.openedPath.empty()) {
        text += L"\r\nOpen a matching ArtForge scope file by launching this app with a file path.";
        text += L"\r\nUse the command bar for refresh, prompt, queue, and suggestion-review actions as they become available.";
    } else {
        text += L"\r\nReview the navigation explorer, current scope tab, inspector, and output panels for loaded-file state.";
    }
    text += L"\r\n";
    text += L"\r\nArtForge bootstrap OK";
    return text;
}

void RenderTableModel(ListViewReport& list, const ArtForge::Presentation::TableModel& table)
{
    for (std::size_t index = 0; index < table.columns.size(); ++index) {
        list.AddColumn(static_cast<int>(index), Utf8ToWide(table.columns[index].label), index == 0 ? 160 : 240);
    }

    for (const auto& row : table.rows) {
        std::vector<std::wstring> wideCells;
        wideCells.reserve(row.cells.size());
        for (const auto& cell : row.cells) {
            wideCells.push_back(Utf8ToWide(cell));
        }

        std::vector<std::wstring_view> views;
        views.reserve(wideCells.size());
        for (const auto& cell : wideCells) {
            views.push_back(cell);
        }
        list.AddRow(views);
    }
}

bool IsOpenedWorkFile(const ScopeShellState& state);
ArtForge::Presentation::WorkAppPresentationModel CurrentWorkPresentation(const ScopeShellState& state);

void PopulateWorkDomainList(ScopeShellState& state)
{
    if (state.openedPath.empty()) {
        return;
    }

    const auto presentation = ArtForge::Presentation::BuildWorkAppPresentationModel(std::filesystem::path{state.openedPath});
    RenderTableModel(state.domainList, presentation.domainTable);
}

bool IsCompatibleScope(const ScopeShellState& state, ArtForge::Files::RecentScopeType scope)
{
    return scope == ToRecentScopeType(state.descriptor.scope);
}

void PopulateStartPageActions(ScopeShellState& state)
{
    state.startActions.clear();
    state.domainList.Clear();
    state.domainList.AddColumn(0, L"Source", 120);
    state.domainList.AddColumn(1, L"Name", 220);
    state.domainList.AddColumn(2, L"Path", 520);
    state.domainList.AddRow({L"Open", L"Open a file...", L"Use the Open command, or double-click a compatible Recent/Example row."});

    const auto recent = ArtForge::Files::LoadRecentFiles(ArtForge::Files::DefaultRecentFilesPath());
    for (const auto& entry : recent.entries) {
        if (!IsCompatibleScope(state, entry.scope)) {
            continue;
        }
        state.startActions.push_back({
            entry.path,
            entry.scope,
            Utf8ToWide(entry.displayName.empty() ? entry.path.filename().string() : entry.displayName),
            L"Recent",
        });
        state.domainList.AddRow({L"Recent", state.startActions.back().label, entry.path.wstring()});
    }

    const auto examples = ArtForge::Files::BuildExampleFileIndex(std::filesystem::current_path());
    for (const auto& entry : examples) {
        if (!entry.exists || !IsCompatibleScope(state, entry.scope)) {
            continue;
        }
        state.startActions.push_back({
            entry.path,
            entry.scope,
            Utf8ToWide(entry.displayName),
            L"Example",
        });
        state.domainList.AddRow({L"Example", state.startActions.back().label, entry.path.wstring()});
    }

    if (state.startActions.empty()) {
        state.domainList.AddRow({L"Status", L"No compatible recent files", L"Use Open to choose a matching ArtForge scope file."});
    }
}

void PopulateCentralList(ScopeShellState& state)
{
    state.domainList.Clear();
    if (state.descriptor.scope == ArtForge::Core::ScopeKind::WorkItem && !state.openedPath.empty()) {
        PopulateWorkDomainList(state);
        return;
    }

    if (state.openedPath.empty()) {
        PopulateStartPageActions(state);
        return;
    }

    state.domainList.AddColumn(0, L"Field", 180);
    state.domainList.AddColumn(1, L"Value", 700);
    state.domainList.AddRow({L"Application", state.descriptor.applicationName});
    state.domainList.AddRow({L"Scope type", ArtForge::Core::ToDisplayName(state.descriptor.scope)});
    state.domainList.AddRow({L"Path", state.openedPath});
    state.domainList.AddRow({L"Load status", state.loadStatusText});
    state.domainList.AddRow({L"Load detail", state.loadDetailText});
}

void RebuildDocumentTabs(ScopeShellState& state)
{
    state.documentTabs.Clear();
    state.documentTabs.AddTab(0, state.openedPath.empty() ? L"Start" : L"Overview");

    switch (state.descriptor.scope) {
    case ArtForge::Core::ScopeKind::Solution:
        state.documentTabs.AddTab(1, L"Scope Graph");
        state.documentTabs.AddTab(2, L"Diagnostics");
        break;
    case ArtForge::Core::ScopeKind::Artist:
        state.documentTabs.AddTab(1, L"Artist Profile");
        state.documentTabs.AddTab(2, L"Works Overview");
        state.documentTabs.AddTab(3, L"Diagnostics");
        break;
    case ArtForge::Core::ScopeKind::Series:
        state.documentTabs.AddTab(1, L"Series Overview");
        state.documentTabs.AddTab(2, L"Ordered Works");
        state.documentTabs.AddTab(3, L"Diagnostics");
        break;
    case ArtForge::Core::ScopeKind::WorkItem:
        state.documentTabs.AddTab(1, L"Work Domain");
        state.documentTabs.AddTab(2, L"Prompt Preview");
        state.documentTabs.AddTab(3, L"Suggestion Review");
        state.documentTabs.AddTab(4, L"Diagnostics");
        break;
    case ArtForge::Core::ScopeKind::Fragment:
        state.documentTabs.AddTab(1, L"Fragment Overview");
        state.documentTabs.AddTab(2, L"Diagnostics");
        break;
    }
    state.documentTabs.SetSelectedIndex(0);
}

void AddBottomPanelRow(
    ScopeShellState& state,
    std::wstring_view severity,
    std::wstring_view category,
    std::wstring_view message)
{
    state.bottomList.AddRow({severity, category, message});
}

void PopulateBottomPanel(ScopeShellState& state)
{
    state.bottomList.Clear();
    state.bottomList.AddColumn(0, L"Severity", 90);
    state.bottomList.AddColumn(1, L"Area", 140);
    state.bottomList.AddColumn(2, L"Message", 720);

    const bool loadFailed = state.loadStatusText.find(L"failed") != std::wstring::npos;
    AddBottomPanelRow(state, loadFailed ? L"Error" : L"Info", L"Output", state.loadStatusText);
    AddBottomPanelRow(state, loadFailed ? L"Error" : L"Info", L"Output", state.loadDetailText);
    if (state.openedPath.empty()) {
        AddBottomPanelRow(state, L"Info", L"Tasks", L"Open a matching scope file or choose a Recent/Example row.");
    } else if (loadFailed) {
        AddBottomPanelRow(state, L"Error", L"Tasks", L"Resolve the file load or schema diagnostic before editing.");
    } else {
        AddBottomPanelRow(state, L"Info", L"Tasks", IsOpenedWorkFile(state) ? L"Work commands available through selection state." : L"Scope file loaded read-only.");
    }

    if (IsOpenedWorkFile(state)) {
        const auto presentation = CurrentWorkPresentation(state);
        AddBottomPanelRow(state, L"Info", L"Tasks", Utf8ToWide(presentation.manualAiQueue.status));
        for (const auto& diagnostic : presentation.manualAiQueue.diagnostics) {
            AddBottomPanelRow(state, L"Warning", L"Tasks", Utf8ToWide(diagnostic));
        }
        for (const auto& provider : presentation.providerStatus.providers) {
            const bool configured = provider.configurationStatus.find("configured") != std::string::npos
                && provider.configurationStatus.find("notConfigured") == std::string::npos;
            AddBottomPanelRow(
                state,
                configured ? L"Info" : L"Warning",
                L"Provider",
                Utf8ToWide(provider.providerKind + ": " + provider.configurationStatus + " / " + provider.requestStatus));
            if (!provider.lastDiagnostic.empty()) {
                AddBottomPanelRow(state, L"Warning", L"Provider", Utf8ToWide(provider.lastDiagnostic));
            }
        }
        AddBottomPanelRow(state, L"Info", L"History", state.openedPath.empty() ? L"No history path." : L"File operation history records are appended next to the opened scope when actions run.");
        return;
    }

        AddBottomPanelRow(state, L"Info", L"Provider", L"Provider status appears here when a work file is loaded.");
    AddBottomPanelRow(state, L"Info", L"History", state.openedPath.empty() ? L"No opened file." : L"File load history was recorded for this scope.");
}

void PopulatePropertyPanel(ScopeShellState& state)
{
    const auto pathText = state.openedPath.empty() ? std::wstring{L"(none)"} : state.openedPath;

    state.propertyPanel.Clear();
    state.propertyPanel.AddGroup(L"Scope");
    state.propertyPanel.AddProperty(L"Application", state.descriptor.applicationName);
    state.propertyPanel.AddProperty(L"Scope", ArtForge::Core::ToDisplayName(state.descriptor.scope));
    state.propertyPanel.AddProperty(L"Expected scope", state.descriptor.expectedScope);

    state.propertyPanel.AddGroup(L"Selection");
    state.propertyPanel.AddProperty(L"Selected item", state.workSelection.hasSelection ? Utf8ToWide(state.workSelection.displayLabel) : L"No item selected");
    state.propertyPanel.AddProperty(L"Selected domain", Utf8ToWide(state.workSelection.domain));
    state.propertyPanel.AddProperty(L"Selected type", Utf8ToWide(state.workSelection.itemType));

    state.propertyPanel.AddGroup(L"File");
    state.propertyPanel.AddProperty(L"Path", pathText);
    state.propertyPanel.AddProperty(L"Load status", state.loadStatusText);

    if (state.descriptor.scope == ArtForge::Core::ScopeKind::WorkItem && !state.openedPath.empty()) {
        const auto presentation = ArtForge::Presentation::BuildWorkAppPresentationModel(
            std::filesystem::path{state.openedPath},
            state.workSelection);
        state.dirtyState = presentation.dirtyState;
        for (const auto& property : presentation.properties.properties) {
            if (property.name.find("Provider ") == 0 || property.name == "Provider status") {
                continue;
            }
            if (property.name.find("diagnostic") != std::string::npos || property.name.find("Diagnostic") != std::string::npos) {
                continue;
            }
            if (property.name.find("Selected") == 0 || property.name == "Selection") {
                continue;
            }
            state.propertyPanel.AddProperty(Utf8ToWide(property.name), Utf8ToWide(property.value));
        }

        state.propertyPanel.AddGroup(L"Provider");
        for (const auto& provider : presentation.providerStatus.providers) {
            state.propertyPanel.AddProperty(
                Utf8ToWide(provider.providerKind),
                Utf8ToWide(provider.configurationStatus + " / " + provider.requestStatus));
            if (!provider.lastDiagnostic.empty()) {
                state.propertyPanel.AddProperty(Utf8ToWide(provider.providerKind + " diagnostic"), Utf8ToWide(provider.lastDiagnostic));
            }
        }

        state.propertyPanel.AddGroup(L"Diagnostics");
        if (presentation.status.diagnostics.empty()
            && presentation.promptPreview.diagnostics.empty()
            && presentation.manualAiQueue.diagnostics.empty()
            && presentation.pendingSuggestionReview.diagnostics.empty()) {
            state.propertyPanel.AddProperty(L"Diagnostics", L"No diagnostics.");
        }
        for (const auto& diagnostic : presentation.status.diagnostics) {
            state.propertyPanel.AddProperty(L"Status diagnostic", Utf8ToWide(diagnostic));
        }
        for (const auto& diagnostic : presentation.promptPreview.diagnostics) {
            state.propertyPanel.AddProperty(L"Prompt diagnostic", Utf8ToWide(diagnostic));
        }
        for (const auto& diagnostic : presentation.manualAiQueue.diagnostics) {
            state.propertyPanel.AddProperty(L"Queue diagnostic", Utf8ToWide(diagnostic));
        }
        for (const auto& diagnostic : presentation.pendingSuggestionReview.diagnostics) {
            state.propertyPanel.AddProperty(L"Suggestion diagnostic", Utf8ToWide(diagnostic));
        }
        return;
    }

    state.propertyPanel.AddGroup(L"Provider");
    state.propertyPanel.AddProperty(L"Provider status", L"Available in WorkApp scope.");

    state.propertyPanel.AddGroup(L"Diagnostics");
    state.propertyPanel.AddProperty(L"Load detail", state.loadDetailText);
}

void SetStatusText(ScopeShellState& state, std::wstring text)
{
    state.loadStatusText = std::move(text);
    if (state.statusBar != nullptr) {
        SendMessageW(state.statusBar, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(state.loadStatusText.c_str()));
    }
}

std::filesystem::path DefaultManualQueueRoot()
{
    char* userProfile = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&userProfile, &length, "USERPROFILE") == 0 && userProfile != nullptr) {
        std::filesystem::path path{userProfile};
        free(userProfile);
        return path / "Documents" / "ArtForge" / "ai-queue";
    }
    return std::filesystem::path{"ArtForge"} / "ai-queue";
}

std::string DefaultTargetField(std::string_view domain, std::string_view itemType)
{
    if (domain == "lyrics" && itemType == "lyricLine") {
        return "text";
    }
    if (domain == "visualArt" && itemType == "visualLayer") {
        return "intent";
    }
    if (domain == "scriptStoryboard" && itemType == "scriptBlock") {
        return "text";
    }
    return "content";
}

bool IsOpenedWorkFile(const ScopeShellState& state)
{
    return state.descriptor.scope == ArtForge::Core::ScopeKind::WorkItem && !state.openedPath.empty();
}

ArtForge::Presentation::WorkAppPresentationModel CurrentWorkPresentation(const ScopeShellState& state)
{
    return ArtForge::Presentation::BuildWorkAppPresentationModel(
        std::filesystem::path{state.openedPath},
        state.workSelection);
}

std::vector<CommandBarButtonSpec> BuildCommandBarButtons(const ScopeShellState& state)
{
    bool canSave = false;
    bool canBuildPrompt = false;
    bool canQueueManualAiTask = false;
    bool canAcceptSuggestion = false;
    bool canRejectSuggestion = false;
    std::wstring saveReason = L"Open a dirty work file to save.";
    std::wstring promptReason = L"Select a work item row before building a prompt.";
    std::wstring queueReason = L"Select a work item row before queuing a manual AI task.";
    std::wstring suggestionReason = L"No pending suggestion is available.";

    if (IsOpenedWorkFile(state)) {
        const auto presentation = CurrentWorkPresentation(state);
        canSave = presentation.dirtyState.canSave && presentation.dirtyState.isDirty;
        canBuildPrompt = state.workSelection.hasSelection && presentation.promptPreview.available;
        canQueueManualAiTask = state.workSelection.hasSelection && presentation.manualAiQueue.available;
        canAcceptSuggestion = presentation.pendingSuggestionReview.acceptCommand.enabled;
        canRejectSuggestion = presentation.pendingSuggestionReview.rejectCommand.enabled;
        saveReason = presentation.dirtyState.isDirty ? L"Save is unavailable." : L"No unsaved work changes.";
        promptReason = state.workSelection.hasSelection ? L"Prompt preview is unavailable." : L"Select a work item row before building a prompt.";
        queueReason = state.workSelection.hasSelection ? L"Manual AI queue is unavailable." : L"Select a work item row before queuing a manual AI task.";
        suggestionReason = presentation.pendingSuggestionReview.status.empty()
            ? L"No pending suggestion is available."
            : Utf8ToWide(presentation.pendingSuggestionReview.status);
    }

    return {
        {FileOpenCommand, L"Open", true, true, L"Open a matching ArtForge scope file."},
        {FileSaveCommand, L"Save", canSave, true, saveReason},
        {FileRefreshCommand, L"Refresh", true, true, L"Reload current shell state."},
        {FileBuildPromptCommand, L"Build Prompt", canBuildPrompt, true, promptReason},
        {FileQueueManualAiTaskCommand, L"Queue Manual AI Task", canQueueManualAiTask, true, queueReason},
        {FileAcceptSuggestionCommand, L"Accept Suggestion", canAcceptSuggestion, true, suggestionReason},
        {FileRejectSuggestionCommand, L"Reject Suggestion", canRejectSuggestion, true, suggestionReason},
    };
}

void RefreshCommandBar(ScopeShellState& state)
{
    const auto buttons = BuildCommandBarButtons(state);
    for (const auto& button : buttons) {
        state.commandBar.SetButtonEnabled(button.commandId, button.enabled);
        state.commandBar.SetButtonVisible(button.commandId, button.visible);
    }

    const auto window = GetParent(state.commandBar.Window());
    const auto menu = window == nullptr ? nullptr : GetMenu(window);
    if (menu != nullptr) {
        for (const auto& button : buttons) {
            EnableMenuItem(
                menu,
                static_cast<UINT>(button.commandId),
                MF_BYCOMMAND | (button.enabled ? MF_ENABLED : MF_GRAYED));
        }
        DrawMenuBar(window);
    }
}

void HandleQueueManualAiTaskCommand(ScopeShellState& state)
{
    if (state.descriptor.scope != ArtForge::Core::ScopeKind::WorkItem || state.openedPath.empty()) {
        SetStatusText(state, L"Manual AI queue is available for work files only.");
        return;
    }
    if (!state.workSelection.hasSelection) {
        SetStatusText(state, L"Select a work item row before queuing a manual AI task.");
        return;
    }

    const std::filesystem::path workPath{state.openedPath};
    const std::string operation = "manual-queue-selected-item";
    const auto prompt = ArtForge::Services::BuildSelectedItemPromptCommand({
        workPath,
        state.workSelection.domain,
        state.workSelection.itemType,
        state.workSelection.itemId,
        state.workSelection.itemIndex,
        operation,
    });

    ArtForge::Prompting::AiExecutionRequest execution;
    execution.providerKind = ArtForge::Prompting::AiProviderKind::ManualQueue;
    execution.queueRoot = DefaultManualQueueRoot();
    execution.promptPackagePath = workPath;
    execution.promptPackageSummary = "Selected item prompt package";
    execution.resultSchemaPath = "docs/prompting/ai-result-contract.md";
    execution.requestedOperation = operation;
    execution.target.workPath = workPath;
    execution.target.domain = state.workSelection.domain;
    execution.target.itemType = state.workSelection.itemType;
    execution.target.itemId = state.workSelection.itemId;
    execution.target.itemIndex = state.workSelection.itemIndex;
    execution.target.field = DefaultTargetField(state.workSelection.domain, state.workSelection.itemType);

    const auto result = ArtForge::Prompting::WriteManualAiQueueRequest({
        execution,
        prompt.promptDebugDump,
    });
    if (result.ok) {
        state.activeManualAiRequest = result.execution;
        if (state.summaryControl != nullptr) {
            SetTimer(GetParent(state.summaryControl), ManualQueuePollTimerId, 1000, nullptr);
        }
    }
    SetStatusText(state, result.ok ? L"Manual AI task queued." : L"Manual AI task queue failed.");
    state.loadDetailText = Utf8ToWide(ArtForge::Prompting::DescribeManualAiQueueWriteResult(result));
    PopulatePropertyPanel(state);
    RefreshCommandBar(state);
    PopulateBottomPanel(state);
}

void HandleManualQueuePollTimer(ScopeShellState& state, HWND window)
{
    if (!state.activeManualAiRequest) {
        KillTimer(window, ManualQueuePollTimerId);
        return;
    }

    const auto result = ArtForge::Prompting::PollManualAiQueueResult({
        *state.activeManualAiRequest,
        true,
    });
    state.loadDetailText = Utf8ToWide(ArtForge::Prompting::DescribeManualAiQueuePollResult(result));
    SetStatusText(state, Utf8ToWide("Manual queue poll: " + std::string{ArtForge::Prompting::ToDisplayName(result.providerResult.status)}));
    PopulatePropertyPanel(state);
    PopulateBottomPanel(state);

    if (result.providerResult.status != ArtForge::Prompting::AiExecutionStatus::WaitingForResult) {
        state.activeManualAiRequest.reset();
        KillTimer(window, ManualQueuePollTimerId);
    }
}

void HandleSaveCommand(ScopeShellState& state)
{
    if (state.descriptor.scope != ArtForge::Core::ScopeKind::WorkItem) {
        SetStatusText(state, L"Save is available for work files only.");
        return;
    }

    RecordChangeSetCommand(state, ArtForge::History::HistoryOperation::SaveRequested, "save-work-document", 0, "Save requested");

    ArtForge::Services::DirtyState serviceDirty;
    serviceDirty.isDirty = state.dirtyState.isDirty;
    serviceDirty.canSave = state.dirtyState.canSave;
    serviceDirty.path = std::filesystem::path{state.openedPath}.generic_string();
    serviceDirty.pendingChangeCount = 0;

    const auto result = ArtForge::Services::SaveWorkDocumentCommand({
        std::filesystem::path{state.openedPath},
        serviceDirty,
    });

    state.dirtyState.isDirty = result.dirtyState.isDirty;
    state.dirtyState.canSave = result.dirtyState.canSave;
    state.dirtyState.saveFailed = !result.command.status.ok;
    state.dirtyState.label = result.dirtyState.isDirty ? "Dirty" : "Clean";
    state.dirtyState.detail = result.command.status.summary;
    state.loadDetailText = Utf8ToWide(result.command.debugSummary);
    SetStatusText(state, Utf8ToWide(result.command.status.summary));
    RecordChangeSetCommand(
        state,
        result.command.status.ok ? ArtForge::History::HistoryOperation::SaveSucceeded : ArtForge::History::HistoryOperation::SaveFailed,
        "save-work-document",
        result.dirtyState.pendingChangeCount,
        result.command.status.summary);
    PopulatePropertyPanel(state);
    RefreshCommandBar(state);
    PopulateBottomPanel(state);
}

void HandleRefreshCommand(ScopeShellState& state)
{
    UpdateFileStatus(state);
    RebuildDocumentTabs(state);
    PopulateNavigationTree(state.navigationTree, state);
    PopulateCentralList(state);
    PopulatePropertyPanel(state);
    RefreshCommandBar(state);
    SetStatusText(state, L"Shell refreshed.");
    PopulateBottomPanel(state);
}

void HandleBuildPromptCommand(ScopeShellState& state)
{
    if (!IsOpenedWorkFile(state) || !state.workSelection.hasSelection) {
        SetStatusText(state, L"Select a work item row before building a prompt.");
        return;
    }

    const auto result = ArtForge::Services::BuildSelectedItemPromptCommand({
        std::filesystem::path{state.openedPath},
        state.workSelection.domain,
        state.workSelection.itemType,
        state.workSelection.itemId,
        state.workSelection.itemIndex,
        "toolbar-build-prompt",
    });
    state.loadDetailText = Utf8ToWide(result.command.debugSummary);
    SetStatusText(state, Utf8ToWide(result.command.status.summary));
    PopulatePropertyPanel(state);
    PopulateBottomPanel(state);
    RecordPresentationCommand(
        state,
        ArtForge::History::HistoryOperation::PromptPreviewRequested,
        "toolbar-build-prompt",
        result.command.status.ok ? "preview-ready" : "preview-failed");
}

std::optional<std::wstring> ShowOpenFileDialog(HWND owner, const ScopeShellState& state)
{
    wchar_t pathBuffer[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = OpenDialogFilter(state.descriptor.scope);
    dialog.lpstrFile = pathBuffer;
    dialog.nMaxFile = MAX_PATH;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    dialog.lpstrTitle = L"Open ArtForge scope file";

    if (GetOpenFileNameW(&dialog) == TRUE) {
        return std::wstring{pathBuffer};
    }
    return std::nullopt;
}

void RefreshLoadedState(ScopeShellState& state)
{
    RebuildDocumentTabs(state);
    PopulateNavigationTree(state.navigationTree, state);
    state.workSelection = {};
    PopulateCentralList(state);
    PopulatePropertyPanel(state);
    RefreshCommandBar(state);
    PopulateBottomPanel(state);
}

void UpdateRecentFilesAfterOpen(ScopeShellState& state)
{
    const auto recentPath = ArtForge::Files::DefaultRecentFilesPath();
    auto recent = ArtForge::Files::LoadRecentFiles(recentPath);
    auto entries = ArtForge::Files::AddOrPromoteRecentFile(
        std::move(recent.entries),
        {
            std::filesystem::absolute(std::filesystem::path{state.openedPath}),
            ToRecentScopeType(state.descriptor.scope),
            std::filesystem::path{state.openedPath}.filename().string(),
            CurrentUtcTimestamp(),
            true,
        });
    const auto save = ArtForge::Files::SaveRecentFiles(recentPath, entries);
    if (!save.ok) {
        const auto detail = save.diagnostics.empty() ? std::string{"Recent files could not be saved."} : save.diagnostics.front();
        state.loadDetailText = Utf8ToWide(detail);
    }
}

void HandleOpenCommand(HWND window, ScopeShellState& state)
{
    const auto path = ShowOpenFileDialog(window, state);
    if (!path) {
        SetStatusText(state, L"Open canceled.");
        return;
    }

    state.openedPath = *path;
    UpdateFileStatus(state);
    RefreshLoadedState(state);

    if (state.loadStatusText == L"File load OK") {
        UpdateRecentFilesAfterOpen(state);
        SetStatusText(state, L"File opened.");
    } else {
        SetStatusText(state, L"File open failed.");
    }
}

void HandleStartPageAction(ScopeShellState& state, int rowIndex)
{
    const int actionIndex = rowIndex - 1;
    if (actionIndex < 0 || static_cast<std::size_t>(actionIndex) >= state.startActions.size()) {
        SetStatusText(state, L"Use Open to choose a matching ArtForge scope file.");
        return;
    }

    const auto action = state.startActions[static_cast<std::size_t>(actionIndex)];
    if (!IsCompatibleScope(state, action.scope)) {
        SetStatusText(state, L"Selected entry is not compatible with this scope app.");
        return;
    }
    if (!std::filesystem::exists(action.path)) {
        state.loadStatusText = L"File load failed";
        state.loadDetailText = L"Recent/example file is missing: " + action.path.wstring();
        PopulateBottomPanel(state);
        SetStatusText(state, L"Selected file is missing.");
        return;
    }

    state.openedPath = action.path.wstring();
    UpdateFileStatus(state);
    RefreshLoadedState(state);
    if (state.loadStatusText == L"File load OK") {
        UpdateRecentFilesAfterOpen(state);
        SetStatusText(state, L"File opened.");
    } else {
        SetStatusText(state, L"File open failed.");
    }
}

void HandleSuggestionCommandPlaceholder(ScopeShellState& state)
{
    SetStatusText(state, L"Suggestion toolbar command is disabled until pending suggestion routing is exposed.");
}

void HandleWorkDomainSelection(ScopeShellState& state, int rowIndex)
{
    if (state.descriptor.scope != ArtForge::Core::ScopeKind::WorkItem || state.openedPath.empty()) {
        return;
    }

    state.workSelection = rowIndex >= 0
        ? ArtForge::Presentation::SelectWorkAppTableRow(std::filesystem::path{state.openedPath}, rowIndex)
        : ArtForge::Presentation::ClearWorkAppSelection(std::filesystem::path{state.openedPath});
    PopulatePropertyPanel(state);
    RecordPresentationCommand(
        state,
        ArtForge::History::HistoryOperation::PresentationSelectionChanged,
        "selection-changed",
        state.workSelection.hasSelection ? "selected" : "cleared");
    if (state.workSelection.hasSelection) {
        RecordPresentationCommand(
            state,
            ArtForge::History::HistoryOperation::PromptPreviewRequested,
            "prompt-preview",
            "preview-ready");
    }
    RefreshCommandBar(state);
    PopulateBottomPanel(state);
}

HMENU CreateShellMenu()
{
    const auto menu = CreateMenu();
    const auto fileMenu = CreatePopupMenu();
    AppendMenuW(fileMenu, MF_STRING, FileOpenCommand, L"Open");
    AppendMenuW(fileMenu, MF_STRING, FileSaveCommand, L"Save");
    AppendMenuW(fileMenu, MF_STRING, FileRefreshCommand, L"Refresh");
    AppendMenuW(fileMenu, MF_STRING, FileBuildPromptCommand, L"Build Prompt");
    AppendMenuW(fileMenu, MF_STRING, FileQueueManualAiTaskCommand, L"Queue Manual AI Task");
    AppendMenuW(fileMenu, MF_STRING | MF_GRAYED, FileAcceptSuggestionCommand, L"Accept Suggestion");
    AppendMenuW(fileMenu, MF_STRING | MF_GRAYED, FileRejectSuggestionCommand, L"Reject Suggestion");
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

    RECT commandBarRect{
        client.left,
        client.top,
        client.right,
        client.top + state->metrics.toolbarHeight,
    };
    state->commandBar.Move(commandBarRect);

    RECT contentRect = client;
    contentRect.top += state->metrics.toolbarHeight;

    const int minimumBottomHeight = 72;
    const int availableClientHeight = client.bottom - client.top - state->metrics.toolbarHeight - statusHeight - (state->metrics.margin * 2);
    const int outputHeight = ClampInt(state->metrics.outputPaneHeight, minimumBottomHeight, availableClientHeight / 2);
    state->metrics.outputPaneHeight = outputHeight;
    const int outputTop = client.bottom - statusHeight - outputHeight;
    state->bottomSplitter = {
        client.left + state->metrics.margin,
        outputTop - state->metrics.splitterWidth,
        client.right - state->metrics.margin,
        outputTop,
    };
    RECT bottomPanelRect{
        client.left + state->metrics.margin,
        outputTop,
        client.right - state->metrics.margin,
        client.bottom - statusHeight - state->metrics.gap,
    };
    state->bottomTabs.Move(bottomPanelRect);

    const auto rectangles = ArtForge::UiWin32::CalculateThreePaneLayout(
        contentRect,
        statusHeight + outputHeight,
        ArtForge::UiWin32::ToPaneLayoutMetrics(state->metrics));
    ArtForge::UiWin32::ApplyThreePaneLayout(
        state->navigationTree,
        state->documentTabs.Window(),
        state->detailTabs.Window(),
        rectangles);

    state->leftSplitter = {
        rectangles.left.right,
        rectangles.left.top,
        rectangles.center.left,
        rectangles.left.bottom,
    };
    state->rightSplitter = {
        rectangles.center.right,
        rectangles.right.top,
        rectangles.right.left,
        rectangles.right.bottom,
    };

    const auto documentArea = state->documentTabs.DisplayArea();
    state->domainList.Move(documentArea);

    const auto detailArea = state->detailTabs.DisplayArea();
    state->propertyPanel.Move(detailArea);

    const auto bottomArea = state->bottomTabs.DisplayArea();
    state->bottomList.Move(bottomArea);
}

bool ContainsPoint(const RECT& rect, POINT point)
{
    return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
}

ActiveSplitter HitTestSplitter(const ScopeShellState& state, POINT point)
{
    if (ContainsPoint(state.leftSplitter, point)) {
        return ActiveSplitter::Left;
    }
    if (ContainsPoint(state.rightSplitter, point)) {
        return ActiveSplitter::Right;
    }
    if (ContainsPoint(state.bottomSplitter, point)) {
        return ActiveSplitter::Bottom;
    }
    return ActiveSplitter::None;
}

void SetSplitterCursor(ActiveSplitter splitter)
{
    if (splitter == ActiveSplitter::Bottom) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZENS));
        return;
    }
    if (splitter == ActiveSplitter::Left || splitter == ActiveSplitter::Right) {
        SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
        return;
    }
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
}

void UpdateSplitterDrag(HWND window, ScopeShellState& state, POINT point)
{
    RECT client{};
    GetClientRect(window, &client);

    int statusHeight = 0;
    if (state.statusBar != nullptr) {
        RECT statusRect{};
        GetWindowRect(state.statusBar, &statusRect);
        statusHeight = statusRect.bottom - statusRect.top;
    }

    const int minimumSideWidth = 160;
    const int minimumBottomHeight = 72;
    const int availableWidth = client.right - client.left - (state.metrics.margin * 2);
    const int maximumSideWidth = (availableWidth - state.metrics.minimumDocumentWidth - (state.metrics.gap * 2)) / 2;
    const int sideMax = maximumSideWidth > minimumSideWidth ? maximumSideWidth : minimumSideWidth;

    switch (state.activeSplitter) {
    case ActiveSplitter::Left:
        state.metrics.navigationWidth = ClampInt(point.x - client.left - state.metrics.margin, minimumSideWidth, sideMax);
        break;
    case ActiveSplitter::Right:
        state.metrics.inspectorWidth = ClampInt(client.right - state.metrics.margin - point.x, minimumSideWidth, sideMax);
        break;
    case ActiveSplitter::Bottom: {
        const int bottomEdge = client.bottom - statusHeight;
        const int availableHeight = bottomEdge - state.metrics.toolbarHeight - (state.metrics.margin * 2);
        const int bottomMax = availableHeight > minimumBottomHeight ? availableHeight / 2 : minimumBottomHeight;
        state.metrics.outputPaneHeight = ClampInt(bottomEdge - point.y, minimumBottomHeight, bottomMax);
        break;
    }
    case ActiveSplitter::None:
        break;
    }

    LayoutChildren(window);
}

LRESULT CALLBACK ShellWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_CREATE: {
        const auto create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        const auto state = reinterpret_cast<ScopeShellState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

        state->commandBar.Create(window, CommandBarId, create->hInstance);
        state->commandBar.SetButtons(BuildCommandBarButtons(*state), create->hInstance);

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
        ApplyDefaultGuiFont(state->navigationTree);
        PopulateNavigationTree(state->navigationTree, *state);

        state->documentTabs.Create(window, DocumentTabId, create->hInstance);
        RebuildDocumentTabs(*state);

        state->summaryControl = state->domainList.Create(window, SummaryControlId, create->hInstance);
        PopulateCentralList(*state);

        state->detailTabs.Create(window, DetailTabId, create->hInstance);
        state->detailTabs.AddTab(0, L"Inspector");
        state->detailTabs.AddTab(1, L"Prompt preview");
        state->detailTabs.AddTab(2, L"Suggestion review");
        state->detailTabs.AddTab(3, L"Provider status");

        state->bottomTabs.Create(window, BottomTabId, create->hInstance);
        state->bottomTabs.AddTab(0, L"Output");
        state->bottomTabs.AddTab(1, L"Tasks");
        state->bottomTabs.AddTab(2, L"Provider");
        state->bottomTabs.AddTab(3, L"History");
        state->bottomTabs.SetSelectedIndex(state->restoredBottomTab);
        state->bottomList.Create(window, BottomListId, create->hInstance);
        PopulateBottomPanel(*state);

        state->propertyPanel.Create(window, DetailListId, create->hInstance);
        PopulatePropertyPanel(*state);

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
        ApplyDefaultGuiFont(state->statusBar);

        RefreshCommandBar(*state);
        LayoutChildren(window);
        return 0;
    }
    case WM_SIZE:
        LayoutChildren(window);
        return 0;
    case WM_SETCURSOR: {
        if (LOWORD(lParam) == HTCLIENT) {
            const auto state = reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
            if (state != nullptr) {
                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(window, &point);
                const auto splitter = state->activeSplitter != ActiveSplitter::None
                    ? state->activeSplitter
                    : HitTestSplitter(*state, point);
                if (splitter != ActiveSplitter::None) {
                    SetSplitterCursor(splitter);
                    return TRUE;
                }
            }
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        const auto state = reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (state != nullptr) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            state->activeSplitter = HitTestSplitter(*state, point);
            if (state->activeSplitter != ActiveSplitter::None) {
                SetCapture(window);
                SetSplitterCursor(state->activeSplitter);
                return 0;
            }
        }
        break;
    }
    case WM_MOUSEMOVE: {
        const auto state = reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (state != nullptr) {
            POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            if (state->activeSplitter != ActiveSplitter::None) {
                UpdateSplitterDrag(window, *state, point);
                SetSplitterCursor(state->activeSplitter);
                return 0;
            }
            SetSplitterCursor(HitTestSplitter(*state, point));
        }
        break;
    }
    case WM_LBUTTONUP: {
        const auto state = reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (state != nullptr && state->activeSplitter != ActiveSplitter::None) {
            state->activeSplitter = ActiveSplitter::None;
            ReleaseCapture();
            return 0;
        }
        break;
    }
    case WM_TIMER:
        if (wParam == ManualQueuePollTimerId) {
            HandleManualQueuePollTimer(*reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)), window);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == FileOpenCommand) {
            HandleOpenCommand(window, *reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)));
            return 0;
        }
        if (LOWORD(wParam) == FileSaveCommand) {
            HandleSaveCommand(*reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)));
            return 0;
        }
        if (LOWORD(wParam) == FileRefreshCommand) {
            HandleRefreshCommand(*reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)));
            return 0;
        }
        if (LOWORD(wParam) == FileBuildPromptCommand) {
            HandleBuildPromptCommand(*reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)));
            return 0;
        }
        if (LOWORD(wParam) == FileQueueManualAiTaskCommand) {
            HandleQueueManualAiTaskCommand(*reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)));
            return 0;
        }
        if (LOWORD(wParam) == FileAcceptSuggestionCommand || LOWORD(wParam) == FileRejectSuggestionCommand) {
            HandleSuggestionCommandPlaceholder(*reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)));
            return 0;
        }
        if (LOWORD(wParam) == FileExitCommand) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_NOTIFY: {
        const auto notification = reinterpret_cast<NMHDR*>(lParam);
        auto& state = *reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (notification != nullptr && notification->idFrom == SummaryControlId && notification->code == LVN_ITEMCHANGED) {
            const auto listChange = reinterpret_cast<NMLISTVIEW*>(lParam);
            if (state.descriptor.scope == ArtForge::Core::ScopeKind::WorkItem
                && !state.openedPath.empty()
                && (listChange->uChanged & LVIF_STATE) != 0) {
                const bool selected = (listChange->uNewState & LVIS_SELECTED) != 0;
                HandleWorkDomainSelection(state, selected ? listChange->iItem : -1);
            }
            return 0;
        }
        if (notification != nullptr && notification->idFrom == SummaryControlId && notification->code == NM_DBLCLK) {
            const auto item = reinterpret_cast<NMITEMACTIVATE*>(lParam);
            if (item != nullptr && item->iItem >= 0 && state.openedPath.empty()) {
                HandleStartPageAction(state, item->iItem);
            }
            return 0;
        }
        break;
    }
    case WM_DESTROY: {
        const auto state = reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (state != nullptr) {
            SaveShellLayoutSettings(window, *state);
        }
        PostQuitMessage(0);
        return 0;
    }
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
    const int x = state.hasRestoredWindowRect ? state.restoredWindowRect.left : CW_USEDEFAULT;
    const int y = state.hasRestoredWindowRect ? state.restoredWindowRect.top : CW_USEDEFAULT;
    const int width = state.hasRestoredWindowRect ? state.restoredWindowRect.right : 720;
    const int height = state.hasRestoredWindowRect ? state.restoredWindowRect.bottom : 420;
    const auto window = CreateWindowExW(
        0,
        ShellWindowClassName,
        title.c_str(),
        WS_OVERLAPPEDWINDOW,
        x,
        y,
        width,
        height,
        nullptr,
        CreateShellMenu(),
        instance,
        &state);

    if (window != nullptr) {
        ShowWindow(window, state.restoreMaximized ? SW_SHOWMAXIMIZED : showCommand);
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
    InitializeCommonControlsForShell();

    RegisterShellWindowClass(instance);

    ScopeShellState state{
        descriptor,
        DefaultShellUiMetrics(),
        FirstCommandLinePath(commandLine),
    };
    LoadShellLayoutSettings(state);
    UpdateFileStatus(state);

    if (CreateShellWindow(instance, showCommand, state) == nullptr) {
        return 1;
    }

    return RunMessageLoop();
}

}
