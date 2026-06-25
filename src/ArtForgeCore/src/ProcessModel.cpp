#include "ArtForge/Core/ProcessModel.hpp"

namespace ArtForge::Core {

std::wstring_view ToDisplayName(ScopeKind scope) noexcept
{
    switch (scope) {
    case ScopeKind::Solution:
        return L"Solution";
    case ScopeKind::Artist:
        return L"Artist";
    case ScopeKind::Series:
        return L"Series / Album / Project";
    case ScopeKind::WorkItem:
        return L"WorkItem / Song / Image / Scene";
    case ScopeKind::Fragment:
        return L"Fragment / Source / Line / Layer";
    }

    return L"Unknown";
}

std::wstring_view ToFileArgumentName(ScopeKind scope) noexcept
{
    switch (scope) {
    case ScopeKind::Solution:
        return L"solution-file";
    case ScopeKind::Artist:
        return L"artist-file";
    case ScopeKind::Series:
        return L"series-file";
    case ScopeKind::WorkItem:
        return L"work-file";
    case ScopeKind::Fragment:
        return L"fragment-file";
    }

    return L"file";
}

bool IsIndependentlyLaunchable(ScopeKind scope) noexcept
{
    switch (scope) {
    case ScopeKind::Solution:
    case ScopeKind::Artist:
    case ScopeKind::Series:
    case ScopeKind::WorkItem:
        return true;
    case ScopeKind::Fragment:
        return false;
    }

    return false;
}

std::wstring_view ToMessageName(ProcessEventKind eventKind) noexcept
{
    switch (eventKind) {
    case ProcessEventKind::FileChanged:
        return L"file changed";
    case ProcessEventKind::SnapshotCreated:
        return L"snapshot created";
    case ProcessEventKind::DependencyGraphInvalidated:
        return L"dependency graph invalidated";
    case ProcessEventKind::RulePackageChanged:
        return L"rule package changed";
    case ProcessEventKind::PromptPackageGenerated:
        return L"prompt package generated";
    case ProcessEventKind::AiResultImported:
        return L"AI result imported";
    case ProcessEventKind::ConflictDetected:
        return L"conflict detected";
    case ProcessEventKind::StatusChanged:
        return L"status changed";
    }

    return L"unknown";
}

std::wstring_view ToEventPurpose(ProcessEventKind eventKind) noexcept
{
    switch (eventKind) {
    case ProcessEventKind::FileChanged:
        return L"Notify related scopes that a human-readable project file changed.";
    case ProcessEventKind::SnapshotCreated:
        return L"Notify related scopes that a persistent history snapshot is available.";
    case ProcessEventKind::DependencyGraphInvalidated:
        return L"Request dependency and conflict analysis refresh.";
    case ProcessEventKind::RulePackageChanged:
        return L"Notify open scopes that constraints or rule packages changed.";
    case ProcessEventKind::PromptPackageGenerated:
        return L"Announce that an AI prompt package was produced for review.";
    case ProcessEventKind::AiResultImported:
        return L"Announce that an AI result was imported into project data.";
    case ProcessEventKind::ConflictDetected:
        return L"Report a dependency, rule, or creative-pressure conflict.";
    case ProcessEventKind::StatusChanged:
        return L"Publish lightweight progress or diagnostic status.";
    }

    return L"No purpose is registered.";
}

std::wstring_view ProcessModelStatus() noexcept
{
    return L"independent scope apps with shared future event boundary";
}

std::size_t SupportedProcessEventCount() noexcept
{
    return 8;
}

}
