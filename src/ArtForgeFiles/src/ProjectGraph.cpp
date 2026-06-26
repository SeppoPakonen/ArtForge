#include "ArtForge/Files/ProjectGraph.hpp"

#include <algorithm>
#include <sstream>

namespace ArtForge::Files {

namespace {

ScopeGraphNodeStatus StatusFromLoadResult(const ScopeFileLoadStatus& status, int schemaVersion)
{
    if (!status.ok) {
        return ScopeGraphNodeStatus::ParseFailed;
    }

    if (schemaVersion != 1) {
        return ScopeGraphNodeStatus::UnsupportedSchemaVersion;
    }

    return ScopeGraphNodeStatus::Loaded;
}

void AppendLoadIssues(
    std::vector<ScopeGraphIssue>& issues,
    const std::filesystem::path& path,
    const ScopeFileLoadStatus& status)
{
    for (const auto& issue : status.issues) {
        issues.push_back({path, issue.message});
    }
}

ScopeGraphNodeStatus MissingOrLoadStatus(
    const std::filesystem::path& path,
    const ScopeFileLoadStatus& status,
    int schemaVersion)
{
    if (!std::filesystem::exists(path)) {
        return ScopeGraphNodeStatus::Missing;
    }

    return StatusFromLoadResult(status, schemaVersion);
}

void AddMissingIssue(
    std::vector<ScopeGraphIssue>& issues,
    const std::filesystem::path& path,
    std::string_view scopeName)
{
    issues.push_back({path, std::string{scopeName} + " file is missing"});
}

void AddUnsupportedIssue(
    std::vector<ScopeGraphIssue>& issues,
    const std::filesystem::path& path,
    int schemaVersion)
{
    issues.push_back({
        path,
        "unsupported schemaVersion: " + std::to_string(schemaVersion),
    });
}

WorkGraphNode LoadWorkNode(const std::filesystem::path& containingFile, const ScopeFileReference& reference)
{
    WorkGraphNode node;
    node.reference = reference;
    node.path = ResolveScopeReference(containingFile, reference.path);

    if (!std::filesystem::exists(node.path)) {
        node.status = ScopeGraphNodeStatus::Missing;
        AddMissingIssue(node.issues, node.path, "work");
        return node;
    }

    const auto result = LoadWorkScopeFile(node.path);
    node.file = result.file;
    node.status = StatusFromLoadResult(result.status, node.file.schemaVersion);
    AppendLoadIssues(node.issues, node.path, result.status);
    if (node.status == ScopeGraphNodeStatus::UnsupportedSchemaVersion) {
        AddUnsupportedIssue(node.issues, node.path, node.file.schemaVersion);
    }

    for (const auto& source : node.file.sources) {
        node.sources.push_back({
            source,
            ResolveScopeReference(node.path, source.path),
        });
    }

    return node;
}

SeriesGraphNode LoadSeriesNode(const std::filesystem::path& containingFile, const ScopeFileReference& reference)
{
    SeriesGraphNode node;
    node.reference = reference;
    node.path = ResolveScopeReference(containingFile, reference.path);

    if (!std::filesystem::exists(node.path)) {
        node.status = ScopeGraphNodeStatus::Missing;
        AddMissingIssue(node.issues, node.path, "series");
        return node;
    }

    const auto result = LoadSeriesScopeFile(node.path);
    node.file = result.file;
    node.status = StatusFromLoadResult(result.status, node.file.schemaVersion);
    AppendLoadIssues(node.issues, node.path, result.status);
    if (node.status == ScopeGraphNodeStatus::UnsupportedSchemaVersion) {
        AddUnsupportedIssue(node.issues, node.path, node.file.schemaVersion);
    }

    std::vector<ScopeFileReference> orderedWorks = node.file.works;
    std::stable_sort(
        orderedWorks.begin(),
        orderedWorks.end(),
        [](const ScopeFileReference& left, const ScopeFileReference& right) {
            if (left.order && right.order) {
                return *left.order < *right.order;
            }
            return left.order.has_value() && !right.order.has_value();
        });

    for (const auto& work : orderedWorks) {
        node.works.push_back(LoadWorkNode(node.path, work));
    }

    return node;
}

ArtistGraphNode LoadArtistNode(const std::filesystem::path& containingFile, const ScopeFileReference& reference)
{
    ArtistGraphNode node;
    node.reference = reference;
    node.path = ResolveScopeReference(containingFile, reference.path);

    if (!std::filesystem::exists(node.path)) {
        node.status = ScopeGraphNodeStatus::Missing;
        AddMissingIssue(node.issues, node.path, "artist");
        return node;
    }

    const auto result = LoadArtistScopeFile(node.path);
    node.file = result.file;
    node.status = StatusFromLoadResult(result.status, node.file.schemaVersion);
    AppendLoadIssues(node.issues, node.path, result.status);
    if (node.status == ScopeGraphNodeStatus::UnsupportedSchemaVersion) {
        AddUnsupportedIssue(node.issues, node.path, node.file.schemaVersion);
    }

    for (const auto& project : node.file.projects) {
        node.projects.push_back(LoadSeriesNode(node.path, project));
    }

    return node;
}

void AppendIssueLines(std::ostringstream& output, const std::vector<ScopeGraphIssue>& issues)
{
    for (const auto& issue : issues) {
        output << "issue: " << issue.path.string() << ": " << issue.message << "\n";
    }
}

void FlattenIssuesFromSeries(const SeriesGraphNode& series, std::vector<ScopeGraphIssue>& issues)
{
    issues.insert(issues.end(), series.issues.begin(), series.issues.end());
    for (const auto& work : series.works) {
        issues.insert(issues.end(), work.issues.begin(), work.issues.end());
    }
}

std::string StatusName(ScopeGraphNodeStatus status)
{
    switch (status) {
    case ScopeGraphNodeStatus::Loaded:
        return "loaded";
    case ScopeGraphNodeStatus::Missing:
        return "missing";
    case ScopeGraphNodeStatus::ParseFailed:
        return "parse failed";
    case ScopeGraphNodeStatus::UnsupportedSchemaVersion:
        return "unsupported schema/version";
    }

    return "unknown";
}

}

std::filesystem::path ResolveScopeReference(
    const std::filesystem::path& containingFile,
    std::string_view referencePath)
{
    const std::filesystem::path reference{std::string{referencePath}};
    if (reference.is_absolute()) {
        return reference.lexically_normal();
    }

    return (containingFile.parent_path() / reference).lexically_normal();
}

SolutionProjectGraph LoadSolutionProjectGraph(const std::filesystem::path& path)
{
    SolutionProjectGraph graph;
    graph.path = std::filesystem::path{path}.lexically_normal();

    if (!std::filesystem::exists(graph.path)) {
        graph.status = ScopeGraphNodeStatus::Missing;
        AddMissingIssue(graph.issues, graph.path, "solution");
        return graph;
    }

    const auto result = LoadSolutionScopeFile(graph.path);
    graph.file = result.file;
    graph.status = StatusFromLoadResult(result.status, graph.file.schemaVersion);
    AppendLoadIssues(graph.issues, graph.path, result.status);
    if (graph.status == ScopeGraphNodeStatus::UnsupportedSchemaVersion) {
        AddUnsupportedIssue(graph.issues, graph.path, graph.file.schemaVersion);
    }

    for (const auto& artist : graph.file.artists) {
        graph.artists.push_back(LoadArtistNode(graph.path, artist));
    }

    for (const auto& project : graph.file.projects) {
        graph.projects.push_back(LoadSeriesNode(graph.path, project));
    }

    return graph;
}

std::vector<ScopeGraphIssue> FlattenGraphIssues(const SolutionProjectGraph& graph)
{
    std::vector<ScopeGraphIssue> issues;
    issues.insert(issues.end(), graph.issues.begin(), graph.issues.end());

    for (const auto& artist : graph.artists) {
        issues.insert(issues.end(), artist.issues.begin(), artist.issues.end());
        for (const auto& project : artist.projects) {
            FlattenIssuesFromSeries(project, issues);
        }
    }

    for (const auto& project : graph.projects) {
        FlattenIssuesFromSeries(project, issues);
    }

    return issues;
}

std::string DescribeProjectGraph(const SolutionProjectGraph& graph)
{
    std::ostringstream output;
    output << "solution: " << graph.file.displayName << " [" << StatusName(graph.status) << "]\n";
    for (const auto& artist : graph.artists) {
        output << "artist: " << artist.file.displayName << " [" << StatusName(artist.status) << "]\n";
        for (const auto& project : artist.projects) {
            output << "  project: " << project.file.displayName << " [" << StatusName(project.status) << "]\n";
            for (const auto& work : project.works) {
                output << "    work: " << work.file.displayName << " [" << StatusName(work.status) << "]\n";
            }
        }
    }

    for (const auto& project : graph.projects) {
        output << "project: " << project.file.displayName << " [" << StatusName(project.status) << "]\n";
        for (const auto& work : project.works) {
            output << "  work: " << work.file.displayName << " [" << StatusName(work.status) << "]\n";
        }
    }

    AppendIssueLines(output, FlattenGraphIssues(graph));
    return output.str();
}

}
