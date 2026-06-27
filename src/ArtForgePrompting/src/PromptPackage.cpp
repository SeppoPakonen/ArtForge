#include "ArtForge/Prompting/PromptPackage.hpp"

#include "ArtForge/Files/DomainWorkViewModels.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>

#pragma comment(lib, "winhttp.lib")

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

std::wstring Utf8ToWide(std::string_view value)
{
    if (value.empty()) {
        return {};
    }
    const auto size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return std::wstring{value.begin(), value.end()};
    }
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string WideToUtf8(std::wstring_view value)
{
    if (value.empty()) {
        return {};
    }
    const auto size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        std::string fallback;
        for (const auto character : value) {
            fallback += character <= 0x7f ? static_cast<char>(character) : '?';
        }
        return fallback;
    }
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

std::string LastWin32ErrorMessage()
{
    const auto error = GetLastError();
    if (error == 0) {
        return {};
    }
    wchar_t* buffer = nullptr;
    const auto length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<LPWSTR>(&buffer),
        0,
        nullptr);
    std::string message = "WinHTTP error " + std::to_string(error);
    if (length > 0 && buffer != nullptr) {
        message += ": " + WideToUtf8(std::wstring_view{buffer, length});
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    return message;
}

struct WinHttpHandle {
    HINTERNET value{};
    explicit WinHttpHandle(HINTERNET handle = nullptr) : value(handle) {}
    ~WinHttpHandle()
    {
        if (value != nullptr) {
            WinHttpCloseHandle(value);
        }
    }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    operator HINTERNET() const { return value; }
};

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
            switch (character) {
            case 'n':
                value += '\n';
                break;
            case 'r':
                value += '\r';
                break;
            case 't':
                value += '\t';
                break;
            case '\\':
                value += '\\';
                break;
            case '"':
                value += '"';
                break;
            default:
                value += character;
                break;
            }
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

std::string Trim(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::string ExtractFirstJsonObject(std::string_view text)
{
    const auto begin = text.find('{');
    if (begin == std::string_view::npos) {
        return {};
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (auto index = begin; index < text.size(); ++index) {
        const char character = text[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (character == '{') {
            ++depth;
            continue;
        }
        if (character == '}') {
            --depth;
            if (depth == 0) {
                return std::string{text.substr(begin, index - begin + 1)};
            }
        }
    }
    return {};
}

std::string ReadEnvironmentVariable(std::string_view name)
{
    std::string result;
    char* value = nullptr;
    std::size_t size = 0;
    const auto variable = std::string{name};
    if (_dupenv_s(&value, &size, variable.c_str()) == 0 && value != nullptr) {
        result.assign(value, size > 0 ? size - 1 : 0);
        std::free(value);
    }
    return result;
}

int NextManualQueueSequence(const std::filesystem::path& queueRoot)
{
    int maxSequence = 0;
    if (!std::filesystem::exists(queueRoot)) {
        return 1;
    }
    for (const auto& entry : std::filesystem::directory_iterator{queueRoot}) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.size() < 6) {
            continue;
        }
        bool allDigits = true;
        for (int index = 0; index < 6; ++index) {
            allDigits = allDigits && name[static_cast<std::size_t>(index)] >= '0' && name[static_cast<std::size_t>(index)] <= '9';
        }
        if (allDigits) {
            maxSequence = std::max(maxSequence, std::stoi(name.substr(0, 6)));
        }
    }
    return maxSequence + 1;
}

std::string SafeSlug(std::string_view value)
{
    std::string slug;
    bool previousDash = false;
    for (const auto character : value) {
        const bool isAlphaNumeric =
            (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9');
        if (isAlphaNumeric) {
            slug += static_cast<char>(character >= 'A' && character <= 'Z' ? character - 'A' + 'a' : character);
            previousDash = false;
            continue;
        }
        if (!previousDash && !slug.empty()) {
            slug += '-';
            previousDash = true;
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug.empty() ? "ai-task" : slug;
}

std::string SequencePrefix(int sequence)
{
    std::ostringstream output;
    output << std::setw(6) << std::setfill('0') << sequence;
    return output.str();
}

std::string BuildManualQueueRequestJson(const AiExecutionRequest& request, int sequence)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"format\": \"artforge.manualAiQueue.request\",\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"id\": " << Quote(request.requestId) << ",\n";
    output << "  \"sequence\": " << sequence << ",\n";
    output << "  \"createdAt\": \"generated-by-artforge\",\n";
    output << "  \"providerKind\": " << Quote(ToDisplayName(request.providerKind)) << ",\n";
    output << "  \"requestedOperation\": " << Quote(request.requestedOperation) << ",\n";
    output << "  \"promptPackage\": {\n";
    output << "    \"path\": " << Quote(request.promptPackagePath.generic_string()) << ",\n";
    output << "    \"summary\": " << Quote(request.promptPackageSummary) << "\n";
    output << "  },\n";
    output << "  \"copyPastePromptPath\": " << Quote(request.locations.promptTextPath.filename().generic_string()) << ",\n";
    output << "  \"expectedResultPath\": " << Quote(request.locations.expectedResultPath.filename().generic_string()) << ",\n";
    output << "  \"statusPath\": " << Quote(request.locations.statusPath.filename().generic_string()) << ",\n";
    output << "  \"resultSchemaPath\": " << Quote(request.resultSchemaPath.generic_string()) << ",\n";
    output << "  \"target\": {\n";
    output << "    \"workPath\": " << Quote(request.target.workPath.generic_string()) << ",\n";
    output << "    \"domain\": " << Quote(request.target.domain) << ",\n";
    output << "    \"itemType\": " << Quote(request.target.itemType) << ",\n";
    output << "    \"itemId\": " << Quote(request.target.itemId) << ",\n";
    output << "    \"itemIndex\": " << request.target.itemIndex << ",\n";
    output << "    \"field\": " << Quote(request.target.field) << "\n";
    output << "  }\n";
    output << "}\n";
    return output.str();
}

std::string BuildManualQueueStatusJson(const AiExecutionRequest& request, AiExecutionStatus status)
{
    std::ostringstream output;
    output << "{\n";
    output << "  \"format\": \"artforge.manualAiQueue.status\",\n";
    output << "  \"schemaVersion\": 1,\n";
    output << "  \"requestId\": " << Quote(request.requestId) << ",\n";
    output << "  \"status\": " << Quote(ToDisplayName(status)) << ",\n";
    output << "  \"updatedAt\": \"generated-by-artforge\",\n";
    output << "  \"diagnostics\": []\n";
    output << "}\n";
    return output.str();
}

bool WriteTextFile(const std::filesystem::path& path, std::string_view content)
{
    std::ofstream output{path, std::ios::binary};
    if (!output) {
        return false;
    }
    output << content;
    return static_cast<bool>(output);
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
    output << "\"rejection_reason\":" << Quote(suggestion.rejectionReason) << ",";
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

std::vector<AiProviderConfiguration> DefaultAiProviderConfigurationStubs()
{
    return {
        {AiProviderKind::OpenAI, "OpenAI", "user-local-openai-profile", "model-placeholder", false},
        {AiProviderKind::Anthropic, "Anthropic", "user-local-anthropic-profile", "model-placeholder", false},
        {AiProviderKind::AlibabaCloud, "Alibaba Cloud", "user-local-alibaba-cloud-profile", "model-placeholder", false},
    };
}

std::string DescribeAiProviderConfigurationStubs(const std::vector<AiProviderConfiguration>& configurations)
{
    std::ostringstream output;
    output << "AI provider configuration stubs\n";
    for (const auto& configuration : configurations) {
        output << "- " << configuration.displayName << " provider=" << ToDisplayName(configuration.providerKind)
               << " enabled=" << (configuration.enabled ? "yes" : "no")
               << " endpointProfile=" << configuration.endpointProfile
               << " model=" << configuration.modelName << "\n";
        output << "  status: " << (configuration.enabled ? "configured placeholder" : "not configured") << "\n";
    }
    output << "Credential values are intentionally not part of provider configuration stubs.\n";
    return output.str();
}

ManualAiQueueWriteResult WriteManualAiQueueRequest(const ManualAiQueueWriteRequest& request)
{
    ManualAiQueueWriteResult result;
    result.execution = request.execution;
    result.execution.providerKind = AiProviderKind::ManualQueue;
    result.providerResult.providerKind = AiProviderKind::ManualQueue;

    if (result.execution.queueRoot.empty()) {
        result.diagnostics.push_back("queue root is required");
    }
    if (request.promptText.empty()) {
        result.diagnostics.push_back("prompt text is required");
    }
    if (result.execution.requestedOperation.empty()) {
        result.diagnostics.push_back("requested operation is required");
    }
    if (result.execution.target.domain.empty() || result.execution.target.itemType.empty()) {
        result.diagnostics.push_back("target domain and item type are required");
    }
    if (!result.diagnostics.empty()) {
        result.providerResult.status = AiExecutionStatus::Failed;
        return result;
    }

    std::error_code error;
    std::filesystem::create_directories(result.execution.queueRoot, error);
    if (error) {
        result.diagnostics.push_back("could not create queue root: " + error.message());
        result.providerResult.status = AiExecutionStatus::Failed;
        return result;
    }

    const int sequence = NextManualQueueSequence(result.execution.queueRoot);
    const auto prefix = SequencePrefix(sequence);
    const auto slug = SafeSlug(result.execution.target.domain + "-" + result.execution.requestedOperation);
    const auto stem = prefix + "-" + slug;
    result.execution.requestId = "manual-" + stem;
    result.execution.locations.requestPath = result.execution.queueRoot / (stem + ".request.json");
    result.execution.locations.promptTextPath = result.execution.queueRoot / (stem + ".prompt.txt");
    result.execution.locations.expectedResultPath = result.execution.queueRoot / (stem + ".result.json");
    result.execution.locations.statusPath = result.execution.queueRoot / (stem + ".status.json");

    const auto requestJson = BuildManualQueueRequestJson(result.execution, sequence);
    const auto statusJson = BuildManualQueueStatusJson(result.execution, AiExecutionStatus::Queued);
    if (!WriteTextFile(result.execution.locations.requestPath, requestJson)) {
        result.diagnostics.push_back("could not write request file");
    }
    if (!WriteTextFile(result.execution.locations.promptTextPath, request.promptText)) {
        result.diagnostics.push_back("could not write prompt text file");
    }
    if (!WriteTextFile(result.execution.locations.statusPath, statusJson)) {
        result.diagnostics.push_back("could not write status file");
    }

    result.ok = result.diagnostics.empty();
    result.providerResult.requestId = result.execution.requestId;
    result.providerResult.locations = result.execution.locations;
    result.providerResult.status = result.ok ? AiExecutionStatus::Queued : AiExecutionStatus::Failed;
    result.providerResult.diagnostics = result.diagnostics;
    return result;
}

std::string DescribeManualAiQueueWriteResult(const ManualAiQueueWriteResult& result)
{
    std::ostringstream output;
    output << "Manual AI queue write: " << (result.ok ? "OK" : "failed") << "\n";
    output << "Request id: " << result.execution.requestId << "\n";
    output << "Status: " << ToDisplayName(result.providerResult.status) << "\n";
    output << "Request file: " << result.execution.locations.requestPath.generic_string() << "\n";
    output << "Prompt text file: " << result.execution.locations.promptTextPath.generic_string() << "\n";
    output << "Expected result file: " << result.execution.locations.expectedResultPath.generic_string() << "\n";
    output << "Status file: " << result.execution.locations.statusPath.generic_string() << "\n";
    for (const auto& diagnostic : result.diagnostics) {
        output << "Diagnostic: " << diagnostic << "\n";
    }
    return output.str();
}

ManualAiQueuePollResult PollManualAiQueueResult(const ManualAiQueuePollRequest& request)
{
    ManualAiQueuePollResult result;
    result.providerResult.requestId = request.execution.requestId;
    result.providerResult.providerKind = AiProviderKind::ManualQueue;
    result.providerResult.locations = request.execution.locations;

    if (request.execution.locations.expectedResultPath.empty()) {
        result.providerResult.status = AiExecutionStatus::Failed;
        result.diagnostics.push_back("expected result path is required");
        return result;
    }
    if (!std::filesystem::exists(request.execution.locations.expectedResultPath)) {
        result.ok = true;
        result.providerResult.status = AiExecutionStatus::WaitingForResult;
        result.diagnostics.push_back("result file is not present yet");
        return result;
    }

    std::ifstream input{request.execution.locations.expectedResultPath, std::ios::binary};
    if (!input) {
        result.providerResult.status = AiExecutionStatus::Failed;
        result.diagnostics.push_back("could not open result file");
        return result;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();

    auto validation = ValidateAiResultJsonText(buffer.str());
    if (!validation.ok) {
        result.providerResult.status = AiExecutionStatus::ResultInvalid;
        result.diagnostics = validation.diagnostics;
        result.providerResult.diagnostics = validation.diagnostics;
        return result;
    }

    for (const auto& suggestion : validation.pendingSuggestions) {
        if (suggestion.target.workPath.generic_string() != request.execution.target.workPath.generic_string()
            || suggestion.target.domain != request.execution.target.domain
            || suggestion.target.itemType != request.execution.target.itemType
            || suggestion.target.itemId != request.execution.target.itemId) {
            result.providerResult.status = AiExecutionStatus::TargetMismatch;
            result.diagnostics.push_back("AI result target does not match the queued request target");
            result.providerResult.diagnostics = result.diagnostics;
            return result;
        }
    }

    result.providerResult.pendingSuggestions = validation.pendingSuggestions;
    if (!request.importPendingSuggestions) {
        result.ok = true;
        result.providerResult.status = AiExecutionStatus::ResultFound;
        return result;
    }

    const auto outputPath = request.execution.target.workPath.parent_path() / "pending-suggestions.jsonl";
    std::ofstream pending{outputPath, std::ios::app};
    if (!pending) {
        result.providerResult.status = AiExecutionStatus::Failed;
        result.diagnostics.push_back("could not open pending suggestion file for append");
        result.providerResult.diagnostics = result.diagnostics;
        return result;
    }
    for (auto suggestion : validation.pendingSuggestions) {
        suggestion.promptPackagePath = request.execution.locations.expectedResultPath;
        pending << SerializePendingSuggestionJsonLine(suggestion) << "\n";
    }

    result.ok = true;
    result.providerResult.status = AiExecutionStatus::ImportedPendingSuggestion;
    result.providerResult.locations.importedSuggestionsPath = outputPath;
    return result;
}

std::string DescribeManualAiQueuePollResult(const ManualAiQueuePollResult& result)
{
    std::ostringstream output;
    output << "Manual AI queue poll: " << (result.ok ? "OK" : "failed") << "\n";
    output << "Status: " << ToDisplayName(result.providerResult.status) << "\n";
    output << "Result file: " << result.providerResult.locations.expectedResultPath.generic_string() << "\n";
    output << "Pending suggestions: " << result.providerResult.pendingSuggestions.size() << "\n";
    if (!result.providerResult.locations.importedSuggestionsPath.empty()) {
        output << "Imported suggestions file: " << result.providerResult.locations.importedSuggestionsPath.generic_string() << "\n";
    }
    for (const auto& diagnostic : result.diagnostics) {
        output << "Diagnostic: " << diagnostic << "\n";
    }
    for (const auto& diagnostic : result.providerResult.diagnostics) {
        if (std::find(result.diagnostics.begin(), result.diagnostics.end(), diagnostic) == result.diagnostics.end()) {
            output << "Diagnostic: " << diagnostic << "\n";
        }
    }
    return output.str();
}

HttpJsonPostResponse FakeHttpJsonPostResponse(int statusCode, std::string body)
{
    HttpJsonPostResponse response;
    response.ok = statusCode >= 200 && statusCode < 300;
    response.statusCode = statusCode;
    response.body = std::move(body);
    if (!response.ok) {
        response.errorCode = "http_status";
        response.errorMessage = "HTTP status " + std::to_string(statusCode);
    }
    return response;
}

HttpJsonPostResponse PostJsonWithWinHttp(const HttpJsonPostRequest& request)
{
    HttpJsonPostResponse response;
    const auto wideUrl = Utf8ToWide(request.url);
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwSchemeLength = static_cast<DWORD>(-1);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &parts)) {
        response.errorCode = "url_parse_failed";
        response.errorMessage = LastWin32ErrorMessage();
        return response;
    }

    const std::wstring host{parts.lpszHostName, parts.dwHostNameLength};
    std::wstring path{parts.lpszUrlPath, parts.dwUrlPathLength};
    if (parts.dwExtraInfoLength > 0) {
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }

    WinHttpHandle session{WinHttpOpen(L"ArtForge/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)};
    if (session.value == nullptr) {
        response.errorCode = "session_failed";
        response.errorMessage = LastWin32ErrorMessage();
        return response;
    }
    if (request.timeoutMilliseconds > 0) {
        WinHttpSetTimeouts(session, request.timeoutMilliseconds, request.timeoutMilliseconds, request.timeoutMilliseconds, request.timeoutMilliseconds);
    }

    WinHttpHandle connection{WinHttpConnect(session, host.c_str(), parts.nPort, 0)};
    if (connection.value == nullptr) {
        response.errorCode = "connect_failed";
        response.errorMessage = LastWin32ErrorMessage();
        return response;
    }

    const auto secure = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle httpRequest{WinHttpOpenRequest(connection, L"POST", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, secure)};
    if (httpRequest.value == nullptr) {
        response.errorCode = "request_failed";
        response.errorMessage = LastWin32ErrorMessage();
        return response;
    }

    std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\nAccept: application/json\r\n";
    for (const auto& header : request.headers) {
        headers += Utf8ToWide(header.name);
        headers += L": ";
        headers += Utf8ToWide(header.value);
        headers += L"\r\n";
    }

    const auto bodySize = static_cast<DWORD>(request.jsonBody.size());
    if (!WinHttpSendRequest(
            httpRequest,
            headers.c_str(),
            static_cast<DWORD>(headers.size()),
            request.jsonBody.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.jsonBody.data()),
            bodySize,
            bodySize,
            0)) {
        response.errorCode = "send_failed";
        response.errorMessage = LastWin32ErrorMessage();
        return response;
    }

    if (!WinHttpReceiveResponse(httpRequest, nullptr)) {
        response.errorCode = "receive_failed";
        response.errorMessage = LastWin32ErrorMessage();
        return response;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(httpRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        response.statusCode = static_cast<int>(statusCode);
    }

    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(httpRequest, &available)) {
            response.errorCode = "read_available_failed";
            response.errorMessage = LastWin32ErrorMessage();
            return response;
        }
        if (available == 0) {
            break;
        }
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(httpRequest, chunk.data(), available, &read)) {
            response.errorCode = "read_failed";
            response.errorMessage = LastWin32ErrorMessage();
            return response;
        }
        chunk.resize(read);
        response.body += chunk;
    }

    response.ok = response.statusCode >= 200 && response.statusCode < 300;
    if (!response.ok) {
        response.errorCode = "http_status";
        response.errorMessage = "HTTP status " + std::to_string(response.statusCode);
    }
    return response;
}

std::string DescribeHttpJsonPostResponse(const HttpJsonPostResponse& response)
{
    std::ostringstream output;
    output << "HTTP JSON POST: " << (response.ok ? "OK" : "failed") << "\n";
    output << "Status code: " << response.statusCode << "\n";
    if (!response.errorCode.empty()) {
        output << "Error code: " << response.errorCode << "\n";
        output << "Error message: " << response.errorMessage << "\n";
    }
    output << "Body bytes: " << response.body.size() << "\n";
    if (!response.body.empty()) {
        output << "Body: " << response.body << "\n";
    }
    return output.str();
}

std::string DescribeHttpJsonPostSmokeExamples()
{
    std::ostringstream output;
    const HttpJsonPostRequest request{
        "https://example.invalid/v1/smoke",
        {{"X-ArtForge-Test", "fake-transport"}},
        "{\"hello\":\"world\"}",
        1000,
    };
    (void)request;
    output << "Fake transport success\n";
    output << DescribeHttpJsonPostResponse(FakeHttpJsonPostResponse(200, "{\"ok\":true}")) << "\n";
    output << "Fake transport failure\n";
    output << DescribeHttpJsonPostResponse(FakeHttpJsonPostResponse(503, "{\"error\":\"unavailable\"}"));
    return output.str();
}

std::string BuildOpenAiResponsesRequestJson(const OpenAiRequestMappingRequest& request)
{
    const auto model = request.providerConfiguration.modelName.empty()
        ? std::string{"model-placeholder"}
        : request.providerConfiguration.modelName;
    const auto schema = Trim(request.resultSchemaJson).empty()
        ? std::string{"{\"type\":\"object\"}"}
        : Trim(request.resultSchemaJson);

    std::ostringstream output;
    output << "{\n";
    output << "  \"model\": " << Quote(model) << ",\n";
    output << "  \"stream\": false,\n";
    output << "  \"metadata\": {\n";
    output << "    \"artforgeRequestId\": " << Quote(request.execution.requestId) << ",\n";
    output << "    \"artforgeProvider\": \"openai\",\n";
    output << "    \"requestedOperation\": " << Quote(request.execution.requestedOperation) << "\n";
    output << "  },\n";
    output << "  \"input\": [\n";
    output << "    {\n";
    output << "      \"role\": \"developer\",\n";
    output << "      \"content\": \"Return exactly one ArtForge AI result JSON object. Do not include Markdown fences or explanatory text.\"\n";
    output << "    },\n";
    output << "    {\n";
    output << "      \"role\": \"user\",\n";
    output << "      \"content\": " << Quote(request.promptText) << "\n";
    output << "    }\n";
    output << "  ],\n";
    output << "  \"text\": {\n";
    output << "    \"format\": {\n";
    output << "      \"type\": \"json_schema\",\n";
    output << "      \"name\": \"artforge_ai_result\",\n";
    output << "      \"strict\": true,\n";
    output << "      \"schema\": " << schema << "\n";
    output << "    }\n";
    output << "  }\n";
    output << "}\n";
    return output.str();
}

OpenAiResponseMappingResult ExtractOpenAiResponseResultJson(
    std::string_view responseJson,
    const AiExecutionRequest& execution)
{
    OpenAiResponseMappingResult result;
    result.requestId = execution.requestId;
    result.providerResult.requestId = execution.requestId;
    result.providerResult.providerKind = AiProviderKind::OpenAI;
    result.providerResult.locations = execution.locations;

    const auto trimmedResponse = Trim(std::string{responseJson});
    if (trimmedResponse.empty() || trimmedResponse.front() != '{' || trimmedResponse.back() != '}') {
        result.providerResult.status = AiExecutionStatus::ResultInvalid;
        result.diagnostics.push_back("stage=providerResponseParse: OpenAI response must be a JSON object.");
        result.providerResult.diagnostics = result.diagnostics;
        return result;
    }
    if (ContainsField(responseJson, "error")) {
        result.providerResult.status = AiExecutionStatus::Failed;
        const auto message = ExtractStringField(responseJson, "message");
        result.diagnostics.push_back("stage=providerResponseParse: OpenAI response reported an error.");
        if (!message.empty()) {
            result.diagnostics.push_back("stage=providerResponseParse: " + message);
        }
        result.providerResult.diagnostics = result.diagnostics;
        return result;
    }

    auto rawCandidate = ExtractStringField(responseJson, "output_text");
    if (rawCandidate.empty()) {
        rawCandidate = ExtractStringField(responseJson, "text");
    }
    if (rawCandidate.empty() && ContainsField(responseJson, "format") && ContainsField(responseJson, "artforge.ai.selectedItemResult")) {
        rawCandidate = std::string{responseJson};
    }
    auto candidate = ExtractFirstJsonObject(rawCandidate);
    if (candidate.empty() && rawCandidate.find('{') != std::string::npos) {
        result.providerResult.status = AiExecutionStatus::ResultInvalid;
        result.diagnostics.push_back("stage=jsonParse: OpenAI candidate text is not a complete JSON object.");
        result.providerResult.diagnostics = result.diagnostics;
        return result;
    }
    if (candidate.empty()) {
        result.providerResult.status = AiExecutionStatus::ResultInvalid;
        result.diagnostics.push_back("stage=candidateExtraction: OpenAI response did not contain an ArtForge JSON result candidate.");
        result.providerResult.diagnostics = result.diagnostics;
        return result;
    }

    result.candidateJson = candidate;
    auto validation = ValidateAiResultJsonText(candidate);
    if (!validation.ok) {
        result.providerResult.status = AiExecutionStatus::ResultInvalid;
        result.diagnostics.push_back("stage=artforgeContract: OpenAI candidate failed the ArtForge AI result contract.");
        for (const auto& diagnostic : validation.diagnostics) {
            result.diagnostics.push_back("stage=artforgeContract: " + diagnostic);
        }
        result.providerResult.diagnostics = result.diagnostics;
        return result;
    }
    for (const auto& suggestion : validation.pendingSuggestions) {
        if (suggestion.target.workPath.generic_string() != execution.target.workPath.generic_string()
            || suggestion.target.domain != execution.target.domain
            || suggestion.target.itemType != execution.target.itemType
            || suggestion.target.itemId != execution.target.itemId
            || suggestion.target.field != execution.target.field) {
            result.providerResult.status = AiExecutionStatus::TargetMismatch;
            result.diagnostics.push_back("stage=targetCompatibility: OpenAI candidate target does not match the execution request.");
            result.diagnostics.push_back("stage=targetCompatibility: expected "
                + execution.target.workPath.generic_string() + " "
                + execution.target.domain + "/" + execution.target.itemType + "#"
                + execution.target.itemId + "." + execution.target.field);
            result.diagnostics.push_back("stage=targetCompatibility: actual "
                + suggestion.target.workPath.generic_string() + " "
                + suggestion.target.domain + "/" + suggestion.target.itemType + "#"
                + suggestion.target.itemId + "." + suggestion.target.field);
            result.providerResult.diagnostics = result.diagnostics;
            return result;
        }
    }

    result.ok = true;
    result.providerResult.status = AiExecutionStatus::ResultFound;
    result.providerResult.pendingSuggestions = validation.pendingSuggestions;
    result.diagnostics.push_back("stage=transport: OpenAI HTTP response was successful.");
    result.diagnostics.push_back("stage=providerResponseParse: OpenAI response JSON object parsed.");
    result.diagnostics.push_back("stage=candidateExtraction: OpenAI ArtForge result candidate extracted.");
    result.diagnostics.push_back("stage=artforgeContract: OpenAI candidate validated as ArtForge AI result JSON.");
    result.diagnostics.push_back("stage=targetCompatibility: OpenAI candidate target matched the execution request.");
    result.providerResult.diagnostics = result.diagnostics;
    return result;
}

std::string DescribeOpenAiMappingSmokeExamples(std::string_view responseJson)
{
    AiExecutionRequest execution;
    execution.requestId = "openai-map-smoke-001";
    execution.providerKind = AiProviderKind::OpenAI;
    execution.promptPackagePath = "examples/prompt-selected-items/lyrics-line-repair.afprompt.json";
    execution.promptPackageSummary = "Selected item prompt package";
    execution.resultSchemaPath = "examples/prompt-build/output_schema.json";
    execution.requestedOperation = "line-repair";
    execution.target.workPath = "examples/work-domains/lyrics.afwork.json";
    execution.target.domain = "lyrics";
    execution.target.itemType = "lyricLine";
    execution.target.itemId = "line.v1.001";
    execution.target.itemIndex = 0;
    execution.target.field = "text";

    const auto requestJson = BuildOpenAiResponsesRequestJson({
        execution,
        {AiProviderKind::OpenAI, "OpenAI", "responses", "gpt-placeholder", true},
        "# ArtForge prompt package\n\nReturn JSON for the selected lyric line.",
        "{\"type\":\"object\",\"additionalProperties\":true}",
    });
    const auto mapped = ExtractOpenAiResponseResultJson(responseJson, execution);

    std::ostringstream output;
    output << "OpenAI request mapping\n";
    output << requestJson << "\n";
    output << "OpenAI response mapping: " << (mapped.ok ? "OK" : "failed") << "\n";
    output << "Candidate bytes: " << mapped.candidateJson.size() << "\n";
    output << DescribeAiExecutionResult(mapped.providerResult);
    for (const auto& diagnostic : mapped.diagnostics) {
        output << "Mapping diagnostic: " << diagnostic << "\n";
    }
    return output.str();
}

std::optional<AiProviderConfiguration> FindProviderConfiguration(
    const std::vector<AiProviderConfiguration>& configurations,
    AiProviderKind providerKind)
{
    const auto found = std::find_if(
        configurations.begin(),
        configurations.end(),
        [&](const AiProviderConfiguration& configuration) {
            return configuration.providerKind == providerKind;
        });
    if (found == configurations.end()) {
        return std::nullopt;
    }
    return *found;
}

std::string OpenAiResponsesEndpointUrl(const AiProviderConfiguration& configuration)
{
    if (configuration.endpointProfile.rfind("https://", 0) == 0
        || configuration.endpointProfile.rfind("http://", 0) == 0) {
        return configuration.endpointProfile;
    }
    if (configuration.endpointProfile.empty()) {
        return {};
    }
    return "https://api.openai.com/v1/responses";
}

AiExecutionResult NotConfiguredOpenAiResult(
    const AiExecutionRequest& execution,
    std::string message)
{
    AiExecutionResult result;
    result.requestId = execution.requestId;
    result.providerKind = AiProviderKind::OpenAI;
    result.locations = execution.locations;
    result.status = AiExecutionStatus::NotConfigured;
    result.errors.push_back({"not-configured", std::move(message)});
    return result;
}

HttpJsonPostRequest BuildOpenAiHttpJsonPostRequest(
    const AiProviderDispatchRequest& request,
    const AiProviderConfiguration& configuration,
    std::string_view apiKey)
{
    const auto requestJson = BuildOpenAiResponsesRequestJson({
        request.execution,
        configuration,
        request.promptText,
        "{\"type\":\"object\",\"additionalProperties\":true}",
    });

    return {
        OpenAiResponsesEndpointUrl(configuration),
        {
            {"Authorization", "Bearer " + std::string{apiKey}},
        },
        requestJson,
        30000,
    };
}

AiExecutionResult DispatchOpenAiProviderWithResponse(
    const AiProviderDispatchRequest& request,
    const AiProviderConfiguration& configuration,
    const HttpJsonPostResponse& httpResponse)
{
    AiExecutionResult result;
    result.requestId = request.execution.requestId;
    result.providerKind = AiProviderKind::OpenAI;
    result.locations = request.execution.locations;

    if (!httpResponse.ok) {
        result.status = AiExecutionStatus::Failed;
        result.errors.push_back({
            httpResponse.errorCode.empty() ? "http-request-failed" : httpResponse.errorCode,
            httpResponse.errorMessage.empty() ? "OpenAI HTTP request failed." : httpResponse.errorMessage,
        });
        result.diagnostics.push_back("stage=transport: OpenAI non-streaming dispatch failed before result validation.");
        return result;
    }

    auto mapped = ExtractOpenAiResponseResultJson(httpResponse.body, request.execution);
    result = mapped.providerResult;
    result.requestId = request.execution.requestId;
    result.providerKind = AiProviderKind::OpenAI;
    result.locations = request.execution.locations;
    result.diagnostics.push_back("OpenAI model: " + configuration.modelName);
    for (const auto& diagnostic : mapped.diagnostics) {
        if (std::find(result.diagnostics.begin(), result.diagnostics.end(), diagnostic) == result.diagnostics.end()) {
            result.diagnostics.push_back(diagnostic);
        }
    }
    return result;
}

AiExecutionResult DispatchOpenAiProvider(
    const AiProviderDispatchRequest& request,
    const HttpJsonPostResponse* fakeResponse)
{
    auto configuration = FindProviderConfiguration(request.providerConfigurations, AiProviderKind::OpenAI);
    if (!configuration.has_value() || !configuration->enabled) {
        return NotConfiguredOpenAiResult(request.execution, "OpenAI provider is not enabled.");
    }
    if (configuration->modelName.empty()) {
        return NotConfiguredOpenAiResult(request.execution, "OpenAI provider model name is missing.");
    }
    if (OpenAiResponsesEndpointUrl(*configuration).empty()) {
        return NotConfiguredOpenAiResult(request.execution, "OpenAI provider endpoint profile is missing.");
    }

    if (fakeResponse != nullptr) {
        return DispatchOpenAiProviderWithResponse(request, *configuration, *fakeResponse);
    }

    const auto apiKey = ReadEnvironmentVariable("ARTFORGE_OPENAI_API_KEY");
    if (apiKey.empty()) {
        return NotConfiguredOpenAiResult(request.execution, "OpenAI API key environment variable is not set.");
    }

    const auto httpRequest = BuildOpenAiHttpJsonPostRequest(request, *configuration, apiKey);
    return DispatchOpenAiProviderWithResponse(request, *configuration, PostJsonWithWinHttp(httpRequest));
}

AiExecutionResult DispatchAiExecutionRequestNoNetwork(const AiProviderDispatchRequest& request)
{
    if (request.execution.providerKind == AiProviderKind::ManualQueue) {
        return WriteManualAiQueueRequest({request.execution, request.promptText}).providerResult;
    }

    AiExecutionResult result;
    result.requestId = request.execution.requestId;
    result.providerKind = request.execution.providerKind;
    result.locations = request.execution.locations;

    if (request.execution.providerKind == AiProviderKind::Unknown
        || request.execution.providerKind == AiProviderKind::Unsupported) {
        result.status = AiExecutionStatus::UnsupportedProvider;
        result.errors.push_back({"unsupported-provider", "AI provider is unknown or unsupported."});
        return result;
    }

    const auto found = std::find_if(
        request.providerConfigurations.begin(),
        request.providerConfigurations.end(),
        [&](const AiProviderConfiguration& configuration) {
            return configuration.providerKind == request.execution.providerKind;
        });
    if (found == request.providerConfigurations.end() || !found->enabled) {
        result.status = AiExecutionStatus::NotConfigured;
        result.errors.push_back({"not-configured", "API provider is not configured."});
        return result;
    }

    result.status = AiExecutionStatus::NotImplemented;
    result.errors.push_back({"not-implemented", "API provider network execution is not implemented."});
    return result;
}

AiExecutionResult DispatchAiExecutionRequestOptionalNetwork(const AiProviderDispatchRequest& request)
{
    if (request.execution.providerKind == AiProviderKind::OpenAI) {
        return DispatchOpenAiProvider(request, nullptr);
    }
    return DispatchAiExecutionRequestNoNetwork(request);
}

AiExecutionResult DispatchAiExecutionRequestWithFakeHttpResponse(
    const AiProviderDispatchRequest& request,
    const HttpJsonPostResponse& fakeResponse)
{
    if (request.execution.providerKind == AiProviderKind::OpenAI) {
        return DispatchOpenAiProvider(request, &fakeResponse);
    }
    return DispatchAiExecutionRequestNoNetwork(request);
}

std::string DescribeAiExecutionResult(const AiExecutionResult& result)
{
    std::ostringstream output;
    output << "AI provider dispatch: " << ToDisplayName(result.status) << "\n";
    output << "Provider: " << ToDisplayName(result.providerKind) << "\n";
    output << "Request id: " << result.requestId << "\n";
    if (!result.locations.requestPath.empty()) {
        output << "Request file: " << result.locations.requestPath.generic_string() << "\n";
    }
    if (!result.locations.promptTextPath.empty()) {
        output << "Prompt text file: " << result.locations.promptTextPath.generic_string() << "\n";
    }
    if (!result.locations.expectedResultPath.empty()) {
        output << "Expected result file: " << result.locations.expectedResultPath.generic_string() << "\n";
    }
    for (const auto& error : result.errors) {
        output << "Error: " << error.code << ": " << error.message << "\n";
    }
    for (const auto& diagnostic : result.diagnostics) {
        output << "Diagnostic: " << diagnostic << "\n";
    }
    return output.str();
}

}
