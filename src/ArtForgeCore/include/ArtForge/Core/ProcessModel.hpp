#pragma once

#include <cstddef>
#include <string_view>

namespace ArtForge::Core {

enum class ScopeKind {
    Solution,
    Artist,
    Series,
    WorkItem,
    Fragment
};

enum class ProcessEventKind {
    FileChanged,
    SnapshotCreated,
    DependencyGraphInvalidated,
    RulePackageChanged,
    PromptPackageGenerated,
    AiResultImported,
    ConflictDetected,
    StatusChanged
};

std::wstring_view ToDisplayName(ScopeKind scope) noexcept;
std::wstring_view ToFileArgumentName(ScopeKind scope) noexcept;
bool IsIndependentlyLaunchable(ScopeKind scope) noexcept;

std::wstring_view ToMessageName(ProcessEventKind eventKind) noexcept;
std::wstring_view ToEventPurpose(ProcessEventKind eventKind) noexcept;

std::wstring_view ProcessModelStatus() noexcept;
std::size_t SupportedProcessEventCount() noexcept;

}
