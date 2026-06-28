#include "ArtForge/Files/RecentFiles.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

namespace ArtForge::Files {

namespace {

std::string JsonEscape(std::string_view value)
{
    std::string escaped;
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += character;
            break;
        }
    }
    return escaped;
}

std::string Quote(std::string_view value)
{
    return "\"" + JsonEscape(value) + "\"";
}

std::optional<std::size_t> FindFieldValue(std::string_view json, std::string_view key)
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
    return valuePosition;
}

std::optional<std::string> ReadStringField(std::string_view json, std::string_view key)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition || *valuePosition >= json.size() || json[*valuePosition] != '"') {
        return std::nullopt;
    }

    std::string value;
    bool escaped = false;
    for (auto index = *valuePosition + 1; index < json.size(); ++index) {
        const char character = json[index];
        if (escaped) {
            switch (character) {
            case 'n':
                value += '\n';
                break;
            case 'r':
                value += '\r';
                break;
            case 't':
                value += '\t';
                break;
            default:
                value += character;
                break;
            }
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '"') {
            return value;
        }
        value += character;
    }
    return std::nullopt;
}

std::optional<std::string_view> ReadArray(std::string_view json, std::string_view key)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition || *valuePosition >= json.size() || json[*valuePosition] != '[') {
        return std::nullopt;
    }

    bool inString = false;
    bool escaped = false;
    int depth = 0;
    for (auto index = *valuePosition; index < json.size(); ++index) {
        const char character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
        } else if (character == '[') {
            ++depth;
        } else if (character == ']') {
            --depth;
            if (depth == 0) {
                return json.substr(*valuePosition, index - *valuePosition + 1);
            }
        }
    }
    return std::nullopt;
}

std::vector<std::string_view> ObjectItems(std::string_view arrayJson)
{
    std::vector<std::string_view> items;
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    std::optional<std::size_t> objectStart;
    for (std::size_t index = 0; index < arrayJson.size(); ++index) {
        const char character = arrayJson[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }
        if (character == '"') {
            inString = true;
        } else if (character == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
        } else if (character == '}') {
            --depth;
            if (depth == 0 && objectStart) {
                items.push_back(arrayJson.substr(*objectStart, index - *objectStart + 1));
                objectStart.reset();
            }
        }
    }
    return items;
}

std::string PathString(const std::filesystem::path& path)
{
    return path.generic_string();
}

bool SamePath(const std::filesystem::path& left, const std::filesystem::path& right)
{
    std::error_code error;
    if (std::filesystem::equivalent(left, right, error)) {
        return true;
    }
    return left == right;
}

void AddExample(std::vector<ExampleFileEntry>& entries, const std::filesystem::path& root, RecentScopeType scope, std::string displayName, std::filesystem::path relativePath)
{
    const auto path = root / relativePath;
    entries.push_back({path, scope, std::move(displayName), std::filesystem::exists(path)});
}

}

std::string ToStorageName(RecentScopeType scope)
{
    switch (scope) {
    case RecentScopeType::Solution:
        return "solution";
    case RecentScopeType::Artist:
        return "artist";
    case RecentScopeType::Series:
        return "series";
    case RecentScopeType::Work:
        return "work";
    case RecentScopeType::Unknown:
        return "unknown";
    }
    return "unknown";
}

RecentScopeType RecentScopeTypeFromStorageName(std::string_view value)
{
    if (value == "solution") {
        return RecentScopeType::Solution;
    }
    if (value == "artist") {
        return RecentScopeType::Artist;
    }
    if (value == "series") {
        return RecentScopeType::Series;
    }
    if (value == "work") {
        return RecentScopeType::Work;
    }
    return RecentScopeType::Unknown;
}

std::filesystem::path DefaultRecentFilesPath()
{
    char* appData = nullptr;
    std::size_t appDataLength = 0;
    if (_dupenv_s(&appData, &appDataLength, "APPDATA") != 0 || appData == nullptr) {
        return {};
    }
    std::filesystem::path path{appData};
    free(appData);
    path /= "ArtForge";
    path /= "settings";
    path /= "recent-files.json";
    return path;
}

RecentFilesLoadResult LoadRecentFiles(const std::filesystem::path& path)
{
    RecentFilesLoadResult result;
    if (path.empty()) {
        result.ok = false;
        result.diagnostics.push_back("APPDATA is unavailable; recent files cannot be loaded");
        return result;
    }

    std::ifstream input{path};
    if (!input) {
        return result;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const auto json = buffer.str();
    const auto entriesJson = ReadArray(json, "entries");
    if (!entriesJson) {
        result.ok = false;
        result.diagnostics.push_back("recent file settings are missing entries array");
        return result;
    }

    for (const auto item : ObjectItems(*entriesJson)) {
        RecentFileEntry entry;
        entry.path = ReadStringField(item, "path").value_or("");
        entry.scope = RecentScopeTypeFromStorageName(ReadStringField(item, "scope").value_or(""));
        entry.displayName = ReadStringField(item, "displayName").value_or("");
        entry.lastOpenedUtc = ReadStringField(item, "lastOpenedUtc").value_or("");
        entry.exists = std::filesystem::exists(entry.path);
        if (!entry.exists) {
            result.diagnostics.push_back("missing recent file: " + PathString(entry.path));
            continue;
        }
        result.entries.push_back(std::move(entry));
    }
    return result;
}

RecentFilesSaveResult SaveRecentFiles(
    const std::filesystem::path& path,
    const std::vector<RecentFileEntry>& entries)
{
    RecentFilesSaveResult result;
    if (path.empty()) {
        result.ok = false;
        result.diagnostics.push_back("APPDATA is unavailable; recent files cannot be saved");
        return result;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        result.ok = false;
        result.diagnostics.push_back("could not create recent file settings directory");
        return result;
    }

    std::ofstream output{path, std::ios::trunc};
    if (!output) {
        result.ok = false;
        result.diagnostics.push_back("could not open recent file settings for writing");
        return result;
    }

    output << "{\n";
    output << "  \"format\": \"artforge.user.recent-files\",\n";
    output << "  \"entries\": [\n";
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto& entry = entries[index];
        output << "    {\n";
        output << "      \"path\": " << Quote(PathString(entry.path)) << ",\n";
        output << "      \"scope\": " << Quote(ToStorageName(entry.scope)) << ",\n";
        output << "      \"displayName\": " << Quote(entry.displayName) << ",\n";
        output << "      \"lastOpenedUtc\": " << Quote(entry.lastOpenedUtc) << "\n";
        output << "    }" << (index + 1 < entries.size() ? "," : "") << "\n";
    }
    output << "  ]\n";
    output << "}\n";
    return result;
}

std::vector<RecentFileEntry> AddOrPromoteRecentFile(
    std::vector<RecentFileEntry> entries,
    RecentFileEntry entry,
    std::size_t maxEntries)
{
    entries.erase(
        std::remove_if(
            entries.begin(),
            entries.end(),
            [&](const RecentFileEntry& existing) {
                return SamePath(existing.path, entry.path);
            }),
        entries.end());
    entry.exists = std::filesystem::exists(entry.path);
    entries.insert(entries.begin(), std::move(entry));
    if (entries.size() > maxEntries) {
        entries.resize(maxEntries);
    }
    return entries;
}

std::vector<ExampleFileEntry> BuildExampleFileIndex(const std::filesystem::path& repositoryRoot)
{
    std::vector<ExampleFileEntry> entries;
    AddExample(entries, repositoryRoot, RecentScopeType::Solution, "Sample solution graph", "examples/graph/sample.afsolution.json");
    AddExample(entries, repositoryRoot, RecentScopeType::Artist, "Sample artist", "examples/graph/artists/sample.afartist.json");
    AddExample(entries, repositoryRoot, RecentScopeType::Series, "Sample series", "examples/graph/projects/sample-series/sample.afseries.json");
    AddExample(entries, repositoryRoot, RecentScopeType::Work, "Lyrics work", "examples/work-domains/lyrics.afwork.json");
    AddExample(entries, repositoryRoot, RecentScopeType::Work, "Visual art work", "examples/work-domains/visual-art.afwork.json");
    AddExample(entries, repositoryRoot, RecentScopeType::Work, "Script storyboard work", "examples/work-domains/script-storyboard.afwork.json");
    return entries;
}

}
