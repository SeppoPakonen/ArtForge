#pragma once

#include "ArtForge/Services/ServiceResult.hpp"

#include <filesystem>
#include <string>

namespace ArtForge::Services {

struct SelectedItemPromptCommandRequest {
    std::filesystem::path workPath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{};
    std::string operation;
};

struct SelectedItemPromptCommandResult {
    ServiceCommandResult command;
    std::string promptDebugDump;
};

SelectedItemPromptCommandResult BuildSelectedItemPromptCommand(
    const SelectedItemPromptCommandRequest& request);

}
