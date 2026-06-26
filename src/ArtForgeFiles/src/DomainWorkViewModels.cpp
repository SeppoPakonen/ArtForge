#include "ArtForge/Files/DomainWorkViewModels.hpp"

#include <charconv>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>

namespace ArtForge::Files {

namespace {

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input{path};
    if (!input) {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::optional<std::size_t> FindFieldValue(std::string_view json, std::string_view key)
{
    const std::string field = "\"" + std::string{key} + "\"";
    const auto fieldPosition = json.find(field);
    if (fieldPosition == std::string_view::npos) {
        return std::nullopt;
    }

    const auto colonPosition = json.find(':', fieldPosition + field.size());
    if (colonPosition == std::string_view::npos) {
        return std::nullopt;
    }

    auto valuePosition = colonPosition + 1;
    while (valuePosition < json.size() && std::isspace(static_cast<unsigned char>(json[valuePosition])) != 0) {
        ++valuePosition;
    }

    return valuePosition;
}

std::optional<std::string> ReadStringField(std::string_view json, std::string_view key)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition || *valuePosition >= json.size() || json[*valuePosition] != '"') {
        return std::nullopt;
    }

    std::string value;
    bool escaped = false;
    for (auto index = *valuePosition + 1; index < json.size(); ++index) {
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

    return std::nullopt;
}

std::string EscapeJsonString(std::string_view value)
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

std::optional<std::pair<std::size_t, std::size_t>> FindStringFieldRange(std::string_view json, std::string_view key)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition || *valuePosition >= json.size() || json[*valuePosition] != '"') {
        return std::nullopt;
    }

    bool escaped = false;
    for (auto index = *valuePosition + 1; index < json.size(); ++index) {
        const char character = json[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '"') {
            return std::make_pair(*valuePosition + 1, index);
        }
    }

    return std::nullopt;
}

std::optional<int> ReadIntField(std::string_view json, std::string_view key)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition) {
        return std::nullopt;
    }

    auto endPosition = *valuePosition;
    while (endPosition < json.size() && std::isdigit(static_cast<unsigned char>(json[endPosition])) != 0) {
        ++endPosition;
    }

    int value = 0;
    const auto parseResult = std::from_chars(json.data() + *valuePosition, json.data() + endPosition, value);
    if (parseResult.ec != std::errc{}) {
        return std::nullopt;
    }
    return value;
}

std::optional<std::string_view> ReadContainer(std::string_view json, std::string_view key, char open, char close)
{
    const auto valuePosition = FindFieldValue(json, key);
    if (!valuePosition || *valuePosition >= json.size() || json[*valuePosition] != open) {
        return std::nullopt;
    }

    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (auto index = *valuePosition; index < json.size(); ++index) {
        const char character = json[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }

        if (character == '"') {
            inString = true;
        } else if (character == open) {
            ++depth;
        } else if (character == close) {
            --depth;
            if (depth == 0) {
                return json.substr(*valuePosition, index - *valuePosition + 1);
            }
        }
    }

    return std::nullopt;
}

std::vector<std::string_view> ObjectItems(std::string_view arrayJson)
{
    std::vector<std::string_view> items;
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    std::optional<std::size_t> objectStart;

    for (std::size_t index = 0; index < arrayJson.size(); ++index) {
        const char character = arrayJson[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }

        if (character == '"') {
            inString = true;
        } else if (character == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
        } else if (character == '}') {
            --depth;
            if (depth == 0 && objectStart) {
                items.push_back(arrayJson.substr(*objectStart, index - *objectStart + 1));
                objectStart.reset();
            }
        }
    }

    return items;
}

std::optional<std::pair<std::size_t, std::size_t>> FindObjectByIdRange(std::string_view arrayJson, std::string_view itemId)
{
    bool inString = false;
    bool escaped = false;
    int depth = 0;
    std::optional<std::size_t> objectStart;

    for (std::size_t index = 0; index < arrayJson.size(); ++index) {
        const char character = arrayJson[index];
        if (inString) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inString = false;
            }
            continue;
        }

        if (character == '"') {
            inString = true;
        } else if (character == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
        } else if (character == '}') {
            --depth;
            if (depth == 0 && objectStart) {
                const auto object = arrayJson.substr(*objectStart, index - *objectStart + 1);
                if (ReadStringField(object, "id").value_or("") == itemId) {
                    return std::make_pair(*objectStart, index + 1);
                }
                objectStart.reset();
            }
        }
    }

    return std::nullopt;
}

std::optional<std::string_view> ArrayKeyForUpdate(std::string_view domain, std::string_view itemType)
{
    if (domain == "lyrics" && itemType == "lyric_line") {
        return "lyricLines";
    }
    if (domain == "visualArt" && itemType == "viewer_layer") {
        return "viewerLayers";
    }
    if (domain == "visualArt" && itemType == "paint_layer") {
        return "paintLayers";
    }
    if (domain == "scriptStoryboard" && itemType == "script_block") {
        return "blocks";
    }
    return std::nullopt;
}

bool IsSupportedUpdateField(std::string_view domain, std::string_view itemType, std::string_view field)
{
    if (domain == "lyrics" && itemType == "lyric_line") {
        return field == "text";
    }
    if (domain == "visualArt" && (itemType == "viewer_layer" || itemType == "paint_layer")) {
        return field == "intent";
    }
    if (domain == "scriptStoryboard" && itemType == "script_block") {
        return field == "text";
    }
    return false;
}

ScopeFileIssue MakeIssue(std::string message)
{
    ScopeFileIssue issue;
    issue.message = std::move(message);
    return issue;
}

std::string ReadIntAsString(std::string_view json, std::string_view key)
{
    const auto value = ReadIntField(json, key);
    return value ? std::to_string(*value) : std::string{};
}

DomainWorkMetadata MakeMetadata(const std::filesystem::path& path, const WorkScopeFile& file)
{
    return {path, file.id, file.displayName, file.workKind, file.workDomain};
}

DomainWorkViewStatus MakeStatus(const WorkScopeFileLoadResult& result, std::string_view expectedDomain)
{
    DomainWorkViewStatus status;
    status.ok = result.status.ok;
    status.supported = result.file.workDomain == expectedDomain;
    status.domain = result.file.workDomain;
    status.issues = result.status.issues;
    if (result.status.ok && !status.supported) {
        status.ok = false;
        status.issues.push_back({"unsupported work domain: " + (result.file.workDomain.empty() ? std::string{"(unspecified)"} : result.file.workDomain)});
    }
    return status;
}

std::string EvaluationSummary(std::string_view lineJson)
{
    const auto evaluation = ReadContainer(lineJson, "evaluation", '{', '}');
    if (!evaluation) {
        return {};
    }

    std::vector<std::string> values;
    for (const auto key : {"singability", "clarity", "hookStrength", "rangeFit"}) {
        if (const auto value = ReadStringField(*evaluation, key)) {
            values.push_back(std::string{key} + "=" + *value);
        }
    }

    if (values.empty()) {
        return "present";
    }

    std::ostringstream output;
    for (std::size_t index = 0; index < values.size(); ++index) {
        output << values[index];
        if (index + 1 < values.size()) {
            output << "; ";
        }
    }
    return output.str();
}

}

LyricsWorkViewModel LoadLyricsWorkViewModel(const std::filesystem::path& path)
{
    const auto base = LoadWorkScopeFile(path);
    const auto json = ReadTextFile(path);
    LyricsWorkViewModel model;
    model.status = MakeStatus(base, "lyrics");
    model.metadata = MakeMetadata(path, base.file);

    const auto song = ReadContainer(json, "song", '{', '}');
    if (song) {
        model.title = ReadStringField(*song, "title").value_or("");
        model.tempoBpm = ReadIntAsString(*song, "tempoBpm");
        model.key = ReadStringField(*song, "key").value_or("");
    }

    if (const auto sections = ReadContainer(json, "sections", '[', ']')) {
        for (const auto item : ObjectItems(*sections)) {
            model.sections.push_back({
                ReadStringField(item, "id").value_or(""),
                ReadStringField(item, "label").value_or(""),
                ReadIntField(item, "order").value_or(0),
            });
        }
    }

    if (const auto lines = ReadContainer(json, "lyricLines", '[', ']')) {
        for (const auto item : ObjectItems(*lines)) {
            model.lyricLines.push_back({
                ReadStringField(item, "id").value_or(""),
                ReadStringField(item, "sectionId").value_or(""),
                ReadStringField(item, "timeRange").value_or(""),
                ReadStringField(item, "text").value_or(""),
                EvaluationSummary(item),
            });
        }
    }

    return model;
}

VisualArtWorkViewModel LoadVisualArtWorkViewModel(const std::filesystem::path& path)
{
    const auto base = LoadWorkScopeFile(path);
    const auto json = ReadTextFile(path);
    VisualArtWorkViewModel model;
    model.status = MakeStatus(base, "visualArt");
    model.metadata = MakeMetadata(path, base.file);

    const auto readLayers = [](std::string_view jsonText, std::string_view key, std::string_view layerType) {
        std::vector<VisualArtLayerView> layers;
        if (const auto array = ReadContainer(jsonText, key, '[', ']')) {
            for (const auto item : ObjectItems(*array)) {
                layers.push_back({
                    ReadStringField(item, "id").value_or(""),
                    std::string{layerType},
                    ReadStringField(item, "label").value_or(""),
                    ReadStringField(item, "intent").value_or(""),
                    ReadStringField(item, "priority").value_or(""),
                    ReadStringField(item, "status").value_or(""),
                });
            }
        }
        return layers;
    };

    model.viewerLayers = readLayers(json, "viewerLayers", "viewer");
    model.paintLayers = readLayers(json, "paintLayers", "paint");
    return model;
}

ScriptStoryboardWorkViewModel LoadScriptStoryboardWorkViewModel(const std::filesystem::path& path)
{
    const auto base = LoadWorkScopeFile(path);
    const auto json = ReadTextFile(path);
    ScriptStoryboardWorkViewModel model;
    model.status = MakeStatus(base, "scriptStoryboard");
    model.metadata = MakeMetadata(path, base.file);

    if (const auto scenes = ReadContainer(json, "scenes", '[', ']')) {
        for (const auto item : ObjectItems(*scenes)) {
            model.scenes.push_back({
                ReadStringField(item, "id").value_or(""),
                ReadStringField(item, "title").value_or(""),
                ReadStringField(item, "timeRange").value_or(""),
                ReadIntField(item, "order").value_or(0),
            });
        }
    }

    if (const auto blocks = ReadContainer(json, "blocks", '[', ']')) {
        for (const auto item : ObjectItems(*blocks)) {
            model.blocks.push_back({
                ReadStringField(item, "id").value_or(""),
                ReadStringField(item, "sceneId").value_or(""),
                ReadStringField(item, "kind").value_or(""),
                ReadStringField(item, "timeRange").value_or(""),
                ReadStringField(item, "speaker").value_or(""),
                ReadStringField(item, "voice").value_or(""),
                ReadStringField(item, "text").value_or(""),
            });
        }
    }

    if (const auto storyboard = ReadContainer(json, "storyboard", '[', ']')) {
        for (const auto item : ObjectItems(*storyboard)) {
            model.storyboard.push_back({
                ReadStringField(item, "id").value_or(""),
                ReadStringField(item, "sceneId").value_or(""),
                ReadStringField(item, "visualization").value_or(""),
                ReadStringField(item, "referencePath").value_or(""),
            });
        }
    }

    return model;
}

WorkDomainTextUpdateResult UpdateWorkDomainTextField(const WorkDomainTextUpdateRequest& request)
{
    WorkDomainTextUpdateResult result;
    result.path = request.path;
    result.tempPath = request.path;
    result.tempPath += ".tmp";
    result.replacementText = request.replacementText;

    const auto base = LoadWorkScopeFile(request.path);
    result.status = MakeStatus(base, request.domain);
    if (!result.status.ok) {
        return result;
    }

    if (!IsSupportedUpdateField(request.domain, request.itemType, request.field)) {
        result.status.ok = false;
        result.status.issues.push_back(MakeIssue("unsupported editable field: " + request.domain + "/" + request.itemType + "." + request.field));
        return result;
    }

    auto json = ReadTextFile(request.path);
    if (json.empty()) {
        result.status.ok = false;
        result.status.issues.push_back(MakeIssue("file is empty or unreadable"));
        return result;
    }

    const auto arrayKey = ArrayKeyForUpdate(request.domain, request.itemType);
    if (!arrayKey) {
        result.status.ok = false;
        result.status.issues.push_back(MakeIssue("unsupported edit target"));
        return result;
    }

    const auto arrayValuePosition = FindFieldValue(json, *arrayKey);
    const auto array = ReadContainer(json, *arrayKey, '[', ']');
    if (!arrayValuePosition || !array) {
        result.status.ok = false;
        result.status.issues.push_back(MakeIssue("target array not found: " + std::string{*arrayKey}));
        return result;
    }

    const auto objectRangeInArray = FindObjectByIdRange(*array, request.itemId);
    if (!objectRangeInArray) {
        result.status.ok = false;
        result.status.issues.push_back(MakeIssue("target item not found: " + request.itemId));
        return result;
    }

    const auto objectStart = *arrayValuePosition + objectRangeInArray->first;
    const auto objectEnd = *arrayValuePosition + objectRangeInArray->second;
    const auto objectJson = std::string_view{json}.substr(objectStart, objectEnd - objectStart);
    const auto fieldRangeInObject = FindStringFieldRange(objectJson, request.field);
    if (!fieldRangeInObject) {
        result.status.ok = false;
        result.status.issues.push_back(MakeIssue("target field not found or not a string: " + request.field));
        return result;
    }

    result.previousText = ReadStringField(objectJson, request.field).value_or("");
    const auto fieldStart = objectStart + fieldRangeInObject->first;
    const auto fieldEnd = objectStart + fieldRangeInObject->second;
    json.replace(fieldStart, fieldEnd - fieldStart, EscapeJsonString(request.replacementText));

    {
        std::ofstream output{result.tempPath, std::ios::trunc};
        if (!output) {
            result.status.ok = false;
            result.status.issues.push_back(MakeIssue("could not open temp file for writing"));
            return result;
        }
        output << json;
        if (!json.empty() && json.back() != '\n') {
            output << "\n";
        }
    }

    const auto tempLoad = LoadWorkScopeFile(result.tempPath);
    if (!tempLoad.status.ok) {
        std::error_code removeError;
        std::filesystem::remove(result.tempPath, removeError);
        result.status.ok = false;
        result.status.issues.push_back(MakeIssue("updated temp file failed reload validation"));
        return result;
    }

    std::error_code replaceError;
    std::filesystem::rename(result.tempPath, request.path, replaceError);
    if (replaceError) {
        std::filesystem::copy_file(result.tempPath, request.path, std::filesystem::copy_options::overwrite_existing, replaceError);
        std::error_code removeError;
        std::filesystem::remove(result.tempPath, removeError);
    }
    if (replaceError) {
        result.status.ok = false;
        result.status.issues.push_back(MakeIssue("could not replace target file: " + replaceError.message()));
        return result;
    }

    const auto reload = LoadWorkScopeFile(request.path);
    result.status = MakeStatus(reload, request.domain);
    return result;
}

std::string DescribeDomainWorkViewModel(const std::filesystem::path& path)
{
    const auto work = LoadWorkScopeFile(path);
    std::ostringstream output;
    output << "View model status: ";
    if (!work.status.ok) {
        output << "failed\n";
        return output.str();
    }

    if (work.file.workDomain == "lyrics") {
        const auto model = LoadLyricsWorkViewModel(path);
        output << (model.status.ok ? "OK" : "failed") << "\n";
        output << "Sections: " << model.sections.size() << "\n";
        output << "Lyric lines: " << model.lyricLines.size() << "\n";
    } else if (work.file.workDomain == "visualArt") {
        const auto model = LoadVisualArtWorkViewModel(path);
        output << (model.status.ok ? "OK" : "failed") << "\n";
        output << "Viewer layers: " << model.viewerLayers.size() << "\n";
        output << "Paint layers: " << model.paintLayers.size() << "\n";
    } else if (work.file.workDomain == "scriptStoryboard") {
        const auto model = LoadScriptStoryboardWorkViewModel(path);
        output << (model.status.ok ? "OK" : "failed") << "\n";
        output << "Scenes: " << model.scenes.size() << "\n";
        output << "Blocks: " << model.blocks.size() << "\n";
        output << "Storyboard placeholders: " << model.storyboard.size() << "\n";
    } else {
        output << "unsupported\n";
        output << "Unsupported domain: " << (work.file.workDomain.empty() ? "(unspecified)" : work.file.workDomain) << "\n";
    }

    return output.str();
}

}
