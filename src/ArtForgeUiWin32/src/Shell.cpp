#include "ArtForge/UiWin32/Shell.hpp"

#include "ArtForge/UiWin32/CommandBar.hpp"
#include "ArtForge/UiWin32/CommonControls.hpp"
#include "ArtForge/UiWin32/PaneLayout.hpp"
#include "ArtForge/UiWin32/PropertyPanel.hpp"
#include "ArtForge/UiWin32/UiMetrics.hpp"

#include "ArtForge/Files/ProjectGraph.hpp"
#include "ArtForge/Files/ScopeFiles.hpp"
#include "ArtForge/History/EventLog.hpp"
#include "ArtForge/Presentation/ScopeNavigationAdapter.hpp"
#include "ArtForge/Presentation/WorkAppPresentationAdapter.hpp"
#include "ArtForge/Prompting/PromptPackage.hpp"
#include "ArtForge/Services/EditCommandService.hpp"
#include "ArtForge/Services/PromptCommandService.hpp"

#include <commctrl.h>
#include <shellapi.h>

#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

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
    ArtForge::Presentation::SelectionModel workSelection;
    ArtForge::Presentation::DirtyStateModel dirtyState;
    std::optional<ArtForge::Prompting::AiExecutionRequest> activeManualAiRequest;
    CommandBar commandBar;
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
        ArtForge::Presentation::TableModel table;
        table.columns = {{"status", "Status"}, {"detail", "Detail"}};
        table.rows.push_back({"missing-path", {"Work file", "No work file path provided"}});
        RenderTableModel(state.domainList, table);
        return;
    }

    const auto presentation = ArtForge::Presentation::BuildWorkAppPresentationModel(std::filesystem::path{state.openedPath});
    RenderTableModel(state.domainList, presentation.domainTable);
}

void AddBottomPanelRow(ScopeShellState& state, std::wstring_view category, std::wstring_view message)
{
    state.bottomList.AddRow({category, message});
}

void PopulateBottomPanel(ScopeShellState& state)
{
    state.bottomList.Clear();
    state.bottomList.AddColumn(0, L"Area", 140);
    state.bottomList.AddColumn(1, L"Message", 720);

    AddBottomPanelRow(state, L"Output", state.loadStatusText);
    AddBottomPanelRow(state, L"Output", state.loadDetailText);
    AddBottomPanelRow(state, L"Tasks", IsOpenedWorkFile(state) ? L"Work commands available through selection state." : L"Open a work file to enable work-item tasks.");

    if (IsOpenedWorkFile(state)) {
        const auto presentation = CurrentWorkPresentation(state);
        AddBottomPanelRow(state, L"Tasks", Utf8ToWide(presentation.manualAiQueue.status));
        for (const auto& diagnostic : presentation.manualAiQueue.diagnostics) {
            AddBottomPanelRow(state, L"Tasks", Utf8ToWide(diagnostic));
        }
        for (const auto& provider : presentation.providerStatus.providers) {
            AddBottomPanelRow(
                state,
                L"Provider",
                Utf8ToWide(provider.providerKind + ": " + provider.configurationStatus + " / " + provider.requestStatus));
            if (!provider.lastDiagnostic.empty()) {
                AddBottomPanelRow(state, L"Provider", Utf8ToWide(provider.lastDiagnostic));
            }
        }
        AddBottomPanelRow(state, L"History", state.openedPath.empty() ? L"No history path." : L"File operation history records are appended next to the opened scope when actions run.");
        return;
    }

    AddBottomPanelRow(state, L"Provider", L"Provider status is available in WorkApp scope.");
    AddBottomPanelRow(state, L"History", state.openedPath.empty() ? L"No opened file." : L"File load history was recorded for this scope.");
}

void PopulatePropertyPanel(ScopeShellState& state)
{
    const auto pathText = state.openedPath.empty() ? std::wstring{L"(none)"} : state.openedPath;

    state.propertyPanel.Clear();
    state.propertyPanel.AddGroup(L"Scope");
    state.propertyPanel.AddProperty(L"Application", state.descriptor.applicationName);
    state.propertyPanel.AddProperty(L"Scope", ArtForge::Core::ToDisplayName(state.descriptor.scope));
    state.propertyPanel.AddProperty(L"Path", pathText);
    state.propertyPanel.AddProperty(L"Load status", state.loadStatusText);
    state.propertyPanel.AddProperty(L"Load detail", state.loadDetailText);

    if (state.descriptor.scope == ArtForge::Core::ScopeKind::WorkItem && !state.openedPath.empty()) {
        const auto presentation = ArtForge::Presentation::BuildWorkAppPresentationModel(
            std::filesystem::path{state.openedPath},
            state.workSelection);
        state.dirtyState = presentation.dirtyState;
        state.propertyPanel.AddGroup(L"Work");
        for (const auto& property : presentation.properties.properties) {
            state.propertyPanel.AddProperty(Utf8ToWide(property.name), Utf8ToWide(property.value));
        }
    }
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

    if (IsOpenedWorkFile(state)) {
        const auto presentation = CurrentWorkPresentation(state);
        canSave = presentation.dirtyState.canSave;
        canBuildPrompt = presentation.promptPreview.available;
        canQueueManualAiTask = presentation.manualAiQueue.available;
        canAcceptSuggestion = false;
        canRejectSuggestion = false;
    }

    return {
        {FileOpenCommand, L"Open", false},
        {FileSaveCommand, L"Save", canSave || IsOpenedWorkFile(state)},
        {FileRefreshCommand, L"Refresh", true},
        {FileBuildPromptCommand, L"Build Prompt", canBuildPrompt},
        {FileQueueManualAiTaskCommand, L"Queue Manual AI Task", canQueueManualAiTask},
        {FileAcceptSuggestionCommand, L"Accept Suggestion", canAcceptSuggestion},
        {FileRejectSuggestionCommand, L"Reject Suggestion", canRejectSuggestion},
    };
}

void RefreshCommandBar(ScopeShellState& state)
{
    const auto buttons = BuildCommandBarButtons(state);
    for (const auto& button : buttons) {
        state.commandBar.SetButtonEnabled(button.commandId, button.enabled);
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
    PopulateNavigationTree(state.navigationTree, state);
    if (state.descriptor.scope == ArtForge::Core::ScopeKind::WorkItem) {
        state.domainList.Clear();
        PopulateWorkDomainList(state);
    } else if (state.summaryControl != nullptr) {
        const auto summary = StartPageText(state);
        SetWindowTextW(state.summaryControl, summary.c_str());
    }
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

void HandleOpenCommand(ScopeShellState& state)
{
    SetStatusText(state, L"Open command placeholder: launch with a scope file path for now.");
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
    AppendMenuW(fileMenu, MF_STRING | MF_GRAYED, FileOpenCommand, L"Open");
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

    const int outputHeight = state->metrics.outputPaneHeight;
    const int outputTop = client.bottom - statusHeight - outputHeight;
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

    const auto documentArea = state->documentTabs.DisplayArea();
    if (state->descriptor.scope == ArtForge::Core::ScopeKind::WorkItem) {
        state->domainList.Move(documentArea);
    } else if (state->summaryControl != nullptr) {
        MoveWindow(
            state->summaryControl,
            documentArea.left,
            documentArea.top,
            documentArea.right - documentArea.left,
            documentArea.bottom - documentArea.top,
            TRUE);
    }

    const auto detailArea = state->detailTabs.DisplayArea();
    state->propertyPanel.Move(detailArea);

    const auto bottomArea = state->bottomTabs.DisplayArea();
    state->bottomList.Move(bottomArea);
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
        state->documentTabs.AddTab(0, L"Start");
        state->documentTabs.AddTab(1, L"Current Scope");
        if (state->descriptor.scope == ArtForge::Core::ScopeKind::WorkItem) {
            state->documentTabs.AddTab(2, L"Work Domain");
            state->documentTabs.AddTab(3, L"Prompt Preview");
            state->documentTabs.AddTab(4, L"Suggestion Review");
        }

        if (state->descriptor.scope == ArtForge::Core::ScopeKind::WorkItem) {
            state->summaryControl = state->domainList.Create(window, SummaryControlId, create->hInstance);
            PopulateWorkDomainList(*state);
        } else {
            const auto summary = StartPageText(*state);
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
            ApplyDefaultGuiFont(state->summaryControl);
        }

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

        LayoutChildren(window);
        return 0;
    }
    case WM_SIZE:
        LayoutChildren(window);
        return 0;
    case WM_TIMER:
        if (wParam == ManualQueuePollTimerId) {
            HandleManualQueuePollTimer(*reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)), window);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == FileOpenCommand) {
            HandleOpenCommand(*reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)));
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
        if (notification != nullptr && notification->idFrom == SummaryControlId && notification->code == LVN_ITEMCHANGED) {
            const auto listChange = reinterpret_cast<NMLISTVIEW*>(lParam);
            if ((listChange->uChanged & LVIF_STATE) != 0) {
                const bool selected = (listChange->uNewState & LVIS_SELECTED) != 0;
                HandleWorkDomainSelection(*reinterpret_cast<ScopeShellState*>(GetWindowLongPtrW(window, GWLP_USERDATA)), selected ? listChange->iItem : -1);
            }
            return 0;
        }
        break;
    }
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
    InitializeCommonControlsForShell();

    RegisterShellWindowClass(instance);

    ScopeShellState state{
        descriptor,
        DefaultShellUiMetrics(),
        FirstCommandLinePath(commandLine),
    };
    UpdateFileStatus(state);

    if (CreateShellWindow(instance, showCommand, state) == nullptr) {
        return 1;
    }

    return RunMessageLoop();
}

}
