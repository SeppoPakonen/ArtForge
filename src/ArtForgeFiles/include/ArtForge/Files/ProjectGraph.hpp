#pragma once

#include "ArtForge/Files/ScopeFiles.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ArtForge::Files {

enum class ScopeGraphNodeStatus {
    Loaded,
    Missing,
    ParseFailed,
    UnsupportedSchemaVersion,
};

struct ScopeGraphIssue {
    std::filesystem::path path;
    std::string message;
};

struct SourceGraphReference {
    ScopeFileReference reference;
    std::filesystem::path resolvedPath;
};

struct WorkGraphNode {
    ScopeFileReference reference;
    std::filesystem::path path;
    ScopeGraphNodeStatus status{ScopeGraphNodeStatus::Missing};
    WorkScopeFile file;
    std::vector<SourceGraphReference> sources;
    std::vector<ScopeGraphIssue> issues;
};

struct SeriesGraphNode {
    ScopeFileReference reference;
    std::filesystem::path path;
    ScopeGraphNodeStatus status{ScopeGraphNodeStatus::Missing};
    SeriesScopeFile file;
    std::vector<WorkGraphNode> works;
    std::vector<ScopeGraphIssue> issues;
};

struct ArtistGraphNode {
    ScopeFileReference reference;
    std::filesystem::path path;
    ScopeGraphNodeStatus status{ScopeGraphNodeStatus::Missing};
    ArtistScopeFile file;
    std::vector<SeriesGraphNode> projects;
    std::vector<ScopeGraphIssue> issues;
};

struct SolutionProjectGraph {
    std::filesystem::path path;
    ScopeGraphNodeStatus status{ScopeGraphNodeStatus::Missing};
    SolutionScopeFile file;
    std::vector<ArtistGraphNode> artists;
    std::vector<SeriesGraphNode> projects;
    std::vector<ScopeGraphIssue> issues;
};

std::filesystem::path ResolveScopeReference(
    const std::filesystem::path& containingFile,
    std::string_view referencePath);

SolutionProjectGraph LoadSolutionProjectGraph(const std::filesystem::path& path);

std::vector<ScopeGraphIssue> FlattenGraphIssues(const SolutionProjectGraph& graph);
std::string DescribeProjectGraph(const SolutionProjectGraph& graph);

}
