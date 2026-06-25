#include "ArtForge/Prompting/PromptPackage.hpp"

namespace ArtForge::Prompting {

std::string_view PromptPackageContractName()
{
    return "ArtForge prompt package JSON contract";
}

std::string_view ToDisplayName(PromptLayer layer)
{
    switch (layer) {
    case PromptLayer::GeneralCreativeRules:
        return "general creative rules";
    case PromptLayer::DomainRules:
        return "domain rules";
    case PromptLayer::ArtistOrSubjectRules:
        return "artist or subject rules";
    case PromptLayer::ProjectOrSeriesRules:
        return "project or series rules";
    case PromptLayer::WorkItemState:
        return "work item state";
    case PromptLayer::UserMarkingsAndRepairRequests:
        return "user markings and repair requests";
    case PromptLayer::OutputContract:
        return "output contract";
    }

    return "unknown prompt layer";
}

std::string_view ToDisplayName(PromptInputFormat format)
{
    switch (format) {
    case PromptInputFormat::Markdown:
        return "Markdown";
    case PromptInputFormat::Json:
        return "JSON";
    }

    return "unknown format";
}

std::string_view ToDisplayName(AiOutputSection section)
{
    switch (section) {
    case AiOutputSection::Suggestions:
        return "suggestions";
    case AiOutputSection::CritiqueItems:
        return "critique items";
    case AiOutputSection::ReplacementCandidates:
        return "replacement candidates";
    case AiOutputSection::RuleHits:
        return "rule hits";
    case AiOutputSection::RuleViolations:
        return "rule violations";
    case AiOutputSection::UnresolvedQuestions:
        return "unresolved questions";
    case AiOutputSection::Confidence:
        return "confidence";
    case AiOutputSection::Rationale:
        return "rationale";
    }

    return "unknown AI output section";
}

std::string_view ToDisplayName(ImportValidationStep step)
{
    switch (step) {
    case ImportValidationStep::ParseJson:
        return "parse JSON";
    case ImportValidationStep::ValidateSchema:
        return "validate schema";
    case ImportValidationStep::VerifyReferencedIdsExist:
        return "verify referenced ids exist";
    case ImportValidationStep::RejectUnknownDestructiveOperations:
        return "reject unknown destructive operations";
    case ImportValidationStep::CreateHistoryItem:
        return "create history item";
    case ImportValidationStep::AskUserToAcceptOrReject:
        return "ask user to accept or reject";
    }

    return "unknown validation step";
}

CreativeSubjectProfileFields PlannedCreativeSubjectProfileFields()
{
    return {
        "id",
        "role",
        "domain_meaning",
        "constraints",
        "viewpoint",
    };
}

std::string_view PromptPackageHistoryGenerationOperation()
{
    return "prompt package generated";
}

std::string_view AiResultHistoryImportOperation()
{
    return "AI JSON result imported";
}

std::string_view AiResultReviewRequirement()
{
    return "AI JSON results must be accepted or rejected by the user before state changes";
}

}
