#include "ArtForge/Deps/PressureModel.hpp"

#include <string>

namespace ArtForge::Deps {

std::string_view PressureModelName()
{
    return "ArtForge Portage-inspired pressure model";
}

std::string_view ToDisplayName(PressureConcept pressureConcept)
{
    switch (pressureConcept) {
    case PressureConcept::Package:
        return "package";
    case PressureConcept::Flag:
        return "flag";
    case PressureConcept::Slot:
        return "slot";
    case PressureConcept::Version:
        return "version";
    case PressureConcept::Dependency:
        return "dependency";
    case PressureConcept::Blocker:
        return "blocker";
    case PressureConcept::CircularDependency:
        return "circular dependency";
    case PressureConcept::WorldSet:
        return "world set";
    }

    return "unknown pressure concept";
}

std::string_view ToDescription(PressureConcept pressureConcept)
{
    switch (pressureConcept) {
    case PressureConcept::Package:
        return "selected creative requirement, work, rule pack, fragment, or project function";
    case PressureConcept::Flag:
        return "user choice that changes package behavior or requirements";
    case PressureConcept::Slot:
        return "role that may need one or more compatible installed items";
    case PressureConcept::Version:
        return "specific revision of a package or creative requirement";
    case PressureConcept::Dependency:
        return "item required by another item";
    case PressureConcept::Blocker:
        return "item that prevents another item from being valid";
    case PressureConcept::CircularDependency:
        return "requirement cycle needing a changed decision";
    case PressureConcept::WorldSet:
        return "selected creative goals and installed requirements";
    }

    return "unknown pressure concept";
}

std::string_view ToDisplayName(PackageKind kind)
{
    switch (kind) {
    case PackageKind::CreativeRequirement:
        return "creative requirement";
    case PackageKind::Work:
        return "work";
    case PackageKind::RulePack:
        return "rule pack";
    case PackageKind::Fragment:
        return "fragment";
    case PackageKind::ProjectFunction:
        return "project function";
    }

    return "unknown package kind";
}

std::string_view ToDisplayName(DependencyRelation relation)
{
    switch (relation) {
    case DependencyRelation::Requires:
        return "requires";
    case DependencyRelation::Recommends:
        return "recommends";
    case DependencyRelation::ConflictsWith:
        return "conflicts with";
    }

    return "unknown dependency relation";
}

std::string_view ToDisplayName(DiagnosticCategory category)
{
    switch (category) {
    case DiagnosticCategory::MissingDependency:
        return "missing dependency";
    case DiagnosticCategory::MissingSlot:
        return "missing slot";
    case DiagnosticCategory::Blocker:
        return "blocker";
    case DiagnosticCategory::FlagConflict:
        return "flag conflict";
    case DiagnosticCategory::CircularDependency:
        return "circular dependency";
    case DiagnosticCategory::VersionMismatch:
        return "version mismatch";
    case DiagnosticCategory::UnresolvedPressure:
        return "unresolved pressure";
    }

    return "unknown diagnostic category";
}

std::string_view ToDisplayName(DiagnosticSeverity severity)
{
    switch (severity) {
    case DiagnosticSeverity::Info:
        return "info";
    case DiagnosticSeverity::Warning:
        return "warning";
    case DiagnosticSeverity::Error:
        return "error";
    }

    return "unknown diagnostic severity";
}

std::string_view ToDisplayName(WorldUpdateItemKind itemKind)
{
    switch (itemKind) {
    case WorldUpdateItemKind::PackageNeedsUpdate:
        return "package needs update";
    case WorldUpdateItemKind::MissingSlot:
        return "missing slot";
    case WorldUpdateItemKind::UnresolvedConflict:
        return "unresolved conflict";
    case WorldUpdateItemKind::FragmentNotImported:
        return "fragment not imported";
    case WorldUpdateItemKind::TaskRequiredBeforeVersionAccepted:
        return "task required before version can be accepted";
    case WorldUpdateItemKind::WorkBlockedByProjectDecision:
        return "work blocked by project-level decision";
    }

    return "unknown world update item";
}

std::string FormatDiagnosticMessage(const PressureDiagnostic& diagnostic)
{
    std::string formatted;
    formatted += "[";
    formatted += ToDisplayName(diagnostic.severity);
    formatted += "] ";
    formatted += ToDisplayName(diagnostic.category);

    if (!diagnostic.packageId.value.empty()) {
        formatted += " for ";
        formatted += diagnostic.packageId.value;
    }

    if (!diagnostic.message.empty()) {
        formatted += ": ";
        formatted += diagnostic.message;
    }

    return formatted;
}

FlagExample LyricsLowFrictionPolicyFlagExample()
{
    return {
        "lyrics-low-friction-policy",
        {{
            "non_native_listener",
            "radio_target",
            "2010_language",
            "male_performer",
        }},
    };
}

PressurePackage LiveSoloPerformanceRequirementExample()
{
    return {
        {"live-solo-performance"},
        PackageKind::CreativeRequirement,
        {"1.0"},
        {
            {"solo_performer", true},
            {"portable_arrangement", true},
            {"wide_vocal_range", false},
        },
        {
            {"performance_context", "live solo set"},
            {"arrangement", "portable arrangement"},
        },
        {
            {{"singable-range"}, DependencyRelation::Requires, "melody must stay within the artist's reliable live range"},
            {{"portable-instrumentation"}, DependencyRelation::Requires, "arrangement must work without unavailable collaborators"},
        },
        {
            {{"studio-only-layering"}, "dense layered parts would block a solo live version"},
        },
    };
}

PressurePackage LowCostMusicVideoRequirementExample()
{
    return {
        {"low-cost-music-video"},
        PackageKind::ProjectFunction,
        {"1.0"},
        {
            {"few_locations", true},
            {"daylight_shooting", true},
            {"licensed_props", false},
        },
        {
            {"production_budget", "low-cost shoot"},
            {"continuity", "edit-friendly continuity"},
        },
        {
            {{"few-location-plan"}, DependencyRelation::Requires, "concept must fit a small number of locations"},
            {{"daylight-availability"}, DependencyRelation::Requires, "shooting plan assumes available daylight"},
        },
        {
            {{"licensed-prop-dependency"}, "licensed or rented props conflict with the low-cost constraint"},
        },
    };
}

PressurePackage AlbumArcPressureRequirementExample()
{
    return {
        {"album-arc-pressure"},
        PackageKind::CreativeRequirement,
        {"1.0"},
        {
            {"opening_contrast", true},
            {"midpoint_escalation", true},
            {"closing_resolution", true},
        },
        {
            {"series_arc", "album or series narrative pressure"},
            {"work_order", "compatible ordered works"},
        },
        {
            {{"opening-contrast"}, DependencyRelation::Requires, "first work must establish contrast for the arc"},
            {{"midpoint-escalation"}, DependencyRelation::Requires, "middle works must raise pressure"},
            {{"closing-resolution"}, DependencyRelation::Requires, "final work must resolve selected pressures"},
        },
        {
            {{"flat-track-order"}, "an order without contrast blocks the intended arc"},
        },
    };
}

}
