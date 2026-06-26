#pragma once

#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ArtForge::History {

enum class HistoryActor {
    User,
    Ai,
    System
};

enum class HistoryScope {
    Solution,
    Artist,
    Series,
    Work,
    Fragment
};

enum class HistoryOperation {
    UserTextEdit,
    AcceptedAiRepairOption,
    RejectedAiSuggestion,
    ImportedSourceFragment,
    RuleWeightChanged,
    PromptPackageGenerated,
    AiJsonResultImported,
    SnapshotCreated,
    BranchCreated,
    FileOpenAttempted,
    FileLoadSucceeded,
    FileLoadFailed,
    FileSaveSucceeded
};

struct HistoryEventId {
    std::string_view value;
};

struct HistoryTimestamp {
    std::string_view iso8601Utc;
};

struct RelatedAiIds {
    std::string_view promptPackageId;
    std::string_view aiResultId;
};

struct HistorySnapshotMetadata {
    std::string snapshotId;
    std::string summary;
    std::string timestamp;
    std::string relatedEventId;
};

struct HistoryBranchPlaceholder {
    std::string branchId;
    std::string parentBranchId;
    std::string creationReason;
};

struct HistoryItemFields {
    std::string_view stableId;
    std::string_view timestamp;
    std::string_view actor;
    std::string_view scope;
    std::string_view affectedFiles;
    std::string_view operationType;
    std::string_view summary;
    std::string_view beforeReference;
    std::string_view afterReference;
    std::string_view promptPackageId;
    std::string_view aiResultId;
};

struct HistoryStorageConvention {
    std::string_view eventLogPath;
    std::string_view snapshotDirectory;
    std::string_view snapshotFilePattern;
    bool appendOnlyEvents;
    bool commitWithProjectFiles;
};

struct HistoryEvent {
    HistoryEventId id;
    HistoryTimestamp timestamp;
    HistoryActor actor;
    HistoryScope scope;
    HistoryOperation operation;
    std::string_view summary;
    std::string_view affectedFilesJsonArray;
    std::string_view beforeReference;
    std::string_view afterReference;
    RelatedAiIds relatedAiIds;
};

struct StoredHistoryEvent {
    std::string id;
    std::string timestamp;
    HistoryActor actor{HistoryActor::User};
    HistoryScope scope{HistoryScope::Work};
    HistoryOperation operation{HistoryOperation::UserTextEdit};
    std::string summary;
    std::vector<std::string> affectedFiles;
    std::string beforeReference;
    std::string afterReference;
    std::string promptPackageId;
    std::string aiResultId;
    std::optional<HistorySnapshotMetadata> snapshot;
    std::optional<HistoryBranchPlaceholder> branch;
};

struct HistoryLogIssue {
    std::size_t lineNumber{};
    std::string message;
};

struct HistoryLogStatus {
    bool ok{};
    std::vector<HistoryLogIssue> issues;
};

struct HistoryLogReadResult {
    HistoryLogStatus status;
    std::vector<StoredHistoryEvent> events;
};

std::string_view PersistentEventLogName();
std::string_view ToDisplayName(HistoryActor actor);
std::string_view ToDisplayName(HistoryScope scope);
std::string_view ToDisplayName(HistoryOperation operation);

HistoryStorageConvention DefaultStorageConvention();
HistoryItemFields PlannedHistoryItemFields();
HistoryEvent SampleUserTextEditEvent();
StoredHistoryEvent SampleSnapshotMetadataEvent();
StoredHistoryEvent SampleBranchPlaceholderEvent();
std::string_view SampleUserTextEditJsonLine();

std::string SerializeHistoryEventJsonLine(const HistoryEvent& event);
std::string SerializeHistoryEventJsonLine(const StoredHistoryEvent& event);
HistoryLogStatus AppendHistoryEventJsonLine(const std::filesystem::path& path, const HistoryEvent& event);
HistoryLogStatus AppendHistoryEventJsonLine(const std::filesystem::path& path, const StoredHistoryEvent& event);
HistoryLogReadResult ReadHistoryEventJsonLines(const std::filesystem::path& path);

std::filesystem::path DefaultOperationHistoryPath(const std::filesystem::path& scopeFilePath);
StoredHistoryEvent CreateFileOperationHistoryEvent(
    HistoryScope scope,
    HistoryOperation operation,
    const std::filesystem::path& affectedFile,
    std::string_view summary,
    std::string_view detail);
HistoryLogStatus RecordFileOperationHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryScope scope,
    HistoryOperation operation,
    std::string_view summary,
    std::string_view detail);

std::string_view CreateHistoryItemOperationName();
std::string_view ListHistoryItemsOperationName();
std::string_view RestoreSnapshotPlaceholderName();
std::string_view BranchMetadataPlaceholderName();

constexpr std::array<HistoryOperation, 13> ExampleHistoryOperations{{
    HistoryOperation::UserTextEdit,
    HistoryOperation::AcceptedAiRepairOption,
    HistoryOperation::RejectedAiSuggestion,
    HistoryOperation::ImportedSourceFragment,
    HistoryOperation::RuleWeightChanged,
    HistoryOperation::PromptPackageGenerated,
    HistoryOperation::AiJsonResultImported,
    HistoryOperation::SnapshotCreated,
    HistoryOperation::BranchCreated,
    HistoryOperation::FileOpenAttempted,
    HistoryOperation::FileLoadSucceeded,
    HistoryOperation::FileLoadFailed,
    HistoryOperation::FileSaveSucceeded,
}};

}
