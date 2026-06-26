#pragma once

#include "ArtForge/Presentation/PresentationModels.hpp"

#include <filesystem>

namespace ArtForge::Presentation {

struct WorkAppPresentationModel {
    StatusModel status;
    NavigationTreeModel navigation;
    TableModel domainTable;
    PropertyListModel properties;
};

WorkAppPresentationModel BuildWorkAppPresentationModel(const std::filesystem::path& workPath);

}
