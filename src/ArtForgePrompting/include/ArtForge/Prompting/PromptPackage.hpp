#pragma once

#include "ArtForge/Files/ProjectGraph.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace ArtForge::Prompting {

enum class PromptLayer {
    GeneralCreativeRules,
    DomainRules,
    ArtistOrSubjectRules,
    ProjectOrSeriesRules,
    WorkItemState,
    UserMarkingsAndRepairRequests,
    OutputContract
};

enum class PromptInputFormat {
    Markdown,
    Json
};

enum class AiOutputSection {
    Suggestions,
    CritiqueItems,
    ReplacementCandidates,
    RuleHits,
    RuleViolations,
    UnresolvedQuestions,
    Confidence,
    Rationale
};

enum class ImportValidationStep {
    ParseJson,
    ValidateSchema,
    VerifyReferencedIdsExist,
    RejectUnknownDestructiveOperations,
    CreateHistoryItem,
    AskUserToAcceptOrReject
};

enum class PendingSuggestionStatus {
    Pending,
    Accepted,
    Rejected
};

struct PendingSuggestionTarget {
    std::filesystem::path workPath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{-1};
    std::string field;
};

struct PendingSuggestion {
    std::string suggestionId;
    std::string promptPackageId;
    std::filesystem::path promptPackagePath;
    PendingSuggestionTarget target;
    std::string proposedText;
    std::string rationale;
    std::vector<std::string> diagnostics;
    PendingSuggestionStatus status{PendingSuggestionStatus::Pending};
};

struct AiResultValidationResult {
    bool ok{};
    std::vector<std::string> diagnostics;
    std::vector<PendingSuggestion> pendingSuggestions;
};

struct PromptLayerDescriptor {
    PromptLayer layer;
    std::string_view displayName;
    std::string_view packagePath;
    PromptInputFormat format;
};

struct CreativeSubjectProfileFields {
    std::string_view stableId;
    std::string_view role;
    std::string_view domainMeaning;
    std::string_view constraints;
    std::string_view viewpoint;
};

struct PromptLayerEntry {
    PromptLayer layer;
    std::string role;
    std::filesystem::path sourcePath;
    PromptInputFormat format;
    bool loaded{};
    std::string content;
    std::string loadError;
};

struct PromptPackageBuildRequest {
    std::filesystem::path solutionPath;
    std::string workId;
    std::vector<std::filesystem::path> generalRuleFiles;
    std::vector<std::filesystem::path> domainRuleFiles;
    std::vector<std::filesystem::path> projectRuleFiles;
    std::filesystem::path taskInstructionPath;
    std::filesystem::path outputSchemaPath;
    std::string taskInstruction;
};

struct SelectedDomainItemPromptRequest {
    std::filesystem::path workPath;
    std::string workId;
    std::string domain;
    std::string selectedItemType;
    std::string selectedItemId;
    int selectedItemIndex{};
    std::string requestedOperation;
};

struct PromptPackageBuildResult {
    bool ok{};
    std::vector<PromptLayerEntry> layers;
    std::vector<std::string> issues;
};

std::string_view PromptPackageContractName();
std::string_view ToDisplayName(PromptLayer layer);
std::string_view ToDisplayName(PromptInputFormat format);
std::string_view ToDisplayName(AiOutputSection section);
std::string_view ToDisplayName(ImportValidationStep step);
std::string_view ToDisplayName(PendingSuggestionStatus status);

CreativeSubjectProfileFields PlannedCreativeSubjectProfileFields();
std::string_view PromptPackageHistoryGenerationOperation();
std::string_view AiResultHistoryImportOperation();
std::string_view AiResultReviewRequirement();

PromptPackageBuildResult BuildPromptPackageFromWorkContext(const PromptPackageBuildRequest& request);
PromptPackageBuildResult BuildPromptPackageFromSelectedDomainItem(const SelectedDomainItemPromptRequest& request);
std::string SerializePromptPackageDebugDump(const PromptPackageBuildResult& result);
AiResultValidationResult ValidateAiResultJsonText(std::string_view jsonText);
std::string DescribeAiResultValidation(const AiResultValidationResult& result);
std::string SerializePendingSuggestionJsonLine(const PendingSuggestion& suggestion);

constexpr std::array<PromptLayerDescriptor, 7> PromptContextOrder{{
    {PromptLayer::GeneralCreativeRules, "general creative rules", "general_rules.md", PromptInputFormat::Markdown},
    {PromptLayer::DomainRules, "domain rules", "domain_rules.md", PromptInputFormat::Markdown},
    {PromptLayer::ArtistOrSubjectRules, "artist or subject rules", "artist_rules.md", PromptInputFormat::Markdown},
    {PromptLayer::ProjectOrSeriesRules, "project or series rules", "project_rules.md", PromptInputFormat::Markdown},
    {PromptLayer::WorkItemState, "work item state", "current_state.json", PromptInputFormat::Json},
    {PromptLayer::UserMarkingsAndRepairRequests, "user markings and repair requests", "task_instruction.md", PromptInputFormat::Markdown},
    {PromptLayer::OutputContract, "output contract", "output_schema.json", PromptInputFormat::Json},
}};

constexpr std::array<AiOutputSection, 8> RequiredAiOutputSections{{
    AiOutputSection::Suggestions,
    AiOutputSection::CritiqueItems,
    AiOutputSection::ReplacementCandidates,
    AiOutputSection::RuleHits,
    AiOutputSection::RuleViolations,
    AiOutputSection::UnresolvedQuestions,
    AiOutputSection::Confidence,
    AiOutputSection::Rationale,
}};

constexpr std::array<ImportValidationStep, 6> AiResultImportValidationOrder{{
    ImportValidationStep::ParseJson,
    ImportValidationStep::ValidateSchema,
    ImportValidationStep::VerifyReferencedIdsExist,
    ImportValidationStep::RejectUnknownDestructiveOperations,
    ImportValidationStep::CreateHistoryItem,
    ImportValidationStep::AskUserToAcceptOrReject,
}};

}
