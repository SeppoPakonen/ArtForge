#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ArtForge::Files {

struct ScopeFileIssue {
    std::string message;
};

struct ScopeFileReference {
    std::string id;
    std::string path;
    std::optional<int> order;
};

struct ScopeFileLoadStatus {
    bool ok{};
    std::vector<ScopeFileIssue> issues;
};

struct SolutionScopeFile {
    int schemaVersion{1};
    std::string id;
    std::string displayName;
    std::vector<ScopeFileReference> artists;
    std::vector<ScopeFileReference> projects;
};

struct ArtistScopeFile {
    int schemaVersion{1};
    std::string id;
    std::string displayName;
    std::vector<std::string> ruleFiles;
    std::vector<ScopeFileReference> projects;
};

struct SeriesScopeFile {
    int schemaVersion{1};
    std::string id;
    std::string displayName;
    std::optional<ScopeFileReference> artist;
    std::vector<ScopeFileReference> works;
};

struct WorkScopeFile {
    int schemaVersion{1};
    std::string id;
    std::string workKind;
    std::string workDomain;
    std::string displayName;
    std::optional<ScopeFileReference> series;
    std::vector<ScopeFileReference> sources;
    std::optional<std::string> historyPath;
};

struct SolutionScopeFileLoadResult {
    ScopeFileLoadStatus status;
    SolutionScopeFile file;
};

struct ArtistScopeFileLoadResult {
    ScopeFileLoadStatus status;
    ArtistScopeFile file;
};

struct SeriesScopeFileLoadResult {
    ScopeFileLoadStatus status;
    SeriesScopeFile file;
};

struct WorkScopeFileLoadResult {
    ScopeFileLoadStatus status;
    WorkScopeFile file;
};

bool IsRelativeScopeReference(std::string_view path);

std::vector<ScopeFileIssue> ValidateRelativeReferences(const SolutionScopeFile& file);
std::vector<ScopeFileIssue> ValidateRelativeReferences(const ArtistScopeFile& file);
std::vector<ScopeFileIssue> ValidateRelativeReferences(const SeriesScopeFile& file);
std::vector<ScopeFileIssue> ValidateRelativeReferences(const WorkScopeFile& file);

SolutionScopeFileLoadResult LoadSolutionScopeFile(const std::filesystem::path& path);
ArtistScopeFileLoadResult LoadArtistScopeFile(const std::filesystem::path& path);
SeriesScopeFileLoadResult LoadSeriesScopeFile(const std::filesystem::path& path);
WorkScopeFileLoadResult LoadWorkScopeFile(const std::filesystem::path& path);

ScopeFileLoadStatus SaveSolutionScopeFile(const std::filesystem::path& path, const SolutionScopeFile& file);
ScopeFileLoadStatus SaveArtistScopeFile(const std::filesystem::path& path, const ArtistScopeFile& file);
ScopeFileLoadStatus SaveSeriesScopeFile(const std::filesystem::path& path, const SeriesScopeFile& file);
ScopeFileLoadStatus SaveWorkScopeFile(const std::filesystem::path& path, const WorkScopeFile& file);

}
