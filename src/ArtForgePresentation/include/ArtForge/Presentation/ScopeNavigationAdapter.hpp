#pragma once

#include "ArtForge/Core/ShellModel.hpp"
#include "ArtForge/Presentation/PresentationModels.hpp"

#include <filesystem>
#include <string>

namespace ArtForge::Presentation {

NavigationTreeModel BuildScopeNavigationModel(
    ArtForge::Core::ScopeKind scope,
    const std::filesystem::path& openedPath,
    const std::string& loadStatus,
    const std::string& loadDetail);

}
