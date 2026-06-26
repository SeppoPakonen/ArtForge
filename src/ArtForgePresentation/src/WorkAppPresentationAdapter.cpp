#include "ArtForge/Presentation/WorkAppPresentationAdapter.hpp"

#include "ArtForge/Files/DomainWorkViewModels.hpp"
#include "ArtForge/Files/ScopeFiles.hpp"

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
    WorkAppPresentationModel model;
    const auto result = ArtForge::Files::LoadWorkScopeFile(workPath);
    model.status = {result.status.ok, result.status.ok ? "File load OK" : "File load failed", {}};
    for (const auto& issue : result.status.issues) {
        AddDiagnostic(model.status, issue.message);
    }
    model.navigation = BuildNavigation(result, workPath);
    model.properties = BuildBaseProperties(result, workPath);

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

}
