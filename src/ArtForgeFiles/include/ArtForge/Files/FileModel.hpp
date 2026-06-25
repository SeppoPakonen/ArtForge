#pragma once

#include <array>
#include <string_view>

namespace ArtForge::Files {

enum class StorageFormat {
    Json,
    JsonLines,
    Markdown
};

struct FileTypeDescriptor {
    std::string_view extension;
    std::string_view purpose;
    StorageFormat storageFormat;
    std::string_view scope;
    bool prefersRelativeReferences;
    bool authoredProjectFile;
};

std::string_view HumanReadableFileModelName();
std::string_view ToDisplayName(StorageFormat storageFormat);

constexpr std::array<FileTypeDescriptor, 9> FileTypeCatalog{{
    {".afsolution", "Solution file", StorageFormat::Json, "solution", true, true},
    {".afartist", "Artist profile file", StorageFormat::Json, "artist", true, true},
    {".afseries", "Project, album, or series file", StorageFormat::Json, "series", true, true},
    {".afwork", "Work item file", StorageFormat::Json, "work item", true, true},
    {".affragment", "Fragment or source file", StorageFormat::Json, "fragment", true, true},
    {".afhistory", "Persistent history event log", StorageFormat::JsonLines, "scope history", true, true},
    {".afhistory.jsonl", "Persistent history event log", StorageFormat::JsonLines, "scope history", true, true},
    {".afrules", "Rule package manifest", StorageFormat::Json, "rule package", true, true},
    {".afprompt", "AI prompt package manifest", StorageFormat::Json, "prompt package", true, true},
}};

constexpr std::string_view GeneratedCacheDirectoryName = ".artforge-cache";
constexpr std::string_view FinnishUiTranslationPath = "locales/ui.fi.json";

}
