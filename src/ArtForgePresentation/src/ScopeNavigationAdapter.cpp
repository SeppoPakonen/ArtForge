#include "ArtForge/Presentation/ScopeNavigationAdapter.hpp"

#include "ArtForge/Files/ProjectGraph.hpp"
#include "ArtForge/Files/ScopeFiles.hpp"

#include <utility>

namespace ArtForge::Presentation {

namespace {

NavigationTreeNodeModel Node(std::string id, std::string label)
{
    return {std::move(id), std::move(label), {}};
}

NavigationTreeNodeModel Node(std::string id, std::string label, std::vector<NavigationTreeNodeModel> children)
{
    return {std::move(id), std::move(label), std::move(children)};
}

std::string ReferenceLabel(std::string_view kind, const ArtForge::Files::ScopeFileReference& reference)
{
    auto label = std::string{kind} + ": " + reference.id;
    if (!reference.path.empty()) {
        label += " -> " + reference.path;
    }
    return label;
}

std::string Narrow(std::wstring_view value)
{
    std::string narrowed;
    narrowed.reserve(value.size());
    for (const auto character : value) {
        narrowed.push_back(static_cast<char>(character));
    }
    return narrowed;
}

NavigationTreeModel EmptyNavigation(
    ArtForge::Core::ScopeKind scope,
    const std::string& loadStatus,
    const std::string& loadDetail)
{
    auto root = Node("current-scope", "Current Scope: " + Narrow(ArtForge::Core::ToDisplayName(scope)));
    root.children.push_back(Node("path", "Path: (none)"));
    root.children.push_back(Node("load-status", "Load status: " + loadStatus));
    root.children.push_back(Node("open-recent", "Open Recent"));
    root.children.push_back(Node("examples", "Examples"));
    root.children.push_back(Node("diagnostics", "Diagnostics: " + loadDetail));
    return {{std::move(root)}};
}

NavigationTreeModel BuildSolutionNavigation(const std::filesystem::path& openedPath)
{
    const auto graph = ArtForge::Files::LoadSolutionProjectGraph(openedPath);
    auto root = Node("solution", "Solution Explorer");
    root.children.push_back(Node("path", "Path: " + openedPath.generic_string()));

    auto artists = Node("artists", "Artists");
    if (graph.artists.empty()) {
        artists.children.push_back(Node("artists.empty", "(none)"));
    }
    for (const auto& artist : graph.artists) {
        auto artistNode = Node("artist." + artist.reference.id, artist.file.displayName.empty() ? artist.reference.id : artist.file.displayName);
        artistNode.children.push_back(Node("artist.path", "Path: " + artist.path.generic_string()));
        for (const auto& project : artist.projects) {
            auto projectNode = Node("artist.project." + project.reference.id, project.file.displayName.empty() ? project.reference.id : project.file.displayName);
            for (const auto& work : project.works) {
                projectNode.children.push_back(Node("artist.project.work." + work.reference.id, work.file.displayName.empty() ? work.reference.id : work.file.displayName));
            }
            artistNode.children.push_back(std::move(projectNode));
        }
        artists.children.push_back(std::move(artistNode));
    }
    root.children.push_back(std::move(artists));

    auto projects = Node("projects", "Series and Projects");
    if (graph.projects.empty()) {
        projects.children.push_back(Node("projects.empty", "(none)"));
    }
    for (const auto& project : graph.projects) {
        auto projectNode = Node("project." + project.reference.id, project.file.displayName.empty() ? project.reference.id : project.file.displayName);
        for (const auto& work : project.works) {
            projectNode.children.push_back(Node("project.work." + work.reference.id, work.file.displayName.empty() ? work.reference.id : work.file.displayName));
        }
        projects.children.push_back(std::move(projectNode));
    }
    root.children.push_back(std::move(projects));

    const auto issues = ArtForge::Files::FlattenGraphIssues(graph);
    auto diagnostics = Node("diagnostics", "Diagnostics");
    if (issues.empty()) {
        diagnostics.children.push_back(Node("diagnostics.ok", "No project graph diagnostics."));
    }
    for (const auto& issue : issues) {
        diagnostics.children.push_back(Node("diagnostic", issue.path.generic_string() + ": " + issue.message));
    }
    root.children.push_back(std::move(diagnostics));
    return {{std::move(root)}};
}

NavigationTreeModel BuildArtistNavigation(const std::filesystem::path& openedPath)
{
    const auto result = ArtForge::Files::LoadArtistScopeFile(openedPath);
    auto root = Node("artist", "Artist Explorer");
    root.children.push_back(Node("path", "Path: " + openedPath.generic_string()));
    auto identity = Node("identity", "Identity");
    identity.children.push_back(Node("artist.id", "Artist id: " + result.file.id));
    identity.children.push_back(Node("artist.name", "Display name: " + result.file.displayName));
    root.children.push_back(std::move(identity));

    auto projects = Node("projects", "Projects");
    if (result.file.projects.empty()) {
        projects.children.push_back(Node("projects.empty", "(none)"));
    }
    for (const auto& project : result.file.projects) {
        projects.children.push_back(Node("project." + project.id, ReferenceLabel("project", project)));
    }
    root.children.push_back(std::move(projects));
    root.children.push_back(Node("diagnostics", result.status.ok ? "Diagnostics: none" : "Diagnostics: file load has issues"));
    return {{std::move(root)}};
}

NavigationTreeModel BuildSeriesNavigation(const std::filesystem::path& openedPath)
{
    const auto result = ArtForge::Files::LoadSeriesScopeFile(openedPath);
    auto root = Node("series", "Series Explorer");
    root.children.push_back(Node("path", "Path: " + openedPath.generic_string()));
    auto parent = Node("parent", "Parent Artist");
    parent.children.push_back(result.file.artist ? Node("artist", ReferenceLabel("artist", *result.file.artist)) : Node("artist.empty", "(none)"));
    root.children.push_back(std::move(parent));

    auto works = Node("works", "Ordered Works");
    if (result.file.works.empty()) {
        works.children.push_back(Node("works.empty", "(none)"));
    }
    for (const auto& work : result.file.works) {
        works.children.push_back(Node("work." + work.id, ReferenceLabel("work", work)));
    }
    root.children.push_back(std::move(works));
    root.children.push_back(Node("diagnostics", result.status.ok ? "Diagnostics: none" : "Diagnostics: file load has issues"));
    return {{std::move(root)}};
}

NavigationTreeModel BuildWorkNavigation(const std::filesystem::path& openedPath)
{
    const auto result = ArtForge::Files::LoadWorkScopeFile(openedPath);
    auto root = Node("work", "Work Explorer");
    root.children.push_back(Node("path", "Path: " + openedPath.generic_string()));
    root.children.push_back(Node("domain", "Work domain: " + (result.file.workDomain.empty() ? std::string{"(unspecified)"} : result.file.workDomain)));

    auto parent = Node("parent", "Parent Series");
    parent.children.push_back(result.file.series ? Node("series", ReferenceLabel("series", *result.file.series)) : Node("series.empty", "(none)"));
    root.children.push_back(std::move(parent));

    auto sources = Node("sources", "Sources and Fragments");
    if (result.file.sources.empty()) {
        sources.children.push_back(Node("sources.empty", "(none)"));
    }
    for (const auto& source : result.file.sources) {
        sources.children.push_back(Node("source." + source.id, ReferenceLabel("source", source)));
    }
    root.children.push_back(std::move(sources));
    root.children.push_back(Node("history", result.file.historyPath ? "History: " + *result.file.historyPath : "History: (none)"));
    root.children.push_back(Node("diagnostics", result.status.ok ? "Diagnostics: none" : "Diagnostics: file load has issues"));
    return {{std::move(root)}};
}

}

NavigationTreeModel BuildScopeNavigationModel(
    ArtForge::Core::ScopeKind scope,
    const std::filesystem::path& openedPath,
    const std::string& loadStatus,
    const std::string& loadDetail)
{
    if (openedPath.empty()) {
        return EmptyNavigation(scope, loadStatus, loadDetail);
    }

    switch (scope) {
    case ArtForge::Core::ScopeKind::Solution:
        return BuildSolutionNavigation(openedPath);
    case ArtForge::Core::ScopeKind::Artist:
        return BuildArtistNavigation(openedPath);
    case ArtForge::Core::ScopeKind::Series:
        return BuildSeriesNavigation(openedPath);
    case ArtForge::Core::ScopeKind::WorkItem:
        return BuildWorkNavigation(openedPath);
    case ArtForge::Core::ScopeKind::Fragment:
        return EmptyNavigation(scope, loadStatus, "Fragment scope navigation is not implemented yet.");
    }

    return EmptyNavigation(scope, loadStatus, loadDetail);
}

}
