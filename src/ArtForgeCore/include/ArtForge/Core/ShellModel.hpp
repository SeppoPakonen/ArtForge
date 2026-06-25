#pragma once

#include "ArtForge/Core/ProcessModel.hpp"

#include <array>
#include <span>
#include <string_view>

namespace ArtForge::Core {

enum class ShellNavigationConcept {
    Artists,
    Projects,
    MissingFiles,
    WorldUpdateView,
    Conflicts,
    Tasks,
    IdentityMap,
    RecurringThemes,
    Constraints,
    AudienceAssumptions,
    OrderedWorks,
    ArcPressure,
    MissingSlots,
    Requirements,
    TechnicalBase,
    Sources,
    EditorArea,
    PressureMap,
    RuleAnalytics,
    RepairOptions,
    HistoryTimeline
};

struct ScopeShellDescriptor {
    ScopeKind scope;
    std::wstring_view applicationName;
    std::wstring_view expectedScope;
    std::string_view titleLocalizationKey;
};

ScopeShellDescriptor SolutionShellDescriptor() noexcept;
ScopeShellDescriptor ArtistShellDescriptor() noexcept;
ScopeShellDescriptor SeriesShellDescriptor() noexcept;
ScopeShellDescriptor WorkShellDescriptor() noexcept;

std::span<const ShellNavigationConcept> NavigationConceptsForScope(ScopeKind scope) noexcept;
std::wstring_view ToDisplayName(ShellNavigationConcept navigationConcept) noexcept;

constexpr std::array<ScopeKind, 4> IndependentlyLaunchableShellScopes{{
    ScopeKind::Solution,
    ScopeKind::Artist,
    ScopeKind::Series,
    ScopeKind::WorkItem,
}};

}
