#pragma once

#include <array>
#include <string_view>

namespace ArtForge::Deps {

enum class PressureConcept {
    Package,
    Flag,
    Slot,
    Version,
    Dependency,
    Blocker,
    CircularDependency,
    WorldSet
};

enum class PackageKind {
    CreativeRequirement,
    Work,
    RulePack,
    Fragment,
    ProjectFunction
};

enum class DiagnosticCategory {
    MissingDependency,
    MissingSlot,
    Blocker,
    FlagConflict,
    CircularDependency,
    VersionMismatch,
    UnresolvedPressure
};

enum class WorldUpdateItemKind {
    PackageNeedsUpdate,
    MissingSlot,
    UnresolvedConflict,
    FragmentNotImported,
    TaskRequiredBeforeVersionAccepted,
    WorkBlockedByProjectDecision
};

struct PressureExample {
    std::string_view name;
    std::string_view description;
};

struct FlagExample {
    std::string_view packageName;
    std::array<std::string_view, 4> flags;
};

std::string_view PressureModelName();
std::string_view ToDisplayName(PressureConcept pressureConcept);
std::string_view ToDescription(PressureConcept pressureConcept);
std::string_view ToDisplayName(PackageKind kind);
std::string_view ToDisplayName(DiagnosticCategory category);
std::string_view ToDisplayName(WorldUpdateItemKind itemKind);

FlagExample LyricsLowFrictionPolicyFlagExample();

constexpr std::array<PressureConcept, 8> CorePressureConcepts{{
    PressureConcept::Package,
    PressureConcept::Flag,
    PressureConcept::Slot,
    PressureConcept::Version,
    PressureConcept::Dependency,
    PressureConcept::Blocker,
    PressureConcept::CircularDependency,
    PressureConcept::WorldSet,
}};

constexpr std::array<DiagnosticCategory, 7> DiagnosticCategories{{
    DiagnosticCategory::MissingDependency,
    DiagnosticCategory::MissingSlot,
    DiagnosticCategory::Blocker,
    DiagnosticCategory::FlagConflict,
    DiagnosticCategory::CircularDependency,
    DiagnosticCategory::VersionMismatch,
    DiagnosticCategory::UnresolvedPressure,
}};

constexpr std::array<WorldUpdateItemKind, 6> FutureWorldUpdateViewItems{{
    WorldUpdateItemKind::PackageNeedsUpdate,
    WorldUpdateItemKind::MissingSlot,
    WorldUpdateItemKind::UnresolvedConflict,
    WorldUpdateItemKind::FragmentNotImported,
    WorldUpdateItemKind::TaskRequiredBeforeVersionAccepted,
    WorldUpdateItemKind::WorkBlockedByProjectDecision,
}};

constexpr std::array<PressureExample, 3> CreativePressureExamples{{
    {
        "live solo performance requirements",
        "A work may require singable range, portable arrangement, minimal scene changes, and no unavailable collaborators.",
    },
    {
        "low-cost music video requirements",
        "A video concept may require few locations, no licensed props, daylight shooting, and edit-friendly continuity.",
    },
    {
        "project or album story-arc requirements",
        "A series may require opening contrast, midpoint escalation, closing resolution, and compatible work ordering.",
    },
}};

}
