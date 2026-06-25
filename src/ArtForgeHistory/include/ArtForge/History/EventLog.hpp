#pragma once

#include <array>
#include <string_view>

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
    BranchCreated
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

std::string_view PersistentEventLogName();
std::string_view ToDisplayName(HistoryActor actor);
std::string_view ToDisplayName(HistoryScope scope);
std::string_view ToDisplayName(HistoryOperation operation);

HistoryStorageConvention DefaultStorageConvention();
HistoryItemFields PlannedHistoryItemFields();
HistoryEvent SampleUserTextEditEvent();
std::string_view SampleUserTextEditJsonLine();

std::string_view CreateHistoryItemOperationName();
std::string_view ListHistoryItemsOperationName();
std::string_view RestoreSnapshotPlaceholderName();
std::string_view BranchMetadataPlaceholderName();

constexpr std::array<HistoryOperation, 9> ExampleHistoryOperations{{
    HistoryOperation::UserTextEdit,
    HistoryOperation::AcceptedAiRepairOption,
    HistoryOperation::RejectedAiSuggestion,
    HistoryOperation::ImportedSourceFragment,
    HistoryOperation::RuleWeightChanged,
    HistoryOperation::PromptPackageGenerated,
    HistoryOperation::AiJsonResultImported,
    HistoryOperation::SnapshotCreated,
    HistoryOperation::BranchCreated,
}};

}
