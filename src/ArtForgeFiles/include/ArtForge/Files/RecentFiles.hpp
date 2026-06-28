#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ArtForge::Files {

enum class RecentScopeType {
    Solution,
    Artist,
    Series,
    Work,
    Unknown,
};

struct RecentFileEntry {
    std::filesystem::path path;
    RecentScopeType scope{RecentScopeType::Unknown};
    std::string displayName;
    std::string lastOpenedUtc;
    bool exists{false};
};

struct ExampleFileEntry {
    std::filesystem::path path;
    RecentScopeType scope{RecentScopeType::Unknown};
    std::string displayName;
    bool exists{false};
};

struct RecentFilesLoadResult {
    bool ok{true};
    std::vector<std::string> diagnostics;
    std::vector<RecentFileEntry> entries;
};

struct RecentFilesSaveResult {
    bool ok{true};
    std::vector<std::string> diagnostics;
};

[[nodiscard]] std::string ToStorageName(RecentScopeType scope);
[[nodiscard]] RecentScopeType RecentScopeTypeFromStorageName(std::string_view value);

[[nodiscard]] std::filesystem::path DefaultRecentFilesPath();

[[nodiscard]] RecentFilesLoadResult LoadRecentFiles(const std::filesystem::path& path);
[[nodiscard]] RecentFilesSaveResult SaveRecentFiles(
    const std::filesystem::path& path,
    const std::vector<RecentFileEntry>& entries);

[[nodiscard]] std::vector<RecentFileEntry> AddOrPromoteRecentFile(
    std::vector<RecentFileEntry> entries,
    RecentFileEntry entry,
    std::size_t maxEntries = 12);

[[nodiscard]] std::vector<ExampleFileEntry> BuildExampleFileIndex(const std::filesystem::path& repositoryRoot);

}
