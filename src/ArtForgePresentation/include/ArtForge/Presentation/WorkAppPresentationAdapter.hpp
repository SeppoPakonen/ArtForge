#pragma once

#include "ArtForge/Presentation/PresentationModels.hpp"

#include <filesystem>

namespace ArtForge::Presentation {

struct WorkAppPresentationModel {
    StatusModel status;
    NavigationTreeModel navigation;
    TableModel domainTable;
    PropertyListModel properties;
    SelectionModel selection;
    PromptPreviewModel promptPreview;
    ManualAiQueueModel manualAiQueue;
    DirtyStateModel dirtyState;
};

WorkAppPresentationModel BuildWorkAppPresentationModel(const std::filesystem::path& workPath);
WorkAppPresentationModel BuildWorkAppPresentationModel(
    const std::filesystem::path& workPath,
    const SelectionModel& selection);
SelectionModel SelectWorkAppTableRow(const std::filesystem::path& workPath, int rowIndex);
SelectionModel ClearWorkAppSelection(const std::filesystem::path& workPath);

}
