#include "ArtForge/Prompting/PromptPackage.hpp"

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

}
