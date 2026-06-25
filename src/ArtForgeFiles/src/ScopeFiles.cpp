#include "ArtForge/Files/ScopeFiles.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string_view>

namespace ArtForge::Files {

namespace {

constexpr std::string_view SolutionFormat = "artforge.solution";
constexpr std::string_view ArtistFormat = "artforge.artist";
constexpr std::string_view SeriesFormat = "artforge.series";
constexpr std::string_view WorkFormat = "artforge.work";

std::string ReadTextFile(const std::filesystem::path& path, ScopeFileLoadStatus& status)
{
    std::ifstream input{path};
    if (!input) {
        status.issues.push_back({"could not open file"});
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

ScopeFileLoadStatus WriteTextFile(const std::filesystem::path& path, std::string_view content)
{
    ScopeFileLoadStatus status{true, {}};
    std::ofstream output{path};
    if (!output) {
        status.ok = false;
        status.issues.push_back({"could not open file for writing"});
        return status;
    }

    output << content;
    if (!output) {
        status.ok = false;
        status.issues.push_back({"could not write file"});
    }

    return status;
}

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

std::string Key(std::string_view name)
{
    return "\"" + std::string{name} + "\": ";
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

std::optional<std::pair<std::string, std::size_t>> ReadStringAt(std::string_view json, std::size_t quotePosition)
{
    if (quotePosition >= json.size() || json[quotePosition] != '"') {
        return std::nullopt;
    }

    std::string value;
    bool escaped = false;
    for (auto index = quotePosition + 1; index < json.size(); ++index) {
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
            return std::pair{value, index + 1};
        }

        value += character;
    }

    return std::nullopt;
}

std::optional<int> ReadIntField(std::string_view json, std::string_view key)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition) {
        return std::nullopt;
    }

    auto endPosition = *valuePosition;
    while (endPosition < json.size() && std::isdigit(static_cast<unsigned char>(json[endPosition])) != 0) {
        ++endPosition;
    }

    int value = 0;
    const auto parseResult = std::from_chars(json.data() + *valuePosition, json.data() + endPosition, value);
    if (parseResult.ec != std::errc{}) {
        return std::nullopt;
    }

    return value;
}

std::optional<std::string_view> ReadContainer(std::string_view json, std::string_view key, char open, char close)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition || *valuePosition >= json.size() || json[*valuePosition] != open) {
        return std::nullopt;
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
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
        } else if (character == open) {
            ++depth;
        } else if (character == close) {
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

std::vector<ScopeFileReference> ReadReferenceArray(std::string_view json, std::string_view key)
{
    std::vector<ScopeFileReference> references;
    const auto arrayJson = ReadContainer(json, key, '[', ']');
    if (!arrayJson) {
        return references;
    }

    for (const auto item : ObjectItems(*arrayJson)) {
        ScopeFileReference reference;
        reference.id = ReadStringField(item, "id").value_or("");
        reference.path = ReadStringField(item, "path").value_or("");
        reference.order = ReadIntField(item, "order");
        references.push_back(reference);
    }

    return references;
}

std::vector<std::string> ReadStringArray(std::string_view json, std::string_view key)
{
    std::vector<std::string> values;
    const auto arrayJson = ReadContainer(json, key, '[', ']');
    if (!arrayJson) {
        return values;
    }

    std::string_view rest = *arrayJson;
    while (true) {
        const auto quotePosition = rest.find('"');
        if (quotePosition == std::string_view::npos) {
            break;
        }

        const auto value = ReadStringAt(rest, quotePosition);
        if (!value) {
            break;
        }

        values.push_back(value->first);
        rest = rest.substr(value->second);
    }

    return values;
}

std::optional<ScopeFileReference> ReadReferenceObject(std::string_view json, std::string_view key)
{
    const auto objectJson = ReadContainer(json, key, '{', '}');
    if (!objectJson) {
        return std::nullopt;
    }

    return ScopeFileReference{
        ReadStringField(*objectJson, "id").value_or(""),
        ReadStringField(*objectJson, "path").value_or(""),
        ReadIntField(*objectJson, "order"),
    };
}

std::optional<std::string> ReadObjectPath(std::string_view json, std::string_view key)
{
    const auto objectJson = ReadContainer(json, key, '{', '}');
    if (!objectJson) {
        return std::nullopt;
    }

    return ReadStringField(*objectJson, "path");
}

void RequireField(ScopeFileLoadStatus& status, std::string_view value, std::string_view field)
{
    if (value.empty()) {
        status.issues.push_back({"missing required field: " + std::string{field}});
    }
}

void ValidateFormatAndScope(
    ScopeFileLoadStatus& status,
    std::string_view json,
    std::string_view expectedFormat,
    std::string_view expectedScope)
{
    if (ReadStringField(json, "format").value_or("") != expectedFormat) {
        status.issues.push_back({"unexpected or missing format"});
    }

    if (ReadStringField(json, "scope").value_or("") != expectedScope) {
        status.issues.push_back({"unexpected or missing scope"});
    }

    const auto schemaVersion = ReadIntField(json, "schemaVersion");
    if (!schemaVersion || *schemaVersion < 1) {
        status.issues.push_back({"schemaVersion must be a positive integer"});
    }
}

void AddRelativeReferenceIssue(std::vector<ScopeFileIssue>& issues, std::string_view owner, std::string_view path)
{
    if (!IsRelativeScopeReference(path)) {
        issues.push_back({std::string{owner} + " must be a non-empty relative path"});
    }
}

void AppendReferenceJson(std::ostringstream& output, const ScopeFileReference& reference, int indent, bool trailingComma)
{
    const std::string spaces(static_cast<std::size_t>(indent), ' ');
    output << spaces << "{\n";
    output << spaces << "  " << Key("id") << Quote(reference.id) << ",\n";
    output << spaces << "  " << Key("path") << Quote(reference.path);
    if (reference.order) {
        output << ",\n" << spaces << "  " << Key("order") << *reference.order << "\n";
    } else {
        output << "\n";
    }
    output << spaces << "}" << (trailingComma ? "," : "") << "\n";
}

void AppendReferenceArrayJson(std::ostringstream& output, std::string_view key, const std::vector<ScopeFileReference>& references)
{
    output << "  " << Key(key) << "[\n";
    for (std::size_t index = 0; index < references.size(); ++index) {
        AppendReferenceJson(output, references[index], 4, index + 1 < references.size());
    }
    output << "  ]";
}

ScopeFileLoadStatus FinalStatus(std::vector<ScopeFileIssue> issues)
{
    return {issues.empty(), std::move(issues)};
}

}

bool IsRelativeScopeReference(std::string_view path)
{
    if (path.empty()) {
        return false;
    }

    const std::filesystem::path referencePath{std::string{path}};
    return referencePath.is_relative();
}

std::vector<ScopeFileIssue> ValidateRelativeReferences(const SolutionScopeFile& file)
{
    std::vector<ScopeFileIssue> issues;
    for (const auto& artist : file.artists) {
        AddRelativeReferenceIssue(issues, "artists[].path", artist.path);
    }
    for (const auto& project : file.projects) {
        AddRelativeReferenceIssue(issues, "projects[].path", project.path);
    }
    return issues;
}

std::vector<ScopeFileIssue> ValidateRelativeReferences(const ArtistScopeFile& file)
{
    std::vector<ScopeFileIssue> issues;
    for (const auto& ruleFile : file.ruleFiles) {
        AddRelativeReferenceIssue(issues, "ruleFiles[]", ruleFile);
    }
    for (const auto& project : file.projects) {
        AddRelativeReferenceIssue(issues, "projects[].path", project.path);
    }
    return issues;
}

std::vector<ScopeFileIssue> ValidateRelativeReferences(const SeriesScopeFile& file)
{
    std::vector<ScopeFileIssue> issues;
    if (file.artist) {
        AddRelativeReferenceIssue(issues, "artist.path", file.artist->path);
    }
    for (const auto& work : file.works) {
        AddRelativeReferenceIssue(issues, "works[].path", work.path);
    }
    return issues;
}

std::vector<ScopeFileIssue> ValidateRelativeReferences(const WorkScopeFile& file)
{
    std::vector<ScopeFileIssue> issues;
    if (file.series) {
        AddRelativeReferenceIssue(issues, "series.path", file.series->path);
    }
    for (const auto& source : file.sources) {
        AddRelativeReferenceIssue(issues, "sources[].path", source.path);
    }
    if (file.historyPath) {
        AddRelativeReferenceIssue(issues, "history.path", *file.historyPath);
    }
    return issues;
}

SolutionScopeFileLoadResult LoadSolutionScopeFile(const std::filesystem::path& path)
{
    ScopeFileLoadStatus status{true, {}};
    const auto json = ReadTextFile(path, status);
    SolutionScopeFile file;
    file.schemaVersion = ReadIntField(json, "schemaVersion").value_or(0);
    file.id = ReadStringField(json, "id").value_or("");
    file.displayName = ReadStringField(json, "displayName").value_or("");
    file.artists = ReadReferenceArray(json, "artists");
    file.projects = ReadReferenceArray(json, "projects");

    ValidateFormatAndScope(status, json, SolutionFormat, "solution");
    RequireField(status, file.id, "id");
    RequireField(status, file.displayName, "displayName");
    auto referenceIssues = ValidateRelativeReferences(file);
    status.issues.insert(status.issues.end(), referenceIssues.begin(), referenceIssues.end());
    status.ok = status.issues.empty();
    return {status, file};
}

ArtistScopeFileLoadResult LoadArtistScopeFile(const std::filesystem::path& path)
{
    ScopeFileLoadStatus status{true, {}};
    const auto json = ReadTextFile(path, status);
    ArtistScopeFile file;
    file.schemaVersion = ReadIntField(json, "schemaVersion").value_or(0);
    file.id = ReadStringField(json, "id").value_or("");
    file.displayName = ReadStringField(json, "displayName").value_or("");
    file.ruleFiles = ReadStringArray(json, "ruleFiles");
    file.projects = ReadReferenceArray(json, "projects");

    ValidateFormatAndScope(status, json, ArtistFormat, "artist");
    RequireField(status, file.id, "id");
    RequireField(status, file.displayName, "displayName");
    auto referenceIssues = ValidateRelativeReferences(file);
    status.issues.insert(status.issues.end(), referenceIssues.begin(), referenceIssues.end());
    status.ok = status.issues.empty();
    return {status, file};
}

SeriesScopeFileLoadResult LoadSeriesScopeFile(const std::filesystem::path& path)
{
    ScopeFileLoadStatus status{true, {}};
    const auto json = ReadTextFile(path, status);
    SeriesScopeFile file;
    file.schemaVersion = ReadIntField(json, "schemaVersion").value_or(0);
    file.id = ReadStringField(json, "id").value_or("");
    file.displayName = ReadStringField(json, "displayName").value_or("");
    file.artist = ReadReferenceObject(json, "artist");
    file.works = ReadReferenceArray(json, "works");

    ValidateFormatAndScope(status, json, SeriesFormat, "series");
    RequireField(status, file.id, "id");
    RequireField(status, file.displayName, "displayName");
    auto referenceIssues = ValidateRelativeReferences(file);
    status.issues.insert(status.issues.end(), referenceIssues.begin(), referenceIssues.end());
    status.ok = status.issues.empty();
    return {status, file};
}

WorkScopeFileLoadResult LoadWorkScopeFile(const std::filesystem::path& path)
{
    ScopeFileLoadStatus status{true, {}};
    const auto json = ReadTextFile(path, status);
    WorkScopeFile file;
    file.schemaVersion = ReadIntField(json, "schemaVersion").value_or(0);
    file.id = ReadStringField(json, "id").value_or("");
    file.workKind = ReadStringField(json, "workKind").value_or("");
    file.displayName = ReadStringField(json, "displayName").value_or("");
    file.series = ReadReferenceObject(json, "series");
    file.sources = ReadReferenceArray(json, "sources");
    file.historyPath = ReadObjectPath(json, "history");

    ValidateFormatAndScope(status, json, WorkFormat, "work");
    RequireField(status, file.id, "id");
    RequireField(status, file.workKind, "workKind");
    RequireField(status, file.displayName, "displayName");
    auto referenceIssues = ValidateRelativeReferences(file);
    status.issues.insert(status.issues.end(), referenceIssues.begin(), referenceIssues.end());
    status.ok = status.issues.empty();
    return {status, file};
}

ScopeFileLoadStatus SaveSolutionScopeFile(const std::filesystem::path& path, const SolutionScopeFile& file)
{
    auto issues = ValidateRelativeReferences(file);
    if (!issues.empty()) {
        return FinalStatus(std::move(issues));
    }

    std::ostringstream output;
    output << "{\n";
    output << "  " << Key("format") << Quote(SolutionFormat) << ",\n";
    output << "  " << Key("schemaVersion") << file.schemaVersion << ",\n";
    output << "  " << Key("id") << Quote(file.id) << ",\n";
    output << "  " << Key("scope") << Quote("solution") << ",\n";
    output << "  " << Key("displayName") << Quote(file.displayName) << ",\n";
    AppendReferenceArrayJson(output, "artists", file.artists);
    output << ",\n";
    AppendReferenceArrayJson(output, "projects", file.projects);
    output << ",\n";
    output << "  " << Key("metadata") << "{}\n";
    output << "}\n";
    return WriteTextFile(path, output.str());
}

ScopeFileLoadStatus SaveArtistScopeFile(const std::filesystem::path& path, const ArtistScopeFile& file)
{
    auto issues = ValidateRelativeReferences(file);
    if (!issues.empty()) {
        return FinalStatus(std::move(issues));
    }

    std::ostringstream output;
    output << "{\n";
    output << "  " << Key("format") << Quote(ArtistFormat) << ",\n";
    output << "  " << Key("schemaVersion") << file.schemaVersion << ",\n";
    output << "  " << Key("id") << Quote(file.id) << ",\n";
    output << "  " << Key("scope") << Quote("artist") << ",\n";
    output << "  " << Key("displayName") << Quote(file.displayName) << ",\n";
    output << "  " << Key("ruleFiles") << "[\n";
    for (std::size_t index = 0; index < file.ruleFiles.size(); ++index) {
        output << "    " << Quote(file.ruleFiles[index]) << (index + 1 < file.ruleFiles.size() ? "," : "") << "\n";
    }
    output << "  ],\n";
    AppendReferenceArrayJson(output, "projects", file.projects);
    output << ",\n";
    output << "  " << Key("metadata") << "{}\n";
    output << "}\n";
    return WriteTextFile(path, output.str());
}

ScopeFileLoadStatus SaveSeriesScopeFile(const std::filesystem::path& path, const SeriesScopeFile& file)
{
    auto issues = ValidateRelativeReferences(file);
    if (!issues.empty()) {
        return FinalStatus(std::move(issues));
    }

    std::ostringstream output;
    output << "{\n";
    output << "  " << Key("format") << Quote(SeriesFormat) << ",\n";
    output << "  " << Key("schemaVersion") << file.schemaVersion << ",\n";
    output << "  " << Key("id") << Quote(file.id) << ",\n";
    output << "  " << Key("scope") << Quote("series") << ",\n";
    output << "  " << Key("displayName") << Quote(file.displayName) << ",\n";
    output << "  " << Key("artist") << "{\n";
    if (file.artist) {
        output << "    " << Key("id") << Quote(file.artist->id) << ",\n";
        output << "    " << Key("path") << Quote(file.artist->path) << "\n";
    }
    output << "  },\n";
    AppendReferenceArrayJson(output, "works", file.works);
    output << ",\n";
    output << "  " << Key("metadata") << "{}\n";
    output << "}\n";
    return WriteTextFile(path, output.str());
}

ScopeFileLoadStatus SaveWorkScopeFile(const std::filesystem::path& path, const WorkScopeFile& file)
{
    auto issues = ValidateRelativeReferences(file);
    if (!issues.empty()) {
        return FinalStatus(std::move(issues));
    }

    std::ostringstream output;
    output << "{\n";
    output << "  " << Key("format") << Quote(WorkFormat) << ",\n";
    output << "  " << Key("schemaVersion") << file.schemaVersion << ",\n";
    output << "  " << Key("id") << Quote(file.id) << ",\n";
    output << "  " << Key("scope") << Quote("work") << ",\n";
    output << "  " << Key("workKind") << Quote(file.workKind) << ",\n";
    output << "  " << Key("displayName") << Quote(file.displayName) << ",\n";
    output << "  " << Key("series") << "{\n";
    if (file.series) {
        output << "    " << Key("id") << Quote(file.series->id) << ",\n";
        output << "    " << Key("path") << Quote(file.series->path) << "\n";
    }
    output << "  },\n";
    AppendReferenceArrayJson(output, "sources", file.sources);
    output << ",\n";
    output << "  " << Key("history") << "{\n";
    output << "    " << Key("path") << Quote(file.historyPath.value_or("")) << "\n";
    output << "  },\n";
    output << "  " << Key("metadata") << "{}\n";
    output << "}\n";
    return WriteTextFile(path, output.str());
}

}
