#include "ArtForge/History/EventLog.hpp"

#include <charconv>
#include <cctype>
#include <chrono>
#include <fstream>
#include <optional>
#include <sstream>
#include <iomanip>
#include <ctime>

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

std::string PathUtf8(const std::filesystem::path& path)
{
    return path.generic_string();
}

std::string CurrentUtcTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time);

    std::ostringstream output;
    output << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return output.str();
}

std::string CompactTimestampForId(std::string_view timestamp)
{
    std::string compact;
    for (const char character : timestamp) {
        if (std::isalnum(static_cast<unsigned char>(character)) != 0) {
            compact += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
    }
    return compact;
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

std::optional<int> ReadIntField(std::string_view json, std::string_view key)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition) {
        return std::nullopt;
    }

    auto endPosition = *valuePosition;
    if (endPosition < json.size() && json[endPosition] == '-') {
        ++endPosition;
    }
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

std::optional<HistorySnapshotMetadata> ReadSnapshotMetadataField(std::string_view json)
{
    const auto objectJson = ReadContainer(json, "snapshot", '{', '}');
    if (!objectJson) {
        return std::nullopt;
    }

    return HistorySnapshotMetadata{
        ReadStringField(*objectJson, "id").value_or(""),
        ReadStringField(*objectJson, "summary").value_or(""),
        ReadStringField(*objectJson, "timestamp").value_or(""),
        ReadStringField(*objectJson, "related_event_id").value_or(""),
        ReadStringField(*objectJson, "reason").value_or(""),
        ReadStringField(*objectJson, "related_suggestion_id").value_or(""),
        ReadStringField(*objectJson, "work_id").value_or(""),
        ReadStringField(*objectJson, "work_path").value_or(""),
        ReadStringField(*objectJson, "domain").value_or(""),
        ReadStringField(*objectJson, "item_type").value_or(""),
        ReadStringField(*objectJson, "item_id").value_or(""),
        ReadIntField(*objectJson, "item_index").value_or(0),
    };
}

std::optional<HistoryBranchPlaceholder> ReadBranchPlaceholderField(std::string_view json)
{
    const auto objectJson = ReadContainer(json, "branch", '{', '}');
    if (!objectJson) {
        return std::nullopt;
    }

    return HistoryBranchPlaceholder{
        ReadStringField(*objectJson, "id").value_or(""),
        ReadStringField(*objectJson, "parent_branch_id").value_or(""),
        ReadStringField(*objectJson, "creation_reason").value_or(""),
    };
}

std::optional<DomainItemHistoryMetadata> ReadDomainItemMetadataField(std::string_view json)
{
    const auto objectJson = ReadContainer(json, "domain_item", '{', '}');
    if (!objectJson) {
        return std::nullopt;
    }

    return DomainItemHistoryMetadata{
        ReadStringField(*objectJson, "work_id").value_or(""),
        ReadStringField(*objectJson, "work_path").value_or(""),
        ReadStringField(*objectJson, "domain").value_or(""),
        ReadStringField(*objectJson, "item_type").value_or(""),
        ReadStringField(*objectJson, "item_id").value_or(""),
        ReadIntField(*objectJson, "item_index").value_or(0),
        ReadStringField(*objectJson, "operation_summary").value_or(""),
    };
}

std::optional<PresentationCommandHistoryMetadata> ReadPresentationCommandMetadataField(std::string_view json)
{
    const auto objectJson = ReadContainer(json, "presentation_command", '{', '}');
    if (!objectJson) {
        return std::nullopt;
    }

    return PresentationCommandHistoryMetadata{
        ReadStringField(*objectJson, "work_id").value_or(""),
        ReadStringField(*objectJson, "work_path").value_or(""),
        ReadStringField(*objectJson, "domain").value_or(""),
        ReadStringField(*objectJson, "item_type").value_or(""),
        ReadStringField(*objectJson, "item_id").value_or(""),
        ReadIntField(*objectJson, "item_index").value_or(0),
        ReadStringField(*objectJson, "command_name").value_or(""),
        ReadStringField(*objectJson, "result_status").value_or(""),
    };
}

std::optional<ChangeSetHistoryMetadata> ReadChangeSetMetadataField(std::string_view json)
{
    const auto objectJson = ReadContainer(json, "change_set", '{', '}');
    if (!objectJson) {
        return std::nullopt;
    }

    return ChangeSetHistoryMetadata{
        ReadStringField(*objectJson, "work_id").value_or(""),
        ReadStringField(*objectJson, "work_path").value_or(""),
        ReadStringField(*objectJson, "domain").value_or(""),
        ReadStringField(*objectJson, "item_type").value_or(""),
        ReadStringField(*objectJson, "item_id").value_or(""),
        ReadIntField(*objectJson, "item_index").value_or(0),
        ReadStringField(*objectJson, "command_id").value_or(""),
        ReadIntField(*objectJson, "change_count").value_or(0),
        ReadStringField(*objectJson, "operation_summary").value_or(""),
    };
}

std::optional<SuggestionReviewHistoryMetadata> ReadSuggestionReviewMetadataField(std::string_view json)
{
    const auto objectJson = ReadContainer(json, "suggestion_review", '{', '}');
    if (!objectJson) {
        return std::nullopt;
    }

    return SuggestionReviewHistoryMetadata{
        ReadStringField(*objectJson, "suggestion_id").value_or(""),
        ReadStringField(*objectJson, "source_request_path").value_or(""),
        ReadStringField(*objectJson, "source_result_path").value_or(""),
        ReadStringField(*objectJson, "work_id").value_or(""),
        ReadStringField(*objectJson, "work_path").value_or(""),
        ReadStringField(*objectJson, "domain").value_or(""),
        ReadStringField(*objectJson, "item_type").value_or(""),
        ReadStringField(*objectJson, "item_id").value_or(""),
        ReadIntField(*objectJson, "item_index").value_or(0),
        ReadStringField(*objectJson, "status").value_or(""),
        ReadStringField(*objectJson, "reason_or_diagnostic").value_or(""),
    };
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
    if (event.operation == HistoryOperation::SnapshotCreated) {
        if (!event.snapshot) {
            AddIssue(status, 0, "snapshot metadata is required for snapshot created events");
        } else {
            if (event.snapshot->snapshotId.empty()) {
                AddIssue(status, 0, "missing required field: snapshot.id");
            }
            if (event.snapshot->summary.empty()) {
                AddIssue(status, 0, "missing required field: snapshot.summary");
            }
            if (event.snapshot->timestamp.empty()) {
                AddIssue(status, 0, "missing required field: snapshot.timestamp");
            }
            if (event.snapshot->relatedEventId.empty()) {
                AddIssue(status, 0, "missing required field: snapshot.related_event_id");
            }
        }
    }
    if (event.operation == HistoryOperation::BranchCreated) {
        if (!event.branch) {
            AddIssue(status, 0, "branch metadata is required for branch created events");
        } else {
            if (event.branch->branchId.empty()) {
                AddIssue(status, 0, "missing required field: branch.id");
            }
            if (event.branch->creationReason.empty()) {
                AddIssue(status, 0, "missing required field: branch.creation_reason");
            }
        }
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
    event.snapshot = ReadSnapshotMetadataField(line);
    event.branch = ReadBranchPlaceholderField(line);
    event.domainItem = ReadDomainItemMetadataField(line);
    event.presentationCommand = ReadPresentationCommandMetadataField(line);
    event.changeSet = ReadChangeSetMetadataField(line);
    event.suggestionReview = ReadSuggestionReviewMetadataField(line);

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

void AppendSnapshotMetadata(std::ostringstream& output, const HistorySnapshotMetadata& snapshot)
{
    output << ",\"snapshot\":{";
    output << "\"id\":" << Quote(snapshot.snapshotId) << ",";
    output << "\"summary\":" << Quote(snapshot.summary) << ",";
    output << "\"timestamp\":" << Quote(snapshot.timestamp) << ",";
    output << "\"related_event_id\":" << Quote(snapshot.relatedEventId);
    if (!snapshot.reason.empty()) {
        output << ",\"reason\":" << Quote(snapshot.reason);
    }
    if (!snapshot.relatedSuggestionId.empty()) {
        output << ",\"related_suggestion_id\":" << Quote(snapshot.relatedSuggestionId);
    }
    if (!snapshot.workPath.empty()) {
        output << ",\"work_id\":" << Quote(snapshot.workId);
        output << ",\"work_path\":" << Quote(snapshot.workPath);
        output << ",\"domain\":" << Quote(snapshot.domain);
        output << ",\"item_type\":" << Quote(snapshot.itemType);
        output << ",\"item_id\":" << Quote(snapshot.itemId);
        output << ",\"item_index\":" << snapshot.itemIndex;
    }
    output << "}";
}

void AppendBranchPlaceholder(std::ostringstream& output, const HistoryBranchPlaceholder& branch)
{
    output << ",\"branch\":{";
    output << "\"id\":" << Quote(branch.branchId) << ",";
    output << "\"parent_branch_id\":" << Quote(branch.parentBranchId) << ",";
    output << "\"creation_reason\":" << Quote(branch.creationReason);
    output << "}";
}

void AppendDomainItemMetadata(std::ostringstream& output, const DomainItemHistoryMetadata& metadata)
{
    output << ",\"domain_item\":{";
    output << "\"work_id\":" << Quote(metadata.workId) << ",";
    output << "\"work_path\":" << Quote(metadata.workPath) << ",";
    output << "\"domain\":" << Quote(metadata.domain) << ",";
    output << "\"item_type\":" << Quote(metadata.itemType) << ",";
    output << "\"item_id\":" << Quote(metadata.itemId) << ",";
    output << "\"item_index\":" << metadata.itemIndex << ",";
    output << "\"operation_summary\":" << Quote(metadata.operationSummary);
    output << "}";
}

void AppendPresentationCommandMetadata(std::ostringstream& output, const PresentationCommandHistoryMetadata& metadata)
{
    output << ",\"presentation_command\":{";
    output << "\"work_id\":" << Quote(metadata.workId) << ",";
    output << "\"work_path\":" << Quote(metadata.workPath) << ",";
    output << "\"domain\":" << Quote(metadata.domain) << ",";
    output << "\"item_type\":" << Quote(metadata.itemType) << ",";
    output << "\"item_id\":" << Quote(metadata.itemId) << ",";
    output << "\"item_index\":" << metadata.itemIndex << ",";
    output << "\"command_name\":" << Quote(metadata.commandName) << ",";
    output << "\"result_status\":" << Quote(metadata.resultStatus);
    output << "}";
}

void AppendChangeSetMetadata(std::ostringstream& output, const ChangeSetHistoryMetadata& metadata)
{
    output << ",\"change_set\":{";
    output << "\"work_id\":" << Quote(metadata.workId) << ",";
    output << "\"work_path\":" << Quote(metadata.workPath) << ",";
    output << "\"domain\":" << Quote(metadata.domain) << ",";
    output << "\"item_type\":" << Quote(metadata.itemType) << ",";
    output << "\"item_id\":" << Quote(metadata.itemId) << ",";
    output << "\"item_index\":" << metadata.itemIndex << ",";
    output << "\"command_id\":" << Quote(metadata.commandId) << ",";
    output << "\"change_count\":" << metadata.changeCount << ",";
    output << "\"operation_summary\":" << Quote(metadata.operationSummary);
    output << "}";
}

void AppendSuggestionReviewMetadata(std::ostringstream& output, const SuggestionReviewHistoryMetadata& metadata)
{
    output << ",\"suggestion_review\":{";
    output << "\"suggestion_id\":" << Quote(metadata.suggestionId) << ",";
    output << "\"source_request_path\":" << Quote(metadata.sourceRequestPath) << ",";
    output << "\"source_result_path\":" << Quote(metadata.sourceResultPath) << ",";
    output << "\"work_id\":" << Quote(metadata.workId) << ",";
    output << "\"work_path\":" << Quote(metadata.workPath) << ",";
    output << "\"domain\":" << Quote(metadata.domain) << ",";
    output << "\"item_type\":" << Quote(metadata.itemType) << ",";
    output << "\"item_id\":" << Quote(metadata.itemId) << ",";
    output << "\"item_index\":" << metadata.itemIndex << ",";
    output << "\"status\":" << Quote(metadata.status) << ",";
    output << "\"reason_or_diagnostic\":" << Quote(metadata.reasonOrDiagnostic);
    output << "}";
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
    case HistoryOperation::SuggestionImported:
        return "suggestion imported";
    case HistoryOperation::SuggestionReviewOpened:
        return "suggestion review opened";
    case HistoryOperation::SuggestionAccepted:
        return "suggestion accepted";
    case HistoryOperation::SuggestionRejected:
        return "suggestion rejected";
    case HistoryOperation::SuggestionApplyRequested:
        return "suggestion apply requested";
    case HistoryOperation::SuggestionApplySucceeded:
        return "suggestion apply succeeded";
    case HistoryOperation::SuggestionApplyFailed:
        return "suggestion apply failed";
    case HistoryOperation::SnapshotCreated:
        return "snapshot created";
    case HistoryOperation::BranchCreated:
        return "branch created";
    case HistoryOperation::FileOpenAttempted:
        return "file open attempted";
    case HistoryOperation::FileLoadSucceeded:
        return "file load succeeded";
    case HistoryOperation::FileLoadFailed:
        return "file load failed";
    case HistoryOperation::FileSaveSucceeded:
        return "file save succeeded";
    case HistoryOperation::DomainItemSelected:
        return "domain item selected";
    case HistoryOperation::PromptRequested:
        return "prompt requested";
    case HistoryOperation::CritiqueRequested:
        return "critique requested";
    case HistoryOperation::RepairAlternativesRequested:
        return "repair alternatives requested";
    case HistoryOperation::ItemFlaggedForReview:
        return "item flagged for review";
    case HistoryOperation::PresentationSelectionChanged:
        return "presentation selection changed";
    case HistoryOperation::PromptPreviewRequested:
        return "prompt preview requested";
    case HistoryOperation::SelectedItemPromptPackageBuilt:
        return "selected item prompt package built";
    case HistoryOperation::EditCommandRequested:
        return "edit command requested";
    case HistoryOperation::ChangeSetValidated:
        return "change set validated";
    case HistoryOperation::ChangeSetApplied:
        return "change set applied";
    case HistoryOperation::SaveRequested:
        return "save requested";
    case HistoryOperation::SaveSucceeded:
        return "save succeeded";
    case HistoryOperation::SaveFailed:
        return "save failed";
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

StoredHistoryEvent SampleSnapshotMetadataEvent()
{
    StoredHistoryEvent event;
    event.id = "hist.sample.0002";
    event.timestamp = "2026-06-25T00:05:00Z";
    event.actor = HistoryActor::System;
    event.scope = HistoryScope::Work;
    event.operation = HistoryOperation::SnapshotCreated;
    event.summary = "Recorded snapshot metadata after text edit";
    event.affectedFiles = {"work.afwork", "snapshots/000002.json"};
    event.afterReference = "snapshots/000002.json";
    event.snapshot = HistorySnapshotMetadata{
        "snapshot.sample.0002",
        "Opening work after first text edit",
        "2026-06-25T00:05:00Z",
        "hist.sample.0001",
    };
    return event;
}

StoredHistoryEvent SampleBranchPlaceholderEvent()
{
    StoredHistoryEvent event;
    event.id = "hist.sample.0003";
    event.timestamp = "2026-06-25T00:10:00Z";
    event.actor = HistoryActor::User;
    event.scope = HistoryScope::Work;
    event.operation = HistoryOperation::BranchCreated;
    event.summary = "Created alternate chorus branch placeholder";
    event.affectedFiles = {"work.afwork"};
    event.branch = HistoryBranchPlaceholder{
        "branch.alt-chorus",
        "branch.main",
        "Explore a lower-energy chorus without changing main history yet",
    };
    return event;
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
    if (event.snapshot) {
        AppendSnapshotMetadata(output, *event.snapshot);
    }
    if (event.branch) {
        AppendBranchPlaceholder(output, *event.branch);
    }
    if (event.domainItem) {
        AppendDomainItemMetadata(output, *event.domainItem);
    }
    if (event.presentationCommand) {
        AppendPresentationCommandMetadata(output, *event.presentationCommand);
    }
    if (event.changeSet) {
        AppendChangeSetMetadata(output, *event.changeSet);
    }
    if (event.suggestionReview) {
        AppendSuggestionReviewMetadata(output, *event.suggestionReview);
    }
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

std::filesystem::path DefaultOperationHistoryPath(const std::filesystem::path& scopeFilePath)
{
    const auto parent = scopeFilePath.has_parent_path() ? scopeFilePath.parent_path() : std::filesystem::path{"."};
    return parent / "operations.afhistory.jsonl";
}

StoredHistoryEvent CreateFileOperationHistoryEvent(
    HistoryScope scope,
    HistoryOperation operation,
    const std::filesystem::path& affectedFile,
    std::string_view summary,
    std::string_view detail)
{
    const auto timestamp = CurrentUtcTimestamp();
    const auto operationName = ToDisplayName(operation);

    StoredHistoryEvent event;
    event.id = "hist.operation." + CompactTimestampForId(timestamp) + "." + std::to_string(std::hash<std::string>{}(PathUtf8(affectedFile) + std::string{operationName}));
    event.timestamp = timestamp;
    event.actor = HistoryActor::System;
    event.scope = scope;
    event.operation = operation;
    event.summary = std::string{summary};
    if (!detail.empty()) {
        event.summary += ": ";
        event.summary += detail;
    }
    event.affectedFiles = {PathUtf8(affectedFile)};
    return event;
}

HistoryLogStatus RecordFileOperationHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryScope scope,
    HistoryOperation operation,
    std::string_view summary,
    std::string_view detail)
{
    try {
        return AppendHistoryEventJsonLine(
            DefaultOperationHistoryPath(scopeFilePath),
            CreateFileOperationHistoryEvent(scope, operation, scopeFilePath, summary, detail));
    } catch (const std::exception& exception) {
        return {false, {{0, std::string{"history recording failed: "} + exception.what()}}};
    } catch (...) {
        return {false, {{0, "history recording failed"}}};
    }
}

StoredHistoryEvent CreateDomainItemHistoryEvent(
    HistoryOperation operation,
    const DomainItemHistoryMetadata& metadata)
{
    const auto timestamp = CurrentUtcTimestamp();
    const auto operationName = ToDisplayName(operation);
    const auto identity = metadata.workPath + metadata.domain + metadata.itemType + metadata.itemId + std::to_string(metadata.itemIndex) + std::string{operationName};

    StoredHistoryEvent event;
    event.id = "hist.domain." + CompactTimestampForId(timestamp) + "." + std::to_string(std::hash<std::string>{}(identity));
    event.timestamp = timestamp;
    event.actor = HistoryActor::User;
    event.scope = HistoryScope::Work;
    event.operation = operation;
    event.summary = metadata.operationSummary.empty() ? std::string{operationName} : metadata.operationSummary;
    if (!metadata.workPath.empty()) {
        event.affectedFiles = {metadata.workPath};
    }
    event.domainItem = metadata;
    return event;
}

HistoryLogStatus RecordDomainItemHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryOperation operation,
    const DomainItemHistoryMetadata& metadata)
{
    try {
        return AppendHistoryEventJsonLine(
            DefaultOperationHistoryPath(scopeFilePath),
            CreateDomainItemHistoryEvent(operation, metadata));
    } catch (const std::exception& exception) {
        return {false, {{0, std::string{"history recording failed: "} + exception.what()}}};
    } catch (...) {
        return {false, {{0, "history recording failed"}}};
    }
}

StoredHistoryEvent CreatePresentationCommandHistoryEvent(
    HistoryOperation operation,
    const PresentationCommandHistoryMetadata& metadata)
{
    const auto timestamp = CurrentUtcTimestamp();
    const auto operationName = ToDisplayName(operation);
    const auto identity = metadata.workPath + metadata.domain + metadata.itemType + metadata.itemId + std::to_string(metadata.itemIndex) + metadata.commandName + metadata.resultStatus + std::string{operationName};

    StoredHistoryEvent event;
    event.id = "hist.presentation." + CompactTimestampForId(timestamp) + "." + std::to_string(std::hash<std::string>{}(identity));
    event.timestamp = timestamp;
    event.actor = HistoryActor::User;
    event.scope = HistoryScope::Work;
    event.operation = operation;
    event.summary = std::string{operationName} + ": " + metadata.commandName + " -> " + metadata.resultStatus;
    if (!metadata.workPath.empty()) {
        event.affectedFiles = {metadata.workPath};
    }
    event.presentationCommand = metadata;
    return event;
}

HistoryLogStatus RecordPresentationCommandHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryOperation operation,
    const PresentationCommandHistoryMetadata& metadata)
{
    try {
        return AppendHistoryEventJsonLine(
            DefaultOperationHistoryPath(scopeFilePath),
            CreatePresentationCommandHistoryEvent(operation, metadata));
    } catch (const std::exception& exception) {
        return {false, {{0, std::string{"history recording failed: "} + exception.what()}}};
    } catch (...) {
        return {false, {{0, "history recording failed"}}};
    }
}

StoredHistoryEvent CreateChangeSetHistoryEvent(
    HistoryOperation operation,
    const ChangeSetHistoryMetadata& metadata)
{
    const auto timestamp = CurrentUtcTimestamp();
    const auto operationName = ToDisplayName(operation);
    const auto identity = metadata.workPath + metadata.domain + metadata.itemType + metadata.itemId + std::to_string(metadata.itemIndex) + metadata.commandId + std::to_string(metadata.changeCount) + std::string{operationName};

    StoredHistoryEvent event;
    event.id = "hist.changeset." + CompactTimestampForId(timestamp) + "." + std::to_string(std::hash<std::string>{}(identity));
    event.timestamp = timestamp;
    event.actor = HistoryActor::User;
    event.scope = HistoryScope::Work;
    event.operation = operation;
    event.summary = metadata.operationSummary.empty() ? std::string{operationName} : metadata.operationSummary;
    if (!metadata.workPath.empty()) {
        event.affectedFiles = {metadata.workPath};
    }
    event.changeSet = metadata;
    return event;
}

HistoryLogStatus RecordChangeSetHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryOperation operation,
    const ChangeSetHistoryMetadata& metadata)
{
    try {
        return AppendHistoryEventJsonLine(
            DefaultOperationHistoryPath(scopeFilePath),
            CreateChangeSetHistoryEvent(operation, metadata));
    } catch (const std::exception& exception) {
        return {false, {{0, std::string{"history recording failed: "} + exception.what()}}};
    } catch (...) {
        return {false, {{0, "history recording failed"}}};
    }
}

StoredHistoryEvent CreateSuggestionReviewHistoryEvent(
    HistoryOperation operation,
    const SuggestionReviewHistoryMetadata& metadata)
{
    const auto timestamp = CurrentUtcTimestamp();
    const auto operationName = ToDisplayName(operation);
    const auto identity = metadata.suggestionId + metadata.workPath + metadata.domain + metadata.itemType + metadata.itemId + std::to_string(metadata.itemIndex) + std::string{operationName};

    StoredHistoryEvent event;
    event.id = "hist.suggestion." + CompactTimestampForId(timestamp) + "." + std::to_string(std::hash<std::string>{}(identity));
    event.timestamp = timestamp;
    event.actor = operation == HistoryOperation::SuggestionImported ? HistoryActor::Ai : HistoryActor::User;
    event.scope = HistoryScope::Work;
    event.operation = operation;
    event.summary = metadata.reasonOrDiagnostic.empty() ? std::string{operationName} : metadata.reasonOrDiagnostic;
    if (!metadata.workPath.empty()) {
        event.affectedFiles = {metadata.workPath};
    }
    if (!metadata.sourceRequestPath.empty()) {
        event.affectedFiles.push_back(metadata.sourceRequestPath);
    }
    if (!metadata.sourceResultPath.empty()) {
        event.affectedFiles.push_back(metadata.sourceResultPath);
    }
    event.promptPackageId = metadata.sourceRequestPath;
    event.aiResultId = metadata.sourceResultPath;
    event.suggestionReview = metadata;
    return event;
}

HistoryLogStatus RecordSuggestionReviewHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryOperation operation,
    const SuggestionReviewHistoryMetadata& metadata)
{
    try {
        return AppendHistoryEventJsonLine(
            DefaultOperationHistoryPath(scopeFilePath),
            CreateSuggestionReviewHistoryEvent(operation, metadata));
    } catch (const std::exception& exception) {
        return {false, {{0, std::string{"history recording failed: "} + exception.what()}}};
    } catch (...) {
        return {false, {{0, "history recording failed"}}};
    }
}

StoredHistoryEvent CreateSuggestionApplySnapshotEvent(const SuggestionReviewHistoryMetadata& metadata)
{
    const auto timestamp = CurrentUtcTimestamp();
    StoredHistoryEvent event;
    event.id = "hist.snapshot." + CompactTimestampForId(timestamp) + "." + std::to_string(std::hash<std::string>{}(metadata.suggestionId + metadata.workPath + metadata.itemId));
    event.timestamp = timestamp;
    event.actor = HistoryActor::System;
    event.scope = HistoryScope::Work;
    event.operation = HistoryOperation::SnapshotCreated;
    event.summary = "Snapshot metadata before suggestion apply: " + metadata.suggestionId;
    if (!metadata.workPath.empty()) {
        event.affectedFiles = {metadata.workPath};
    }
    event.snapshot = HistorySnapshotMetadata{
        "snapshot.before-suggestion." + CompactTimestampForId(timestamp),
        event.summary,
        timestamp,
        metadata.suggestionId,
        "before suggestion apply",
        metadata.suggestionId,
        metadata.workId,
        metadata.workPath,
        metadata.domain,
        metadata.itemType,
        metadata.itemId,
        metadata.itemIndex,
    };
    event.suggestionReview = metadata;
    return event;
}

HistoryLogStatus RecordSuggestionApplySnapshotEvent(
    const std::filesystem::path& scopeFilePath,
    const SuggestionReviewHistoryMetadata& metadata)
{
    try {
        return AppendHistoryEventJsonLine(
            DefaultOperationHistoryPath(scopeFilePath),
            CreateSuggestionApplySnapshotEvent(metadata));
    } catch (const std::exception& exception) {
        return {false, {{0, std::string{"history recording failed: "} + exception.what()}}};
    } catch (...) {
        return {false, {{0, "history recording failed"}}};
    }
}

std::string SampleSuggestionReviewHistoryJsonLines()
{
    const SuggestionReviewHistoryMetadata base{
        "suggestion.sample.lyrics.001",
        "manual-ai-queue/000001-lyrics-line-repair.request.json",
        "manual-ai-queue/000001-lyrics-line-repair.result.json",
        "work.sample.lyrics",
        "examples/work-domains/lyrics.afwork.json",
        "lyrics",
        "lyricLine",
        "line.v1.001",
        0,
        "pending",
        "Imported lyric replacement as pending suggestion",
    };

    std::ostringstream output;
    for (const auto operation : {
             HistoryOperation::SuggestionImported,
             HistoryOperation::SuggestionReviewOpened,
             HistoryOperation::SuggestionAccepted,
             HistoryOperation::SuggestionRejected,
             HistoryOperation::SuggestionApplyRequested,
             HistoryOperation::SuggestionApplySucceeded,
             HistoryOperation::SuggestionApplyFailed,
         }) {
        auto metadata = base;
        metadata.status = std::string{ToDisplayName(operation)};
        metadata.reasonOrDiagnostic = std::string{ToDisplayName(operation)} + " sample";
        output << SerializeHistoryEventJsonLine(CreateSuggestionReviewHistoryEvent(operation, metadata)) << "\n";
    }
    return output.str();
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
