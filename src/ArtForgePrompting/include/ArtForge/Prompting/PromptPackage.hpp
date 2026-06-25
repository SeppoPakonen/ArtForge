#pragma once

#include <array>
#include <string_view>

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

std::string_view PromptPackageContractName();
std::string_view ToDisplayName(PromptLayer layer);
std::string_view ToDisplayName(PromptInputFormat format);
std::string_view ToDisplayName(AiOutputSection section);
std::string_view ToDisplayName(ImportValidationStep step);

CreativeSubjectProfileFields PlannedCreativeSubjectProfileFields();
std::string_view PromptPackageHistoryGenerationOperation();
std::string_view AiResultHistoryImportOperation();
std::string_view AiResultReviewRequirement();

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
