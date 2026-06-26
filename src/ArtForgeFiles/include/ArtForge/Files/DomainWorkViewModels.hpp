#pragma once

#include "ArtForge/Files/ScopeFiles.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ArtForge::Files {

struct DomainWorkViewStatus {
    bool ok{};
    bool supported{};
    std::string domain;
    std::vector<ScopeFileIssue> issues;
};

struct DomainWorkMetadata {
    std::filesystem::path path;
    std::string id;
    std::string displayName;
    std::string workKind;
    std::string workDomain;
};

struct LyricSectionView {
    std::string id;
    std::string label;
    int order{};
};

struct LyricLineView {
    std::string id;
    std::string sectionId;
    std::string timeRange;
    std::string text;
    std::string evaluationSummary;
};

struct LyricsWorkViewModel {
    DomainWorkViewStatus status;
    DomainWorkMetadata metadata;
    std::string title;
    std::string tempoBpm;
    std::string key;
    std::vector<LyricSectionView> sections;
    std::vector<LyricLineView> lyricLines;
};

struct VisualArtLayerView {
    std::string id;
    std::string layerType;
    std::string label;
    std::string intent;
    std::string priority;
    std::string status;
};

struct VisualArtWorkViewModel {
    DomainWorkViewStatus status;
    DomainWorkMetadata metadata;
    std::vector<VisualArtLayerView> viewerLayers;
    std::vector<VisualArtLayerView> paintLayers;
};

struct ScriptSceneView {
    std::string id;
    std::string title;
    std::string timeRange;
    int order{};
};

struct ScriptBlockView {
    std::string id;
    std::string sceneId;
    std::string kind;
    std::string timeRange;
    std::string speaker;
    std::string voice;
    std::string text;
};

struct StoryboardPlaceholderView {
    std::string id;
    std::string sceneId;
    std::string visualization;
    std::string referencePath;
};

struct ScriptStoryboardWorkViewModel {
    DomainWorkViewStatus status;
    DomainWorkMetadata metadata;
    std::vector<ScriptSceneView> scenes;
    std::vector<ScriptBlockView> blocks;
    std::vector<StoryboardPlaceholderView> storyboard;
};

struct WorkDomainTextUpdateRequest {
    std::filesystem::path path;
    std::string domain;
    std::string itemType;
    std::string itemId;
    std::string field;
    std::string replacementText;
};

struct WorkDomainTextUpdateResult {
    DomainWorkViewStatus status;
    std::filesystem::path path;
    std::filesystem::path tempPath;
    std::string previousText;
    std::string replacementText;
};

LyricsWorkViewModel LoadLyricsWorkViewModel(const std::filesystem::path& path);
VisualArtWorkViewModel LoadVisualArtWorkViewModel(const std::filesystem::path& path);
ScriptStoryboardWorkViewModel LoadScriptStoryboardWorkViewModel(const std::filesystem::path& path);

WorkDomainTextUpdateResult UpdateWorkDomainTextField(const WorkDomainTextUpdateRequest& request);

std::string DescribeDomainWorkViewModel(const std::filesystem::path& path);

}
