#include "ArtForge/Files/DomainWorkViewModels.hpp"

#include <charconv>
#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

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
