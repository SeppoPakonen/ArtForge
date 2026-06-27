#include "ArtForge/Presentation/WorkAppPresentationAdapter.hpp"

#include "ArtForge/Files/DomainWorkViewModels.hpp"
#include "ArtForge/Files/ScopeFiles.hpp"
#include "ArtForge/Services/PromptCommandService.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>

namespace ArtForge::Presentation {

namespace {

void AddDiagnostic(StatusModel& status, std::string message)
{
    status.ok = false;
    status.diagnostics.push_back(std::move(message));
}

void AddProperty(PropertyListModel& properties, std::string name, std::string value)
{
    properties.properties.push_back({std::move(name), std::move(value)});
}

NavigationTreeModel BuildNavigation(const ArtForge::Files::WorkScopeFileLoadResult& result, const std::filesystem::path& workPath)
{
    NavigationTreeNodeModel root;
    root.id = "work";
    root.label = "Current scope: work item";
    root.children.push_back({"path", "Path: " + workPath.generic_string(), {}});
    root.children.push_back({"load", std::string{"Load status: "} + (result.status.ok ? "loaded" : "failed"), {}});
    root.children.push_back({"domain", "Work domain: " + (result.file.workDomain.empty() ? std::string{"(unspecified)"} : result.file.workDomain), {}});

    NavigationTreeNodeModel parent;
    parent.id = "parent";
    parent.label = "Parent";
    if (result.file.series) {
        parent.children.push_back({"series", "series: " + result.file.series->id + " -> " + result.file.series->path, {}});
    } else {
        parent.children.push_back({"none", "(none)", {}});
    }
    root.children.push_back(std::move(parent));

    NavigationTreeNodeModel sources;
    sources.id = "sources";
    sources.label = "Sources";
    if (result.file.sources.empty()) {
        sources.children.push_back({"none", "(none)", {}});
    }
    for (const auto& source : result.file.sources) {
        sources.children.push_back({source.id, "source: " + source.id + " -> " + source.path, {}});
    }
    root.children.push_back(std::move(sources));

    if (result.file.historyPath) {
        root.children.push_back({"history", "History: " + *result.file.historyPath, {}});
    }

    return {{std::move(root)}};
}

PropertyListModel BuildBaseProperties(const ArtForge::Files::WorkScopeFileLoadResult& result, const std::filesystem::path& workPath)
{
    PropertyListModel properties;
    properties.title = "Work";
    AddProperty(properties, "Path", workPath.generic_string());
    AddProperty(properties, "Load status", result.status.ok ? "File load OK" : "File load failed");
    AddProperty(properties, "Work id", result.file.id);
    AddProperty(properties, "Display name", result.file.displayName);
    AddProperty(properties, "Work kind", result.file.workKind);
    AddProperty(properties, "Work domain", result.file.workDomain.empty() ? "(unspecified)" : result.file.workDomain);
    return properties;
}

void AddLyricsSelectionProperties(
    PropertyListModel& properties,
    const std::filesystem::path& workPath,
    const SelectionModel& selection)
{
    const auto model = ArtForge::Files::LoadLyricsWorkViewModel(workPath);
    const auto index = selection.itemIndex >= 0 ? static_cast<std::size_t>(selection.itemIndex) : model.lyricLines.size();
    if (index >= model.lyricLines.size()) {
        return;
    }

    const auto& line = model.lyricLines[index];
    AddProperty(properties, "Domain", "lyrics");
    AddProperty(properties, "Section", line.sectionId);
    AddProperty(properties, "Time", line.timeRange);
    AddProperty(properties, "Line text", line.text);
    AddProperty(properties, "Line", line.id + " / " + std::to_string(selection.itemIndex));
}

void AddVisualSelectionProperties(
    PropertyListModel& properties,
    const std::filesystem::path& workPath,
    const SelectionModel& selection)
{
    const auto model = ArtForge::Files::LoadVisualArtWorkViewModel(workPath);
    const auto index = selection.itemIndex >= 0 ? static_cast<std::size_t>(selection.itemIndex) : model.viewerLayers.size() + model.paintLayers.size();

    const ArtForge::Files::VisualArtLayerView* layer = nullptr;
    if (index < model.viewerLayers.size()) {
        layer = &model.viewerLayers[index];
    } else {
        const auto paintIndex = index - model.viewerLayers.size();
        if (paintIndex < model.paintLayers.size()) {
            layer = &model.paintLayers[paintIndex];
        }
    }

    if (layer == nullptr) {
        return;
    }

    AddProperty(properties, "Domain", "visualArt");
    AddProperty(properties, "Layer type", layer->layerType);
    AddProperty(properties, "Label", layer->label);
    AddProperty(properties, "Intent", layer->intent);
    AddProperty(properties, "Priority", layer->priority.empty() ? "(none)" : layer->priority);
    AddProperty(properties, "Status", layer->status.empty() ? "(none)" : layer->status);
}

void AddScriptSelectionProperties(
    PropertyListModel& properties,
    const std::filesystem::path& workPath,
    const SelectionModel& selection)
{
    const auto model = ArtForge::Files::LoadScriptStoryboardWorkViewModel(workPath);
    const auto rowIndex = selection.itemIndex >= 0 ? static_cast<std::size_t>(selection.itemIndex) : model.scenes.size() + model.blocks.size();

    if (rowIndex < model.scenes.size()) {
        const auto& scene = model.scenes[rowIndex];
        AddProperty(properties, "Domain", "scriptStoryboard");
        AddProperty(properties, "Scene", scene.id);
        AddProperty(properties, "Time range", scene.timeRange);
        AddProperty(properties, "Speaker / type", "scene");
        AddProperty(properties, "Summary", scene.title);
        return;
    }

    const auto blockIndex = rowIndex - model.scenes.size();
    if (blockIndex >= model.blocks.size()) {
        return;
    }

    const auto& block = model.blocks[blockIndex];
    AddProperty(properties, "Domain", "scriptStoryboard");
    AddProperty(properties, "Scene / block", block.sceneId + " / " + block.id);
    AddProperty(properties, "Time range", block.timeRange);
    AddProperty(properties, "Speaker / type", block.speaker.empty() ? block.kind : block.speaker + " / " + block.kind);
    AddProperty(properties, "Dialogue / action", block.text);
}

void AddSelectionProperties(PropertyListModel& properties, const std::filesystem::path& workPath, const SelectionModel& selection)
{
    if (!selection.hasSelection) {
        AddProperty(properties, "Selection", "(none)");
        return;
    }

    AddProperty(properties, "Selected domain", selection.domain);
    AddProperty(properties, "Selected item type", selection.itemType);
    AddProperty(properties, "Selected item id", selection.itemId);
    AddProperty(properties, "Selected index", std::to_string(selection.itemIndex));
    AddProperty(properties, "Selected item", selection.displayLabel);

    if (selection.domain == "lyrics") {
        AddLyricsSelectionProperties(properties, workPath, selection);
    } else if (selection.domain == "visualArt") {
        AddVisualSelectionProperties(properties, workPath, selection);
    } else if (selection.domain == "scriptStoryboard") {
        AddScriptSelectionProperties(properties, workPath, selection);
    }
}

PromptPreviewModel BuildPromptPreview(const std::filesystem::path& workPath, const SelectionModel& selection)
{
    PromptPreviewModel preview;
    preview.operation = "Preview selected item prompt";
    if (!selection.hasSelection) {
        preview.diagnostics.push_back("Select a work item row to preview a prompt package.");
        return preview;
    }

    const auto command = ArtForge::Services::BuildSelectedItemPromptCommand({
        workPath,
        selection.domain,
        selection.itemType,
        selection.itemId,
        selection.itemIndex,
        preview.operation,
    });

    preview.available = command.command.status.ok;
    preview.selectedItemSummary = selection.itemType + " " + selection.itemId + " (" + selection.domain + ")";
    preview.layerSummaries = {
        "selected domain item JSON",
        "selected item request Markdown",
        command.command.debugSummary,
    };
    preview.outputContract = "selected item output contract JSON";
    for (const auto& diagnostic : command.command.status.diagnostics) {
        preview.diagnostics.push_back(diagnostic.message);
    }
    return preview;
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

std::string SafeSlug(std::string value)
{
    std::string slug;
    bool previousDash = false;
    for (auto character : value) {
        const bool alphaNumeric =
            (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9');
        if (alphaNumeric) {
            slug += static_cast<char>(character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character);
            previousDash = false;
        } else if (!previousDash && !slug.empty()) {
            slug += '-';
            previousDash = true;
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug.empty() ? "ai-task" : slug;
}

ManualAiQueueModel BuildManualAiQueueModel(const std::filesystem::path& workPath, const SelectionModel& selection)
{
    ManualAiQueueModel model;
    model.queueRoot = DefaultManualQueueRoot().generic_string();
    model.status = "Select a work item row to queue a manual AI task.";
    if (!selection.hasSelection) {
        model.diagnostics.push_back("No selected item.");
        return model;
    }

    const std::string operation = "manual-queue-selected-item";
    const auto prompt = ArtForge::Services::BuildSelectedItemPromptCommand({
        workPath,
        selection.domain,
        selection.itemType,
        selection.itemId,
        selection.itemIndex,
        operation,
    });
    model.available = prompt.command.status.ok;
    model.copyPastePromptText = prompt.promptDebugDump;
    const auto stem = "next-" + SafeSlug(selection.domain + "-" + operation);
    model.requestFile = (DefaultManualQueueRoot() / (stem + ".request.json")).generic_string();
    model.promptTextFile = (DefaultManualQueueRoot() / (stem + ".prompt.txt")).generic_string();
    model.expectedResultFile = (DefaultManualQueueRoot() / (stem + ".result.json")).generic_string();
    model.status = model.available ? "Ready to queue manual AI task." : "Manual queue prompt is not available.";
    for (const auto& diagnostic : prompt.command.status.diagnostics) {
        model.diagnostics.push_back(diagnostic.message);
    }
    return model;
}

void AddPromptPreviewProperties(PropertyListModel& properties, const PromptPreviewModel& preview)
{
    AddProperty(properties, "Prompt preview", preview.available ? "available" : "not available");
    if (!preview.selectedItemSummary.empty()) {
        AddProperty(properties, "Preview item", preview.selectedItemSummary);
    }
    AddProperty(properties, "Operation", preview.operation);
    for (std::size_t index = 0; index < preview.layerSummaries.size(); ++index) {
        AddProperty(properties, "Prompt layer " + std::to_string(index + 1), preview.layerSummaries[index]);
    }
    if (!preview.outputContract.empty()) {
        AddProperty(properties, "Output schema", preview.outputContract);
    }
    for (std::size_t index = 0; index < preview.diagnostics.size(); ++index) {
        AddProperty(properties, "Prompt diagnostic " + std::to_string(index + 1), preview.diagnostics[index]);
    }
}

void AddManualAiQueueProperties(PropertyListModel& properties, const ManualAiQueueModel& queue)
{
    AddProperty(properties, "Manual AI queue", queue.available ? "available" : "not available");
    AddProperty(properties, "Manual queue root", queue.queueRoot);
    AddProperty(properties, "Manual queue status", queue.status);
    if (!queue.requestFile.empty()) {
        AddProperty(properties, "Manual request file", queue.requestFile);
    }
    if (!queue.promptTextFile.empty()) {
        AddProperty(properties, "Manual prompt text file", queue.promptTextFile);
    }
    if (!queue.expectedResultFile.empty()) {
        AddProperty(properties, "Manual expected result", queue.expectedResultFile);
    }
    if (!queue.copyPastePromptText.empty()) {
        AddProperty(properties, "Copy-paste prompt text", queue.copyPastePromptText);
    }
    for (std::size_t index = 0; index < queue.diagnostics.size(); ++index) {
        AddProperty(properties, "Manual queue diagnostic " + std::to_string(index + 1), queue.diagnostics[index]);
    }
}

DirtyStateModel BuildCleanDirtyState()
{
    DirtyStateModel state;
    state.label = "Clean";
    state.detail = "No pending changes.";
    return state;
}

void AddDirtyStateProperties(PropertyListModel& properties, const DirtyStateModel& dirty)
{
    AddProperty(properties, "Dirty state", dirty.label);
    AddProperty(properties, "Save status", dirty.detail);
    AddProperty(properties, "Can save", dirty.canSave ? "yes" : "no");
}

TableModel BuildUnsupportedTable(std::string reason)
{
    TableModel table;
    table.columns = {{"status", "Status"}, {"detail", "Detail"}};
    table.rows.push_back({"unsupported", {"Work domain", std::move(reason)}});
    return table;
}

void AddLyricsRows(TableModel& table, const ArtForge::Files::LyricsWorkViewModel& model)
{
    table.columns = {{"time", "Time"}, {"section", "Section"}, {"text", "Text"}, {"evaluation", "Evaluation"}};
    for (const auto& line : model.lyricLines) {
        table.rows.push_back({line.id, {line.timeRange, line.sectionId, line.text, line.evaluationSummary}});
    }
}

void AddVisualArtRows(TableModel& table, const ArtForge::Files::VisualArtWorkViewModel& model)
{
    table.columns = {{"layer", "Layer"}, {"type", "Type"}, {"label", "Label"}, {"intent", "Intent"}};
    for (const auto& layer : model.viewerLayers) {
        table.rows.push_back({layer.id, {layer.id, layer.layerType, layer.label, layer.intent}});
    }
    for (const auto& layer : model.paintLayers) {
        table.rows.push_back({layer.id, {layer.id, layer.layerType, layer.label, layer.intent}});
    }
}

void AddScriptStoryboardRows(TableModel& table, const ArtForge::Files::ScriptStoryboardWorkViewModel& model)
{
    table.columns = {{"time", "Time"}, {"scene_block", "Scene / block"}, {"speaker_type", "Speaker / type"}, {"text", "Text"}};
    for (const auto& scene : model.scenes) {
        table.rows.push_back({scene.id, {scene.timeRange, scene.id, "scene", scene.title}});
    }
    for (const auto& block : model.blocks) {
        const auto speakerOrType = block.speaker.empty() ? block.kind : block.speaker;
        table.rows.push_back({block.id, {block.timeRange, block.id, speakerOrType, block.text}});
    }
}

}

WorkAppPresentationModel BuildWorkAppPresentationModel(const std::filesystem::path& workPath)
{
    return BuildWorkAppPresentationModel(workPath, ClearWorkAppSelection(workPath));
}

WorkAppPresentationModel BuildWorkAppPresentationModel(
    const std::filesystem::path& workPath,
    const SelectionModel& selection)
{
    WorkAppPresentationModel model;
    const auto result = ArtForge::Files::LoadWorkScopeFile(workPath);
    model.status = {result.status.ok, result.status.ok ? "File load OK" : "File load failed", {}};
    for (const auto& issue : result.status.issues) {
        AddDiagnostic(model.status, issue.message);
    }
    model.navigation = BuildNavigation(result, workPath);
    model.properties = BuildBaseProperties(result, workPath);
    model.selection = selection;
    model.promptPreview = BuildPromptPreview(workPath, model.selection);
    model.manualAiQueue = BuildManualAiQueueModel(workPath, model.selection);
    model.dirtyState = BuildCleanDirtyState();
    AddSelectionProperties(model.properties, workPath, model.selection);
    AddPromptPreviewProperties(model.properties, model.promptPreview);
    AddManualAiQueueProperties(model.properties, model.manualAiQueue);
    AddDirtyStateProperties(model.properties, model.dirtyState);

    if (!result.status.ok) {
        model.domainTable = BuildUnsupportedTable(model.status.diagnostics.empty() ? "Work file load failed" : model.status.diagnostics.front());
        return model;
    }

    if (result.file.workDomain == "lyrics") {
        const auto lyrics = ArtForge::Files::LoadLyricsWorkViewModel(workPath);
        if (lyrics.status.ok) {
            AddLyricsRows(model.domainTable, lyrics);
        } else {
            model.domainTable = BuildUnsupportedTable("Lyrics view model failed to load");
        }
    } else if (result.file.workDomain == "visualArt") {
        const auto visual = ArtForge::Files::LoadVisualArtWorkViewModel(workPath);
        if (visual.status.ok) {
            AddVisualArtRows(model.domainTable, visual);
        } else {
            model.domainTable = BuildUnsupportedTable("Visual art view model failed to load");
        }
    } else if (result.file.workDomain == "scriptStoryboard") {
        const auto script = ArtForge::Files::LoadScriptStoryboardWorkViewModel(workPath);
        if (script.status.ok) {
            AddScriptStoryboardRows(model.domainTable, script);
        } else {
            model.domainTable = BuildUnsupportedTable("Script/storyboard view model failed to load");
        }
    } else {
        model.domainTable = BuildUnsupportedTable(result.file.workDomain.empty() ? "Missing workDomain" : "Unsupported workDomain: " + result.file.workDomain);
    }

    return model;
}

SelectionModel ClearWorkAppSelection(const std::filesystem::path& workPath)
{
    SelectionModel selection;
    selection.sourcePath = workPath.generic_string();
    return selection;
}

SelectionModel SelectWorkAppTableRow(const std::filesystem::path& workPath, int rowIndex)
{
    SelectionModel selection = ClearWorkAppSelection(workPath);
    if (rowIndex < 0) {
        return selection;
    }

    const auto result = ArtForge::Files::LoadWorkScopeFile(workPath);
    if (!result.status.ok) {
        return selection;
    }

    selection.hasSelection = true;
    selection.domain = result.file.workDomain;
    selection.itemIndex = rowIndex;

    const auto index = static_cast<std::size_t>(rowIndex);
    if (result.file.workDomain == "lyrics") {
        const auto lyrics = ArtForge::Files::LoadLyricsWorkViewModel(workPath);
        if (index < lyrics.lyricLines.size()) {
            const auto& line = lyrics.lyricLines[index];
            selection.itemType = "lyricLine";
            selection.itemId = line.id;
            selection.displayLabel = line.text;
            return selection;
        }
    } else if (result.file.workDomain == "visualArt") {
        const auto visual = ArtForge::Files::LoadVisualArtWorkViewModel(workPath);
        if (index < visual.viewerLayers.size()) {
            const auto& layer = visual.viewerLayers[index];
            selection.itemType = "visualLayer";
            selection.itemId = layer.id;
            selection.displayLabel = layer.label;
            return selection;
        }
        const auto paintIndex = index - visual.viewerLayers.size();
        if (paintIndex < visual.paintLayers.size()) {
            const auto& layer = visual.paintLayers[paintIndex];
            selection.itemType = "visualLayer";
            selection.itemId = layer.id;
            selection.displayLabel = layer.label;
            return selection;
        }
    } else if (result.file.workDomain == "scriptStoryboard") {
        const auto script = ArtForge::Files::LoadScriptStoryboardWorkViewModel(workPath);
        if (index < script.scenes.size()) {
            const auto& scene = script.scenes[index];
            selection.itemType = "scriptScene";
            selection.itemId = scene.id;
            selection.displayLabel = scene.title;
            return selection;
        }
        const auto blockIndex = index - script.scenes.size();
        if (blockIndex < script.blocks.size()) {
            const auto& block = script.blocks[blockIndex];
            selection.itemType = "scriptBlock";
            selection.itemId = block.id;
            selection.displayLabel = block.text;
            return selection;
        }
    }

    selection.hasSelection = false;
    selection.itemIndex = -1;
    return selection;
}

}
