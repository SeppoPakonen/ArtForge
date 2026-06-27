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
    SuggestionImported,
    SuggestionReviewOpened,
    SuggestionAccepted,
    SuggestionRejected,
    SuggestionApplyRequested,
    SuggestionApplySucceeded,
    SuggestionApplyFailed,
    SnapshotCreated,
    BranchCreated,
    FileOpenAttempted,
    FileLoadSucceeded,
    FileLoadFailed,
    FileSaveSucceeded,
    DomainItemSelected,
    PromptRequested,
    CritiqueRequested,
    RepairAlternativesRequested,
    ItemFlaggedForReview,
    PresentationSelectionChanged,
    PromptPreviewRequested,
    SelectedItemPromptPackageBuilt,
    EditCommandRequested,
    ChangeSetValidated,
    ChangeSetApplied,
    SaveRequested,
    SaveSucceeded,
    SaveFailed
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
    std::string reason;
    std::string relatedSuggestionId;
    std::string workId;
    std::string workPath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{};
};

struct HistoryBranchPlaceholder {
    std::string branchId;
    std::string parentBranchId;
    std::string creationReason;
};

struct DomainItemHistoryMetadata {
    std::string workId;
    std::string workPath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{};
    std::string operationSummary;
};

struct PresentationCommandHistoryMetadata {
    std::string workId;
    std::string workPath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{};
    std::string commandName;
    std::string resultStatus;
};

struct ChangeSetHistoryMetadata {
    std::string workId;
    std::string workPath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{};
    std::string commandId;
    int changeCount{};
    std::string operationSummary;
};

struct SuggestionReviewHistoryMetadata {
    std::string suggestionId;
    std::string sourceRequestPath;
    std::string sourceResultPath;
    std::string workId;
    std::string workPath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{};
    std::string status;
    std::string reasonOrDiagnostic;
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
    std::optional<DomainItemHistoryMetadata> domainItem;
    std::optional<PresentationCommandHistoryMetadata> presentationCommand;
    std::optional<ChangeSetHistoryMetadata> changeSet;
    std::optional<SuggestionReviewHistoryMetadata> suggestionReview;
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

StoredHistoryEvent CreateDomainItemHistoryEvent(
    HistoryOperation operation,
    const DomainItemHistoryMetadata& metadata);
HistoryLogStatus RecordDomainItemHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryOperation operation,
    const DomainItemHistoryMetadata& metadata);

StoredHistoryEvent CreatePresentationCommandHistoryEvent(
    HistoryOperation operation,
    const PresentationCommandHistoryMetadata& metadata);
HistoryLogStatus RecordPresentationCommandHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryOperation operation,
    const PresentationCommandHistoryMetadata& metadata);

StoredHistoryEvent CreateChangeSetHistoryEvent(
    HistoryOperation operation,
    const ChangeSetHistoryMetadata& metadata);
HistoryLogStatus RecordChangeSetHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryOperation operation,
    const ChangeSetHistoryMetadata& metadata);

StoredHistoryEvent CreateSuggestionReviewHistoryEvent(
    HistoryOperation operation,
    const SuggestionReviewHistoryMetadata& metadata);
HistoryLogStatus RecordSuggestionReviewHistoryEvent(
    const std::filesystem::path& scopeFilePath,
    HistoryOperation operation,
    const SuggestionReviewHistoryMetadata& metadata);
std::string SampleSuggestionReviewHistoryJsonLines();
StoredHistoryEvent CreateSuggestionApplySnapshotEvent(const SuggestionReviewHistoryMetadata& metadata);
HistoryLogStatus RecordSuggestionApplySnapshotEvent(
    const std::filesystem::path& scopeFilePath,
    const SuggestionReviewHistoryMetadata& metadata);

std::string_view CreateHistoryItemOperationName();
std::string_view ListHistoryItemsOperationName();
std::string_view RestoreSnapshotPlaceholderName();
std::string_view BranchMetadataPlaceholderName();

constexpr std::array<HistoryOperation, 34> ExampleHistoryOperations{{
    HistoryOperation::UserTextEdit,
    HistoryOperation::AcceptedAiRepairOption,
    HistoryOperation::RejectedAiSuggestion,
    HistoryOperation::ImportedSourceFragment,
    HistoryOperation::RuleWeightChanged,
    HistoryOperation::PromptPackageGenerated,
    HistoryOperation::AiJsonResultImported,
    HistoryOperation::SuggestionImported,
    HistoryOperation::SuggestionReviewOpened,
    HistoryOperation::SuggestionAccepted,
    HistoryOperation::SuggestionRejected,
    HistoryOperation::SuggestionApplyRequested,
    HistoryOperation::SuggestionApplySucceeded,
    HistoryOperation::SuggestionApplyFailed,
    HistoryOperation::SnapshotCreated,
    HistoryOperation::BranchCreated,
    HistoryOperation::FileOpenAttempted,
    HistoryOperation::FileLoadSucceeded,
    HistoryOperation::FileLoadFailed,
    HistoryOperation::FileSaveSucceeded,
    HistoryOperation::DomainItemSelected,
    HistoryOperation::PromptRequested,
    HistoryOperation::CritiqueRequested,
    HistoryOperation::RepairAlternativesRequested,
    HistoryOperation::ItemFlaggedForReview,
    HistoryOperation::PresentationSelectionChanged,
    HistoryOperation::PromptPreviewRequested,
    HistoryOperation::SelectedItemPromptPackageBuilt,
    HistoryOperation::EditCommandRequested,
    HistoryOperation::ChangeSetValidated,
    HistoryOperation::ChangeSetApplied,
    HistoryOperation::SaveRequested,
    HistoryOperation::SaveSucceeded,
    HistoryOperation::SaveFailed,
}};

}
