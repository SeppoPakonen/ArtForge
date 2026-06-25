#include "ArtForge/Files/FileModel.hpp"

namespace ArtForge::Files {

std::string_view HumanReadableFileModelName()
{
    return "ArtForge human-readable file model";
}

std::string_view ToDisplayName(StorageFormat storageFormat)
{
    switch (storageFormat) {
    case StorageFormat::Json:
        return "JSON";
    case StorageFormat::JsonLines:
        return "JSON Lines";
    case StorageFormat::Markdown:
        return "Markdown";
    }

    return "Unknown";
}

}
