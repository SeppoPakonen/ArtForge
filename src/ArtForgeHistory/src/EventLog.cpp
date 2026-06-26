#include "ArtForge/History/EventLog.hpp"

#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>

namespace ArtForge::History {

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

void AddIssue(HistoryLogStatus& status, std::size_t lineNumber, std::string message)
{
    status.issues.push_back({lineNumber, std::move(message)});
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

std::optional<std::string> ReadStringAt(std::string_view json, std::size_t quotePosition, std::size_t& nextPosition)
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
            nextPosition = index + 1;
            return value;
        }

        value += character;
    }

    return std::nullopt;
}

std::optional<std::string> ReadStringField(std::string_view json, std::string_view key)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition) {
        return std::nullopt;
    }

    std::size_t nextPosition = 0;
    return ReadStringAt(json, *valuePosition, nextPosition);
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

std::vector<std::string> ReadStringArrayField(std::string_view json, std::string_view key)
{
    std::vector<std::string> values;
    const auto arrayJson = ReadContainer(json, key, '[', ']');
    if (!arrayJson) {
        return values;
    }

    auto position = std::size_t{0};
    while (position < arrayJson->size()) {
        while (position < arrayJson->size() && (*arrayJson)[position] != '"') {
            ++position;
        }
        if (position >= arrayJson->size()) {
            break;
        }

        std::size_t nextPosition = 0;
        const auto value = ReadStringAt(*arrayJson, position, nextPosition);
        if (!value) {
            break;
        }
        values.push_back(*value);
        position = nextPosition;
    }

    return values;
}

std::optional<HistoryActor> ParseActor(std::string_view value)
{
    if (value == "user") {
        return HistoryActor::User;
    }
    if (value == "AI") {
        return HistoryActor::Ai;
    }
    if (value == "system") {
        return HistoryActor::System;
    }
    return std::nullopt;
}

std::optional<HistoryScope> ParseScope(std::string_view value)
{
    if (value == "solution") {
        return HistoryScope::Solution;
    }
    if (value == "artist") {
        return HistoryScope::Artist;
    }
    if (value == "series") {
        return HistoryScope::Series;
    }
    if (value == "work") {
        return HistoryScope::Work;
    }
    if (value == "fragment") {
        return HistoryScope::Fragment;
    }
    return std::nullopt;
}

std::optional<HistoryOperation> ParseOperation(std::string_view value)
{
    for (const auto operation : ExampleHistoryOperations) {
        if (value == ToDisplayName(operation)) {
            return operation;
        }
    }
    return std::nullopt;
}

bool IsBlank(std::string_view value)
{
    for (const char character : value) {
        if (std::isspace(static_cast<unsigned char>(character)) == 0) {
            return false;
        }
    }
    return true;
}

bool LooksLikeJsonArray(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    const auto last = value.find_last_not_of(" \t\r\n");
    return first != std::string_view::npos && value[first] == '[' && value[last] == ']';
}

HistoryLogStatus ValidateStoredEvent(const StoredHistoryEvent& event)
{
    HistoryLogStatus status{true, {}};
    if (event.id.empty()) {
        AddIssue(status, 0, "missing required field: id");
    }
    if (event.timestamp.empty()) {
        AddIssue(status, 0, "missing required field: timestamp");
    }
    if (event.summary.empty()) {
        AddIssue(status, 0, "missing required field: summary");
    }
    status.ok = status.issues.empty();
    return status;
}

HistoryLogStatus ValidateHistoryEvent(const HistoryEvent& event)
{
    StoredHistoryEvent stored;
    stored.id = event.id.value;
    stored.timestamp = event.timestamp.iso8601Utc;
    stored.actor = event.actor;
    stored.scope = event.scope;
    stored.operation = event.operation;
    stored.summary = event.summary;
    stored.beforeReference = event.beforeReference;
    stored.afterReference = event.afterReference;
    stored.promptPackageId = event.relatedAiIds.promptPackageId;
    stored.aiResultId = event.relatedAiIds.aiResultId;
    auto status = ValidateStoredEvent(stored);
    if (!LooksLikeJsonArray(event.affectedFilesJsonArray)) {
        AddIssue(status, 0, "affectedFilesJsonArray must be a JSON array");
    }
    status.ok = status.issues.empty();
    return status;
}

std::optional<StoredHistoryEvent> ParseHistoryEventLine(std::string_view line, std::size_t lineNumber, HistoryLogStatus& status)
{
    if (IsBlank(line)) {
        AddIssue(status, lineNumber, "empty JSON Lines entry");
        return std::nullopt;
    }

    const auto firstNonSpace = line.find_first_not_of(" \t\r\n");
    if (firstNonSpace == std::string_view::npos || line[firstNonSpace] != '{') {
        AddIssue(status, lineNumber, "history event line must be a JSON object");
        return std::nullopt;
    }

    StoredHistoryEvent event;
    event.id = ReadStringField(line, "id").value_or("");
    event.timestamp = ReadStringField(line, "timestamp").value_or("");
    event.summary = ReadStringField(line, "summary").value_or("");
    event.affectedFiles = ReadStringArrayField(line, "affected_files");
    event.beforeReference = ReadStringField(line, "before").value_or("");
    event.afterReference = ReadStringField(line, "after").value_or("");
    event.promptPackageId = ReadStringField(line, "prompt_package_id").value_or("");
    event.aiResultId = ReadStringField(line, "ai_result_id").value_or("");

    const auto actor = ParseActor(ReadStringField(line, "actor").value_or(""));
    if (!actor) {
        AddIssue(status, lineNumber, "unknown or missing actor");
    } else {
        event.actor = *actor;
    }

    const auto scope = ParseScope(ReadStringField(line, "scope").value_or(""));
    if (!scope) {
        AddIssue(status, lineNumber, "unknown or missing scope");
    } else {
        event.scope = *scope;
    }

    const auto operation = ParseOperation(ReadStringField(line, "operation").value_or(""));
    if (!operation) {
        AddIssue(status, lineNumber, "unknown or missing operation");
    } else {
        event.operation = *operation;
    }

    const auto validation = ValidateStoredEvent(event);
    for (const auto& issue : validation.issues) {
        AddIssue(status, lineNumber, issue.message);
    }

    if (actor && scope && operation && validation.ok) {
        return event;
    }

    return std::nullopt;
}

void AppendAffectedFiles(std::ostringstream& output, const std::vector<std::string>& affectedFiles)
{
    output << '[';
    for (std::size_t index = 0; index < affectedFiles.size(); ++index) {
        output << Quote(affectedFiles[index]);
        if (index + 1 < affectedFiles.size()) {
            output << ',';
        }
    }
    output << ']';
}

HistoryLogStatus WriteJsonLine(const std::filesystem::path& path, std::string_view jsonLine)
{
    HistoryLogStatus status{true, {}};
    std::ofstream output{path, std::ios::app};
    if (!output) {
        AddIssue(status, 0, "could not open history file for append");
        status.ok = false;
        return status;
    }

    output << jsonLine << '\n';
    if (!output) {
        AddIssue(status, 0, "could not write history event");
    }

    status.ok = status.issues.empty();
    return status;
}

}

std::string_view PersistentEventLogName()
{
    return "ArtForge persistent undo/redo event log";
}

std::string_view ToDisplayName(HistoryActor actor)
{
    switch (actor) {
    case HistoryActor::User:
        return "user";
    case HistoryActor::Ai:
        return "AI";
    case HistoryActor::System:
        return "system";
    }

    return "unknown";
}

std::string_view ToDisplayName(HistoryScope scope)
{
    switch (scope) {
    case HistoryScope::Solution:
        return "solution";
    case HistoryScope::Artist:
        return "artist";
    case HistoryScope::Series:
        return "series";
    case HistoryScope::Work:
        return "work";
    case HistoryScope::Fragment:
        return "fragment";
    }

    return "unknown";
}

std::string_view ToDisplayName(HistoryOperation operation)
{
    switch (operation) {
    case HistoryOperation::UserTextEdit:
        return "user text edit";
    case HistoryOperation::AcceptedAiRepairOption:
        return "accepted AI repair option";
    case HistoryOperation::RejectedAiSuggestion:
        return "rejected AI suggestion";
    case HistoryOperation::ImportedSourceFragment:
        return "imported source fragment";
    case HistoryOperation::RuleWeightChanged:
        return "rule weight changed";
    case HistoryOperation::PromptPackageGenerated:
        return "prompt package generated";
    case HistoryOperation::AiJsonResultImported:
        return "AI JSON result imported";
    case HistoryOperation::SnapshotCreated:
        return "snapshot created";
    case HistoryOperation::BranchCreated:
        return "branch created";
    }

    return "unknown";
}

HistoryStorageConvention DefaultStorageConvention()
{
    return {
        "history.afhistory.jsonl",
        "snapshots/",
        "000001.json",
        true,
        true,
    };
}

HistoryItemFields PlannedHistoryItemFields()
{
    return {
        "id",
        "timestamp",
        "actor",
        "scope",
        "affected_files",
        "operation",
        "summary",
        "before",
        "after",
        "prompt_package_id",
        "ai_result_id",
    };
}

HistoryEvent SampleUserTextEditEvent()
{
    return {
        {"hist.sample.0001"},
        {"2026-06-25T00:00:00Z"},
        HistoryActor::User,
        HistoryScope::Work,
        HistoryOperation::UserTextEdit,
        "Edited opening lyric draft",
        R"(["work.afwork","drafts/opening.md"])",
        "snapshots/000001.json",
        "snapshots/000002.json",
        {"", ""},
    };
}

std::string_view SampleUserTextEditJsonLine()
{
    return R"({"id":"hist.sample.0001","timestamp":"2026-06-25T00:00:00Z","actor":"user","scope":"work","operation":"user text edit","summary":"Edited opening lyric draft","affected_files":["work.afwork","drafts/opening.md"],"before":"snapshots/000001.json","after":"snapshots/000002.json","prompt_package_id":"","ai_result_id":""})";
}

std::string SerializeHistoryEventJsonLine(const HistoryEvent& event)
{
    std::ostringstream output;
    output << "{";
    output << "\"id\":" << Quote(event.id.value) << ",";
    output << "\"timestamp\":" << Quote(event.timestamp.iso8601Utc) << ",";
    output << "\"actor\":" << Quote(ToDisplayName(event.actor)) << ",";
    output << "\"scope\":" << Quote(ToDisplayName(event.scope)) << ",";
    output << "\"operation\":" << Quote(ToDisplayName(event.operation)) << ",";
    output << "\"summary\":" << Quote(event.summary) << ",";
    output << "\"affected_files\":" << event.affectedFilesJsonArray << ",";
    output << "\"before\":" << Quote(event.beforeReference) << ",";
    output << "\"after\":" << Quote(event.afterReference) << ",";
    output << "\"prompt_package_id\":" << Quote(event.relatedAiIds.promptPackageId) << ",";
    output << "\"ai_result_id\":" << Quote(event.relatedAiIds.aiResultId);
    output << "}";
    return output.str();
}

std::string SerializeHistoryEventJsonLine(const StoredHistoryEvent& event)
{
    std::ostringstream output;
    output << "{";
    output << "\"id\":" << Quote(event.id) << ",";
    output << "\"timestamp\":" << Quote(event.timestamp) << ",";
    output << "\"actor\":" << Quote(ToDisplayName(event.actor)) << ",";
    output << "\"scope\":" << Quote(ToDisplayName(event.scope)) << ",";
    output << "\"operation\":" << Quote(ToDisplayName(event.operation)) << ",";
    output << "\"summary\":" << Quote(event.summary) << ",";
    output << "\"affected_files\":";
    AppendAffectedFiles(output, event.affectedFiles);
    output << ",";
    output << "\"before\":" << Quote(event.beforeReference) << ",";
    output << "\"after\":" << Quote(event.afterReference) << ",";
    output << "\"prompt_package_id\":" << Quote(event.promptPackageId) << ",";
    output << "\"ai_result_id\":" << Quote(event.aiResultId);
    output << "}";
    return output.str();
}

HistoryLogStatus AppendHistoryEventJsonLine(const std::filesystem::path& path, const HistoryEvent& event)
{
    auto status = ValidateHistoryEvent(event);
    if (!status.ok) {
        return status;
    }

    return WriteJsonLine(path, SerializeHistoryEventJsonLine(event));
}

HistoryLogStatus AppendHistoryEventJsonLine(const std::filesystem::path& path, const StoredHistoryEvent& event)
{
    auto status = ValidateStoredEvent(event);
    if (!status.ok) {
        return status;
    }

    return WriteJsonLine(path, SerializeHistoryEventJsonLine(event));
}

HistoryLogReadResult ReadHistoryEventJsonLines(const std::filesystem::path& path)
{
    HistoryLogReadResult result{{true, {}}, {}};
    std::ifstream input{path};
    if (!input) {
        AddIssue(result.status, 0, "could not open history file for read");
        result.status.ok = false;
        return result;
    }

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        auto event = ParseHistoryEventLine(line, lineNumber, result.status);
        if (event) {
            result.events.push_back(std::move(*event));
        }
    }

    if (input.bad()) {
        AddIssue(result.status, 0, "could not read history file");
    }

    result.status.ok = result.status.issues.empty();
    return result;
}

std::string_view CreateHistoryItemOperationName()
{
    return "create history item";
}

std::string_view ListHistoryItemsOperationName()
{
    return "list history items";
}

std::string_view RestoreSnapshotPlaceholderName()
{
    return "restore snapshot placeholder";
}

std::string_view BranchMetadataPlaceholderName()
{
    return "branch metadata placeholder";
}

}
