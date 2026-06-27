#pragma once

#include "ArtForge/Files/ProjectGraph.hpp"

#include <array>
#include <filesystem>
#include <utility>
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

enum class AiProviderKind {
    Unknown,
    ManualQueue,
    OpenAI,
    Anthropic,
    AlibabaCloud,
    Unsupported
};

enum class AiExecutionStatus {
    Draft,
    Queued,
    WaitingForResult,
    ResultFound,
    ResultInvalid,
    ImportedPendingSuggestion,
    TargetMismatch,
    NotConfigured,
    NotImplemented,
    UnsupportedProvider,
    Failed
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
    std::string rejectionReason;
    std::vector<std::string> diagnostics;
    PendingSuggestionStatus status{PendingSuggestionStatus::Pending};
};

struct AiResultValidationResult {
    bool ok{};
    std::vector<std::string> diagnostics;
    std::vector<PendingSuggestion> pendingSuggestions;
};

struct AiResultLocation {
    std::filesystem::path requestPath;
    std::filesystem::path promptTextPath;
    std::filesystem::path expectedResultPath;
    std::filesystem::path statusPath;
    std::filesystem::path importedSuggestionsPath;
};

struct AiExecutionError {
    std::string code;
    std::string message;
};

struct AiExecutionRequest {
    std::string requestId;
    AiProviderKind providerKind{AiProviderKind::Unknown};
    std::filesystem::path queueRoot;
    std::filesystem::path promptPackagePath;
    std::string promptPackageSummary;
    std::filesystem::path resultSchemaPath;
    std::string requestedOperation;
    PendingSuggestionTarget target;
    AiResultLocation locations;
};

struct AiExecutionResult {
    std::string requestId;
    AiProviderKind providerKind{AiProviderKind::Unknown};
    AiExecutionStatus status{AiExecutionStatus::Draft};
    AiResultLocation locations;
    std::vector<AiExecutionError> errors;
    std::vector<std::string> diagnostics;
    std::vector<PendingSuggestion> pendingSuggestions;
};

struct AiProviderConfiguration {
    AiProviderKind providerKind{AiProviderKind::Unknown};
    std::string displayName;
    std::string endpointProfile;
    std::string modelName;
    bool enabled{};
};

struct ManualAiQueueWriteRequest {
    AiExecutionRequest execution;
    std::string promptText;
};

struct ManualAiQueueWriteResult {
    bool ok{};
    AiExecutionRequest execution;
    AiExecutionResult providerResult;
    std::vector<std::string> diagnostics;
};

struct ManualAiQueuePollRequest {
    AiExecutionRequest execution;
    bool importPendingSuggestions{};
};

struct ManualAiQueuePollResult {
    bool ok{};
    AiExecutionResult providerResult;
    std::vector<std::string> diagnostics;
};

struct AiProviderDispatchRequest {
    AiExecutionRequest execution;
    std::string promptText;
    std::vector<AiProviderConfiguration> providerConfigurations;
};

struct HttpHeader {
    std::string name;
    std::string value;
};

struct HttpJsonPostRequest {
    std::string url;
    std::vector<HttpHeader> headers;
    std::string jsonBody;
    int timeoutMilliseconds{30000};
};

struct HttpJsonPostResponse {
    bool ok{};
    int statusCode{};
    std::string body;
    std::string errorCode;
    std::string errorMessage;
};

struct OpenAiRequestMappingRequest {
    AiExecutionRequest execution;
    AiProviderConfiguration providerConfiguration;
    std::string promptText;
    std::string resultSchemaJson;
};

struct OpenAiResponseMappingResult {
    bool ok{};
    std::string requestId;
    std::string candidateJson;
    AiExecutionResult providerResult;
    std::vector<std::string> diagnostics;
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
std::string_view ToDisplayName(AiProviderKind provider);
std::string_view ToDisplayName(AiExecutionStatus status);

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
std::string DescribeAiExecutionModel();
std::vector<AiProviderConfiguration> DefaultAiProviderConfigurationStubs();
std::string DescribeAiProviderConfigurationStubs(const std::vector<AiProviderConfiguration>& configurations);
ManualAiQueueWriteResult WriteManualAiQueueRequest(const ManualAiQueueWriteRequest& request);
std::string DescribeManualAiQueueWriteResult(const ManualAiQueueWriteResult& result);
ManualAiQueuePollResult PollManualAiQueueResult(const ManualAiQueuePollRequest& request);
std::string DescribeManualAiQueuePollResult(const ManualAiQueuePollResult& result);
AiExecutionResult DispatchAiExecutionRequestNoNetwork(const AiProviderDispatchRequest& request);
std::string DescribeAiExecutionResult(const AiExecutionResult& result);
HttpJsonPostResponse PostJsonWithWinHttp(const HttpJsonPostRequest& request);
HttpJsonPostResponse FakeHttpJsonPostResponse(int statusCode, std::string body);
std::string DescribeHttpJsonPostResponse(const HttpJsonPostResponse& response);
std::string DescribeHttpJsonPostSmokeExamples();
std::string BuildOpenAiResponsesRequestJson(const OpenAiRequestMappingRequest& request);
OpenAiResponseMappingResult ExtractOpenAiResponseResultJson(
    std::string_view responseJson,
    const AiExecutionRequest& execution);
std::string DescribeOpenAiMappingSmokeExamples(std::string_view responseJson);

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
