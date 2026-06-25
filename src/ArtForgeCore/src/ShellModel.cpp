#include "ArtForge/Core/ShellModel.hpp"

namespace ArtForge::Core {

namespace {

constexpr std::array<ShellNavigationConcept, 6> SolutionNavigation{{
    ShellNavigationConcept::Artists,
    ShellNavigationConcept::Projects,
    ShellNavigationConcept::MissingFiles,
    ShellNavigationConcept::WorldUpdateView,
    ShellNavigationConcept::Conflicts,
    ShellNavigationConcept::Tasks,
}};

constexpr std::array<ShellNavigationConcept, 5> ArtistNavigation{{
    ShellNavigationConcept::IdentityMap,
    ShellNavigationConcept::RecurringThemes,
    ShellNavigationConcept::Constraints,
    ShellNavigationConcept::AudienceAssumptions,
    ShellNavigationConcept::Projects,
}};

constexpr std::array<ShellNavigationConcept, 5> SeriesNavigation{{
    ShellNavigationConcept::OrderedWorks,
    ShellNavigationConcept::ArcPressure,
    ShellNavigationConcept::MissingSlots,
    ShellNavigationConcept::Requirements,
    ShellNavigationConcept::Conflicts,
}};

constexpr std::array<ShellNavigationConcept, 7> WorkNavigation{{
    ShellNavigationConcept::TechnicalBase,
    ShellNavigationConcept::Sources,
    ShellNavigationConcept::EditorArea,
    ShellNavigationConcept::PressureMap,
    ShellNavigationConcept::RuleAnalytics,
    ShellNavigationConcept::RepairOptions,
    ShellNavigationConcept::HistoryTimeline,
}};

}

ScopeShellDescriptor SolutionShellDescriptor() noexcept
{
    return {
        ScopeKind::Solution,
        L"ArtForgeSolutionApp",
        L"solution",
        "app.solution.title",
    };
}

ScopeShellDescriptor ArtistShellDescriptor() noexcept
{
    return {
        ScopeKind::Artist,
        L"ArtForgeArtistApp",
        L"artist",
        "app.artist.title",
    };
}

ScopeShellDescriptor SeriesShellDescriptor() noexcept
{
    return {
        ScopeKind::Series,
        L"ArtForgeSeriesApp",
        L"project/album/series",
        "app.series.title",
    };
}

ScopeShellDescriptor WorkShellDescriptor() noexcept
{
    return {
        ScopeKind::WorkItem,
        L"ArtForgeWorkApp",
        L"work item",
        "app.work.title",
    };
}

std::span<const ShellNavigationConcept> NavigationConceptsForScope(ScopeKind scope) noexcept
{
    switch (scope) {
    case ScopeKind::Solution:
        return SolutionNavigation;
    case ScopeKind::Artist:
        return ArtistNavigation;
    case ScopeKind::Series:
        return SeriesNavigation;
    case ScopeKind::WorkItem:
        return WorkNavigation;
    case ScopeKind::Fragment:
        return {};
    }

    return {};
}

std::wstring_view ToDisplayName(ShellNavigationConcept navigationConcept) noexcept
{
    switch (navigationConcept) {
    case ShellNavigationConcept::Artists:
        return L"artists";
    case ShellNavigationConcept::Projects:
        return L"projects";
    case ShellNavigationConcept::MissingFiles:
        return L"missing files";
    case ShellNavigationConcept::WorldUpdateView:
        return L"world update view";
    case ShellNavigationConcept::Conflicts:
        return L"conflicts";
    case ShellNavigationConcept::Tasks:
        return L"tasks";
    case ShellNavigationConcept::IdentityMap:
        return L"identity map";
    case ShellNavigationConcept::RecurringThemes:
        return L"recurring themes";
    case ShellNavigationConcept::Constraints:
        return L"constraints";
    case ShellNavigationConcept::AudienceAssumptions:
        return L"audience assumptions";
    case ShellNavigationConcept::OrderedWorks:
        return L"ordered works";
    case ShellNavigationConcept::ArcPressure:
        return L"arc pressure";
    case ShellNavigationConcept::MissingSlots:
        return L"missing slots";
    case ShellNavigationConcept::Requirements:
        return L"requirements";
    case ShellNavigationConcept::TechnicalBase:
        return L"technical base";
    case ShellNavigationConcept::Sources:
        return L"sources";
    case ShellNavigationConcept::EditorArea:
        return L"editor area";
    case ShellNavigationConcept::PressureMap:
        return L"pressure map";
    case ShellNavigationConcept::RuleAnalytics:
        return L"rule analytics";
    case ShellNavigationConcept::RepairOptions:
        return L"repair options";
    case ShellNavigationConcept::HistoryTimeline:
        return L"history timeline";
    }

    return L"unknown shell navigation concept";
}

}
