#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

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

enum class DependencyRelation {
    Requires,
    Recommends,
    ConflictsWith
};

enum class DiagnosticSeverity {
    Info,
    Warning,
    Error
};

struct PressurePackageId {
    std::string value;
};

struct PackageVersion {
    std::string value;
};

struct PressureFlag {
    std::string name;
    bool enabled{};
};

struct PressureSlot {
    std::string name;
    std::string requiredRole;
};

struct PressureSlotRequirement {
    std::string slotName;
    std::string reason;
};

struct PressureDependency {
    PressurePackageId packageId;
    DependencyRelation relation{};
    std::string reason;
};

struct PressureBlocker {
    PressurePackageId packageId;
    std::string reason;
};

struct PressureFlagConflict {
    std::string firstFlag;
    std::string secondFlag;
    std::string reason;
};

struct PressureDiagnostic {
    DiagnosticSeverity severity{};
    DiagnosticCategory category{};
    PressurePackageId packageId;
    std::string message;
};

struct WorldUpdateSummary {
    std::size_t packageCount{};
    std::size_t infoCount{};
    std::size_t warningCount{};
    std::size_t errorCount{};
    std::vector<PressureDiagnostic> diagnostics;
    std::vector<PressureDiagnostic> missingDependencies;
    std::vector<PressureDiagnostic> blockers;
    std::vector<PressureDiagnostic> flagConflicts;
    std::vector<PressureDiagnostic> missingSlots;
    std::vector<PressureDiagnostic> circularDependencies;
    std::vector<std::string> packagesNeedingAttention;
};

struct PressurePackage {
    PressurePackageId id;
    PackageKind kind{};
    PackageVersion version;
    std::vector<PressureFlag> flags;
    std::vector<PressureSlot> slots;
    std::vector<PressureSlotRequirement> requiredSlots;
    std::vector<PressureDependency> dependencies;
    std::vector<PressureBlocker> blockers;
    std::vector<PressureFlagConflict> flagConflicts;
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
std::string_view ToDisplayName(DependencyRelation relation);
std::string_view ToDisplayName(DiagnosticCategory category);
std::string_view ToDisplayName(DiagnosticSeverity severity);
std::string_view ToDisplayName(WorldUpdateItemKind itemKind);
std::string FormatDiagnosticMessage(const PressureDiagnostic& diagnostic);
std::vector<PressureDiagnostic> EvaluatePressureDiagnostics(const std::vector<PressurePackage>& packages);
std::vector<std::string> SamplePressureDiagnosticOutput();
WorldUpdateSummary BuildWorldUpdateSummary(const std::vector<PressurePackage>& packages);
std::string FormatWorldUpdateSummary(const WorldUpdateSummary& summary);
std::string SampleWorldUpdateSummaryOutput();

FlagExample LyricsLowFrictionPolicyFlagExample();
PressurePackage LiveSoloPerformanceRequirementExample();
PressurePackage LowCostMusicVideoRequirementExample();
PressurePackage AlbumArcPressureRequirementExample();

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
