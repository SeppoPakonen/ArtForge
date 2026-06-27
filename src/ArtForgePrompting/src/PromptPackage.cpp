#include "ArtForge/Prompting/PromptPackage.hpp"

#include "ArtForge/Files/DomainWorkViewModels.hpp"

#include <fstream>
#include <optional>
#include <sstream>

namespace ArtForge::Prompting {

namespace {

struct WorkContext {
    const ArtForge::Files::ArtistGraphNode* artist{};
    const ArtForge::Files::SeriesGraphNode* series{};
    const ArtForge::Files::WorkGraphNode* work{};
};

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

std::optional<WorkContext> FindWorkContext(
    const ArtForge::Files::SolutionProjectGraph& graph,
    std::string_view workId)
{
    for (const auto& artist : graph.artists) {
        for (const auto& project : artist.projects) {
            for (const auto& work : project.works) {
                if (work.file.id == workId) {
                    return WorkContext{&artist, &project, &work};
                }
            }
        }
    }

    for (const auto& project : graph.projects) {
        for (const auto& work : project.works) {
            if (work.file.id == workId) {
                return WorkContext{nullptr, &project, &work};
            }
        }
    }

    return std::nullopt;
}

PromptLayerEntry LoadLayerFile(
    PromptLayer layer,
    std::string role,
    const std::filesystem::path& sourcePath,
    PromptInputFormat format)
{
    PromptLayerEntry entry{layer, std::move(role), sourcePath, format, false, {}, {}};

    std::ifstream input{sourcePath};
    if (!input) {
        entry.loadError = "could not open prompt layer file";
        return entry;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    if (!input.good() && !input.eof()) {
        entry.loadError = "could not read prompt layer file";
        return entry;
    }

    entry.content = buffer.str();
    entry.loaded = true;
    return entry;
}

PromptLayerEntry GeneratedLayer(
    PromptLayer layer,
    std::string role,
    PromptInputFormat format,
    std::string content)
{
    return {layer, std::move(role), {}, format, true, std::move(content), {}};
}

void AddFiles(
    PromptPackageBuildResult& result,
    PromptLayer layer,
    std::string_view role,
    PromptInputFormat format,
    const std::vector<std::filesystem::path>& paths)
{
    for (const auto& path : paths) {
        auto entry = LoadLayerFile(layer, std::string{role}, path, format);
        if (!entry.loaded) {
            result.issues.push_back(entry.sourcePath.string() + ": " + entry.loadError);
        }
        result.layers.push_back(std::move(entry));
    }
}

std::string BuildCurrentStateJson(
    const ArtForge::Files::SolutionProjectGraph& graph,
    const WorkContext& context)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"format\": \"artforge.prompt.currentState\",\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"solution\": {\n";
    output << "    \"id\": " << Quote(graph.file.id) << ",\n";
    output << "    \"displayName\": " << Quote(graph.file.displayName) << ",\n";
    output << "    \"path\": " << Quote(graph.path.string()) << "\n";
    output << "  },\n";
    output << "  \"artist\": {\n";
    output << "    \"id\": " << Quote(context.artist == nullptr ? "" : context.artist->file.id) << ",\n";
    output << "    \"displayName\": " << Quote(context.artist == nullptr ? "" : context.artist->file.displayName) << "\n";
    output << "  },\n";
    output << "  \"series\": {\n";
    output << "    \"id\": " << Quote(context.series == nullptr ? "" : context.series->file.id) << ",\n";
    output << "    \"displayName\": " << Quote(context.series == nullptr ? "" : context.series->file.displayName) << "\n";
    output << "  },\n";
    output << "  \"scope\": {\n";
    output << "    \"type\": \"work\",\n";
    output << "    \"id\": " << Quote(context.work == nullptr ? "" : context.work->file.id) << ",\n";
    output << "    \"displayName\": " << Quote(context.work == nullptr ? "" : context.work->file.displayName) << ",\n";
    output << "    \"path\": " << Quote(context.work == nullptr ? "" : context.work->path.string()) << "\n";
    output << "  },\n";
    output << "  \"knownReferences\": [\n";
    if (context.work != nullptr) {
        for (std::size_t index = 0; index < context.work->sources.size(); ++index) {
            const auto& source = context.work->sources[index];
            output << "    {\"id\": " << Quote(source.reference.id) << ", \"path\": " << Quote(source.resolvedPath.string()) << "}";
            output << (index + 1 < context.work->sources.size() ? "," : "") << "\n";
        }
    }
    output << "  ]\n";
    output << "}\n";
    return output.str();
}

std::string FenceName(PromptInputFormat format)
{
    return format == PromptInputFormat::Json ? "json" : "markdown";
}

std::string BuildSelectedItemOutputContractJson()
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"format\": \"artforge.ai.selectedItemResult\",\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"requiredTopLevelKeys\": [\n";
    output << "    \"suggestions\",\n";
    output << "    \"critiqueItems\",\n";
    output << "    \"replacementCandidates\",\n";
    output << "    \"ruleHits\",\n";
    output << "    \"ruleViolations\",\n";
    output << "    \"unresolvedQuestions\",\n";
    output << "    \"confidence\",\n";
    output << "    \"rationale\"\n";
    output << "  ],\n";
    output << "  \"importPolicy\": \"AI output must be valid JSON and validated before import.\"\n";
    output << "}\n";
    return output.str();
}

std::string BuildSelectedItemTaskInstruction(const SelectedDomainItemPromptRequest& request)
{
    std::ostringstream output;
    output << "# Selected Domain Item Request\n\n";
    output << "- Requested operation: " << request.requestedOperation << "\n";
    output << "- Domain: " << request.domain << "\n";
    output << "- Selected item type: " << request.selectedItemType << "\n";
    output << "- Selected item id: " << request.selectedItemId << "\n";
    output << "- Selected item index: " << request.selectedItemIndex << "\n\n";
    output << "Return only JSON matching the output contract. Do not modify project files.\n";
    return output.str();
}

std::string BuildSelectedLyricLineJson(const SelectedDomainItemPromptRequest& request)
{
    const auto model = ArtForge::Files::LoadLyricsWorkViewModel(request.workPath);
    std::ostringstream output;
    output << "{\n";
    output << "  \"format\": \"artforge.prompt.selectedDomainItem\",\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"work\": {\n";
    output << "    \"id\": " << Quote(model.metadata.id) << ",\n";
    output << "    \"path\": " << Quote(request.workPath.string()) << ",\n";
    output << "    \"displayName\": " << Quote(model.metadata.displayName) << ",\n";
    output << "    \"domain\": \"lyrics\"\n";
    output << "  },\n";
    output << "  \"selection\": {\n";
    output << "    \"type\": \"lyricLine\",\n";
    const auto index = request.selectedItemIndex >= 0 ? static_cast<std::size_t>(request.selectedItemIndex) : model.lyricLines.size();
    if (index < model.lyricLines.size()) {
        const auto& line = model.lyricLines[index];
        output << "    \"id\": " << Quote(line.id) << ",\n";
        output << "    \"index\": " << request.selectedItemIndex << ",\n";
        output << "    \"sectionId\": " << Quote(line.sectionId) << ",\n";
        output << "    \"timeRange\": " << Quote(line.timeRange) << ",\n";
        output << "    \"text\": " << Quote(line.text) << ",\n";
        output << "    \"evaluation\": " << Quote(line.evaluationSummary) << "\n";
    } else {
        output << "    \"id\": " << Quote(request.selectedItemId) << ",\n";
        output << "    \"index\": " << request.selectedItemIndex << ",\n";
        output << "    \"missing\": true\n";
    }
    output << "  }\n";
    output << "}\n";
    return output.str();
}

std::string BuildSelectedVisualLayerJson(const SelectedDomainItemPromptRequest& request)
{
    const auto model = ArtForge::Files::LoadVisualArtWorkViewModel(request.workPath);
    std::vector<ArtForge::Files::VisualArtLayerView> layers = model.viewerLayers;
    layers.insert(layers.end(), model.paintLayers.begin(), model.paintLayers.end());

    std::ostringstream output;
    output << "{\n";
    output << "  \"format\": \"artforge.prompt.selectedDomainItem\",\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"work\": {\n";
    output << "    \"id\": " << Quote(model.metadata.id) << ",\n";
    output << "    \"path\": " << Quote(request.workPath.string()) << ",\n";
    output << "    \"displayName\": " << Quote(model.metadata.displayName) << ",\n";
    output << "    \"domain\": \"visualArt\"\n";
    output << "  },\n";
    output << "  \"selection\": {\n";
    output << "    \"type\": \"visualLayer\",\n";
    const auto index = request.selectedItemIndex >= 0 ? static_cast<std::size_t>(request.selectedItemIndex) : layers.size();
    if (index < layers.size()) {
        const auto& layer = layers[index];
        output << "    \"id\": " << Quote(layer.id) << ",\n";
        output << "    \"index\": " << request.selectedItemIndex << ",\n";
        output << "    \"layerType\": " << Quote(layer.layerType) << ",\n";
        output << "    \"label\": " << Quote(layer.label) << ",\n";
        output << "    \"intent\": " << Quote(layer.intent) << ",\n";
        output << "    \"priority\": " << Quote(layer.priority) << ",\n";
        output << "    \"status\": " << Quote(layer.status) << "\n";
    } else {
        output << "    \"id\": " << Quote(request.selectedItemId) << ",\n";
        output << "    \"index\": " << request.selectedItemIndex << ",\n";
        output << "    \"missing\": true\n";
    }
    output << "  }\n";
    output << "}\n";
    return output.str();
}

std::string BuildSelectedScriptBlockJson(const SelectedDomainItemPromptRequest& request)
{
    const auto model = ArtForge::Files::LoadScriptStoryboardWorkViewModel(request.workPath);
    std::ostringstream output;
    output << "{\n";
    output << "  \"format\": \"artforge.prompt.selectedDomainItem\",\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"work\": {\n";
    output << "    \"id\": " << Quote(model.metadata.id) << ",\n";
    output << "    \"path\": " << Quote(request.workPath.string()) << ",\n";
    output << "    \"displayName\": " << Quote(model.metadata.displayName) << ",\n";
    output << "    \"domain\": \"scriptStoryboard\"\n";
    output << "  },\n";
    output << "  \"selection\": {\n";
    output << "    \"type\": \"scriptBlock\",\n";
    const auto index = request.selectedItemIndex >= 0 ? static_cast<std::size_t>(request.selectedItemIndex) : model.blocks.size();
    if (index < model.blocks.size()) {
        const auto& block = model.blocks[index];
        output << "    \"id\": " << Quote(block.id) << ",\n";
        output << "    \"index\": " << request.selectedItemIndex << ",\n";
        output << "    \"sceneId\": " << Quote(block.sceneId) << ",\n";
        output << "    \"kind\": " << Quote(block.kind) << ",\n";
        output << "    \"timeRange\": " << Quote(block.timeRange) << ",\n";
        output << "    \"speaker\": " << Quote(block.speaker) << ",\n";
        output << "    \"voice\": " << Quote(block.voice) << ",\n";
        output << "    \"text\": " << Quote(block.text) << "\n";
    } else {
        output << "    \"id\": " << Quote(request.selectedItemId) << ",\n";
        output << "    \"index\": " << request.selectedItemIndex << ",\n";
        output << "    \"missing\": true\n";
    }
    output << "  }\n";
    output << "}\n";
    return output.str();
}

std::string BuildSelectedItemJson(const SelectedDomainItemPromptRequest& request)
{
    if (request.domain == "lyrics") {
        return BuildSelectedLyricLineJson(request);
    }
    if (request.domain == "visualArt") {
        return BuildSelectedVisualLayerJson(request);
    }
    if (request.domain == "scriptStoryboard") {
        return BuildSelectedScriptBlockJson(request);
    }

    std::ostringstream output;
    output << "{\n";
    output << "  \"format\": \"artforge.prompt.selectedDomainItem\",\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"work\": {\n";
    output << "    \"id\": " << Quote(request.workId) << ",\n";
    output << "    \"path\": " << Quote(request.workPath.string()) << ",\n";
    output << "    \"domain\": " << Quote(request.domain) << "\n";
    output << "  },\n";
    output << "  \"selection\": {\n";
    output << "    \"type\": " << Quote(request.selectedItemType) << ",\n";
    output << "    \"id\": " << Quote(request.selectedItemId) << ",\n";
    output << "    \"index\": " << request.selectedItemIndex << ",\n";
    output << "    \"unsupportedDomain\": true\n";
    output << "  }\n";
    output << "}\n";
    return output.str();
}

bool ContainsField(std::string_view json, std::string_view key)
{
    const std::string field = "\"" + std::string{key} + "\"";
    return json.find(field) != std::string_view::npos;
}

std::string ExtractStringField(std::string_view json, std::string_view key)
{
    const std::string field = "\"" + std::string{key} + "\"";
    const auto fieldPosition = json.find(field);
    if (fieldPosition == std::string_view::npos) {
        return {};
    }
    const auto colonPosition = json.find(':', fieldPosition + field.size());
    if (colonPosition == std::string_view::npos) {
        return {};
    }
    auto quotePosition = json.find('"', colonPosition + 1);
    if (quotePosition == std::string_view::npos) {
        return {};
    }
    std::string value;
    bool escaped = false;
    for (auto index = quotePosition + 1; index < json.size(); ++index) {
        const char character = json[index];
        if (escaped) {
            value += character;
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '"') {
            return value;
        }
        value += character;
    }
    return {};
}

}

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

std::string_view ToDisplayName(PendingSuggestionStatus status)
{
    switch (status) {
    case PendingSuggestionStatus::Pending:
        return "pending";
    case PendingSuggestionStatus::Accepted:
        return "accepted";
    case PendingSuggestionStatus::Rejected:
        return "rejected";
    }

    return "unknown";
}

std::string_view ToDisplayName(AiProviderKind provider)
{
    switch (provider) {
    case AiProviderKind::Unknown:
        return "unknown";
    case AiProviderKind::ManualQueue:
        return "manualQueue";
    case AiProviderKind::OpenAI:
        return "openai";
    case AiProviderKind::Anthropic:
        return "anthropic";
    case AiProviderKind::AlibabaCloud:
        return "alibabaCloud";
    case AiProviderKind::Unsupported:
        return "unsupported";
    }

    return "unknown";
}

std::string_view ToDisplayName(AiExecutionStatus status)
{
    switch (status) {
    case AiExecutionStatus::Draft:
        return "draft";
    case AiExecutionStatus::Queued:
        return "queued";
    case AiExecutionStatus::WaitingForResult:
        return "waitingForResult";
    case AiExecutionStatus::ResultFound:
        return "resultFound";
    case AiExecutionStatus::ResultInvalid:
        return "resultInvalid";
    case AiExecutionStatus::ImportedPendingSuggestion:
        return "importedPendingSuggestion";
    case AiExecutionStatus::TargetMismatch:
        return "targetMismatch";
    case AiExecutionStatus::NotConfigured:
        return "notConfigured";
    case AiExecutionStatus::NotImplemented:
        return "notImplemented";
    case AiExecutionStatus::UnsupportedProvider:
        return "unsupportedProvider";
    case AiExecutionStatus::Failed:
        return "failed";
    }

    return "unknown";
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

PromptPackageBuildResult BuildPromptPackageFromWorkContext(const PromptPackageBuildRequest& request)
{
    PromptPackageBuildResult result;
    const auto graph = ArtForge::Files::LoadSolutionProjectGraph(request.solutionPath);
    for (const auto& issue : ArtForge::Files::FlattenGraphIssues(graph)) {
        result.issues.push_back(issue.path.string() + ": " + issue.message);
    }

    const auto context = FindWorkContext(graph, request.workId);
    if (!context) {
        result.issues.push_back("could not find work id in loaded graph: " + request.workId);
        result.ok = false;
        return result;
    }

    AddFiles(
        result,
        PromptLayer::GeneralCreativeRules,
        "general creative rules",
        PromptInputFormat::Markdown,
        request.generalRuleFiles);
    AddFiles(
        result,
        PromptLayer::DomainRules,
        "domain rules",
        PromptInputFormat::Markdown,
        request.domainRuleFiles);

    if (context->artist != nullptr) {
        std::vector<std::filesystem::path> artistRuleFiles;
        for (const auto& ruleFile : context->artist->file.ruleFiles) {
            artistRuleFiles.push_back(ArtForge::Files::ResolveScopeReference(context->artist->path, ruleFile));
        }
        AddFiles(
            result,
            PromptLayer::ArtistOrSubjectRules,
            "artist or subject rules",
            PromptInputFormat::Markdown,
            artistRuleFiles);
    } else {
        result.issues.push_back("work context has no loaded artist parent");
    }

    AddFiles(
        result,
        PromptLayer::ProjectOrSeriesRules,
        "project or series rules",
        PromptInputFormat::Markdown,
        request.projectRuleFiles);

    result.layers.push_back(GeneratedLayer(
        PromptLayer::WorkItemState,
        "current technical state",
        PromptInputFormat::Json,
        BuildCurrentStateJson(graph, *context)));
    if (!request.taskInstructionPath.empty()) {
        auto taskInstruction = LoadLayerFile(
            PromptLayer::UserMarkingsAndRepairRequests,
            "task instruction",
            request.taskInstructionPath,
            PromptInputFormat::Markdown);
        if (!taskInstruction.loaded) {
            result.issues.push_back(taskInstruction.sourcePath.string() + ": " + taskInstruction.loadError);
        }
        result.layers.push_back(std::move(taskInstruction));
    } else {
        result.layers.push_back(GeneratedLayer(
            PromptLayer::UserMarkingsAndRepairRequests,
            "task instruction",
            PromptInputFormat::Markdown,
            request.taskInstruction));
    }

    if (!request.outputSchemaPath.empty()) {
        auto outputSchema = LoadLayerFile(
            PromptLayer::OutputContract,
            "output contract",
            request.outputSchemaPath,
            PromptInputFormat::Json);
        if (!outputSchema.loaded) {
            result.issues.push_back(outputSchema.sourcePath.string() + ": " + outputSchema.loadError);
        }
        result.layers.push_back(std::move(outputSchema));
    } else {
        result.issues.push_back("output schema path is required");
    }

    result.ok = result.issues.empty();
    return result;
}

PromptPackageBuildResult BuildPromptPackageFromSelectedDomainItem(const SelectedDomainItemPromptRequest& request)
{
    PromptPackageBuildResult result;
    if (request.workPath.empty()) {
        result.issues.push_back("work path is required");
    }
    if (request.domain.empty()) {
        result.issues.push_back("domain is required");
    }
    if (request.selectedItemType.empty()) {
        result.issues.push_back("selected item type is required");
    }
    if (request.requestedOperation.empty()) {
        result.issues.push_back("requested operation is required");
    }

    result.layers.push_back(GeneratedLayer(
        PromptLayer::WorkItemState,
        "selected domain item",
        PromptInputFormat::Json,
        BuildSelectedItemJson(request)));
    result.layers.push_back(GeneratedLayer(
        PromptLayer::UserMarkingsAndRepairRequests,
        "selected item request",
        PromptInputFormat::Markdown,
        BuildSelectedItemTaskInstruction(request)));
    result.layers.push_back(GeneratedLayer(
        PromptLayer::OutputContract,
        "selected item output contract",
        PromptInputFormat::Json,
        BuildSelectedItemOutputContractJson()));

    result.ok = result.issues.empty();
    return result;
}

std::string SerializePromptPackageDebugDump(const PromptPackageBuildResult& result)
{
    std::ostringstream output;
    output << "# ArtForge Prompt Package Debug Dump\n\n";
    output << "Status: " << (result.ok ? "OK" : "Has issues") << "\n\n";

    if (!result.issues.empty()) {
        output << "## Issues\n\n";
        for (const auto& issue : result.issues) {
            output << "- " << issue << "\n";
        }
        output << "\n";
    }

    output << "## Layers\n\n";
    for (const auto& layer : result.layers) {
        output << "### " << ToDisplayName(layer.layer) << "\n\n";
        output << "- Role: " << layer.role << "\n";
        output << "- Format: " << ToDisplayName(layer.format) << "\n";
        output << "- Source: " << (layer.sourcePath.empty() ? "(generated)" : layer.sourcePath.string()) << "\n";
        output << "- Status: " << (layer.loaded ? "loaded" : "load failed") << "\n\n";
        if (layer.loaded) {
            output << "```" << FenceName(layer.format) << "\n";
            output << layer.content;
            if (layer.content.empty() || layer.content.back() != '\n') {
                output << "\n";
            }
            output << "```\n\n";
        } else {
            output << layer.loadError << "\n\n";
        }
    }

    return output.str();
}

AiResultValidationResult ValidateAiResultJsonText(std::string_view jsonText)
{
    AiResultValidationResult result;
    if (jsonText.size() >= 3
        && static_cast<unsigned char>(jsonText[0]) == 0xef
        && static_cast<unsigned char>(jsonText[1]) == 0xbb
        && static_cast<unsigned char>(jsonText[2]) == 0xbf) {
        jsonText.remove_prefix(3);
    }
    const auto first = jsonText.find_first_not_of(" \t\r\n");
    const auto last = jsonText.find_last_not_of(" \t\r\n");
    if (first == std::string_view::npos || jsonText[first] != '{' || jsonText[last] != '}') {
        result.diagnostics.push_back("AI result must be a JSON object.");
    }

    for (const auto key : {
             "suggestions",
             "critiqueItems",
             "replacementCandidates",
             "ruleHits",
             "ruleViolations",
             "unresolvedQuestions",
             "confidence",
             "rationale",
         }) {
        if (!ContainsField(jsonText, key)) {
            result.diagnostics.push_back("missing required AI result section: " + std::string{key});
        }
    }
    for (const auto key : {"resultId", "promptPackageId", "target", "replacementCandidates"}) {
        if (!ContainsField(jsonText, key)) {
            result.diagnostics.push_back("missing required field: " + std::string{key});
        }
    }

    if (result.diagnostics.empty()) {
        PendingSuggestion suggestion;
        suggestion.suggestionId = ExtractStringField(jsonText, "id");
        suggestion.promptPackageId = ExtractStringField(jsonText, "promptPackageId");
        suggestion.target.workPath = ExtractStringField(jsonText, "workPath");
        suggestion.target.domain = ExtractStringField(jsonText, "domain");
        suggestion.target.itemType = ExtractStringField(jsonText, "itemType");
        suggestion.target.itemId = ExtractStringField(jsonText, "itemId");
        suggestion.target.field = ExtractStringField(jsonText, "field");
        suggestion.proposedText = ExtractStringField(jsonText, "proposedText");
        suggestion.rationale = ExtractStringField(jsonText, "rationale");
        suggestion.status = PendingSuggestionStatus::Pending;
        result.pendingSuggestions.push_back(std::move(suggestion));
    }

    result.ok = result.diagnostics.empty();
    return result;
}

std::string DescribeAiResultValidation(const AiResultValidationResult& result)
{
    std::ostringstream output;
    output << "AI result validation: " << (result.ok ? "OK" : "failed") << "\n";
    output << "Pending suggestions: " << result.pendingSuggestions.size() << "\n";
    for (const auto& diagnostic : result.diagnostics) {
        output << "Diagnostic: " << diagnostic << "\n";
    }
    for (const auto& suggestion : result.pendingSuggestions) {
        output << "Suggestion: " << suggestion.suggestionId << " status=" << ToDisplayName(suggestion.status) << "\n";
        output << "Target: " << suggestion.target.domain << "/" << suggestion.target.itemType << "#" << suggestion.target.itemId << "." << suggestion.target.field << "\n";
        output << "Proposed text: " << suggestion.proposedText << "\n";
    }
    return output.str();
}

std::string SerializePendingSuggestionJsonLine(const PendingSuggestion& suggestion)
{
    std::ostringstream output;
    output << "{";
    output << "\"id\":" << Quote(suggestion.suggestionId) << ",";
    output << "\"prompt_package_id\":" << Quote(suggestion.promptPackageId) << ",";
    output << "\"prompt_package_path\":" << Quote(suggestion.promptPackagePath.generic_string()) << ",";
    output << "\"status\":" << Quote(ToDisplayName(suggestion.status)) << ",";
    output << "\"target\":{";
    output << "\"work_path\":" << Quote(suggestion.target.workPath.generic_string()) << ",";
    output << "\"domain\":" << Quote(suggestion.target.domain) << ",";
    output << "\"item_type\":" << Quote(suggestion.target.itemType) << ",";
    output << "\"item_id\":" << Quote(suggestion.target.itemId) << ",";
    output << "\"item_index\":" << suggestion.target.itemIndex << ",";
    output << "\"field\":" << Quote(suggestion.target.field);
    output << "},";
    output << "\"proposed_text\":" << Quote(suggestion.proposedText) << ",";
    output << "\"rationale\":" << Quote(suggestion.rationale) << ",";
    output << "\"diagnostics\":[";
    for (std::size_t index = 0; index < suggestion.diagnostics.size(); ++index) {
        output << Quote(suggestion.diagnostics[index]);
        if (index + 1 < suggestion.diagnostics.size()) {
            output << ",";
        }
    }
    output << "]";
    output << "}";
    return output.str();
}

std::string DescribeAiExecutionModel()
{
    std::ostringstream output;
    output << "AI execution request model\n";
    output << "Provider kinds:\n";
    for (const auto provider : {
             AiProviderKind::ManualQueue,
             AiProviderKind::OpenAI,
             AiProviderKind::Anthropic,
             AiProviderKind::AlibabaCloud,
             AiProviderKind::Unknown,
             AiProviderKind::Unsupported,
         }) {
        output << "- " << ToDisplayName(provider) << "\n";
    }
    output << "Statuses:\n";
    for (const auto status : {
             AiExecutionStatus::Draft,
             AiExecutionStatus::Queued,
             AiExecutionStatus::WaitingForResult,
             AiExecutionStatus::ResultFound,
             AiExecutionStatus::ResultInvalid,
             AiExecutionStatus::ImportedPendingSuggestion,
             AiExecutionStatus::TargetMismatch,
             AiExecutionStatus::NotConfigured,
             AiExecutionStatus::NotImplemented,
             AiExecutionStatus::UnsupportedProvider,
             AiExecutionStatus::Failed,
         }) {
        output << "- " << ToDisplayName(status) << "\n";
    }
    output << "Request fields exclude API credentials and other secrets.\n";
    output << "Providers return validated AI result JSON before pending-suggestion import.\n";
    return output.str();
}

}
