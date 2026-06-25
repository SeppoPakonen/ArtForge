#include "ArtForge/History/EventLog.hpp"

namespace ArtForge::History {

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
