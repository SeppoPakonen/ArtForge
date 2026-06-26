#include "ArtForge/Deps/PressureModel.hpp"

#include <algorithm>
#include <set>
#include <sstream>
#include <string>

namespace ArtForge::Deps {

namespace {

bool ContainsPackageId(const std::vector<PressurePackage>& packages, const PressurePackageId& packageId)
{
    return std::any_of(packages.begin(), packages.end(), [&](const PressurePackage& package) {
        return package.id.value == packageId.value;
    });
}

bool HasEnabledFlag(const PressurePackage& package, std::string_view flagName)
{
    return std::any_of(package.flags.begin(), package.flags.end(), [&](const PressureFlag& flag) {
        return flag.name == flagName && flag.enabled;
    });
}

std::set<std::string> ProvidedSlotNames(const std::vector<PressurePackage>& packages)
{
    std::set<std::string> names;
    for (const auto& package : packages) {
        for (const auto& slot : package.slots) {
            names.insert(slot.name);
        }
    }
    return names;
}

bool HasDirectRequiresCycle(const PressurePackage& package, const PressurePackage& dependencyPackage)
{
    return std::any_of(package.dependencies.begin(), package.dependencies.end(), [&](const PressureDependency& dependency) {
        return dependency.relation == DependencyRelation::Requires && dependency.packageId.value == dependencyPackage.id.value;
    }) && std::any_of(dependencyPackage.dependencies.begin(), dependencyPackage.dependencies.end(), [&](const PressureDependency& dependency) {
        return dependency.relation == DependencyRelation::Requires && dependency.packageId.value == package.id.value;
    });
}

void AddDiagnostic(
    std::vector<PressureDiagnostic>& diagnostics,
    DiagnosticSeverity severity,
    DiagnosticCategory category,
    const PressurePackageId& packageId,
    std::string message)
{
    diagnostics.push_back({
        severity,
        category,
        packageId,
        std::move(message),
    });
}

void AddGroupedDiagnostic(WorldUpdateSummary& summary, const PressureDiagnostic& diagnostic)
{
    switch (diagnostic.category) {
    case DiagnosticCategory::MissingDependency:
        summary.missingDependencies.push_back(diagnostic);
        break;
    case DiagnosticCategory::MissingSlot:
        summary.missingSlots.push_back(diagnostic);
        break;
    case DiagnosticCategory::Blocker:
        summary.blockers.push_back(diagnostic);
        break;
    case DiagnosticCategory::FlagConflict:
        summary.flagConflicts.push_back(diagnostic);
        break;
    case DiagnosticCategory::CircularDependency:
        summary.circularDependencies.push_back(diagnostic);
        break;
    case DiagnosticCategory::VersionMismatch:
    case DiagnosticCategory::UnresolvedPressure:
        break;
    }
}

void AppendDiagnosticGroup(
    std::ostringstream& output,
    std::string_view title,
    const std::vector<PressureDiagnostic>& diagnostics)
{
    output << "\n" << title << " (" << diagnostics.size() << ")\n";
    if (diagnostics.empty()) {
        output << "- none\n";
        return;
    }

    for (const auto& diagnostic : diagnostics) {
        output << "- " << FormatDiagnosticMessage(diagnostic) << "\n";
    }
}

std::vector<PressurePackage> SampleWorldUpdatePackages()
{
    PressurePackage liveRequirement = LiveSoloPerformanceRequirementExample();
    liveRequirement.flags.push_back({"studio_layering", true});

    PressurePackage lowCostVideo = LowCostMusicVideoRequirementExample();
    PressurePackage albumArc = AlbumArcPressureRequirementExample();

    PressurePackage studioOnlyLayering{
        {"studio-only-layering"},
        PackageKind::CreativeRequirement,
        {"1.0"},
        {},
        {},
        {},
        {},
        {},
        {},
    };

    PressurePackage flatTrackOrder{
        {"flat-track-order"},
        PackageKind::ProjectFunction,
        {"1.0"},
        {},
        {},
        {},
        {},
        {},
        {},
    };

    PressurePackage cycleA{
        {"cycle-a"},
        PackageKind::CreativeRequirement,
        {"1.0"},
        {},
        {},
        {},
        {{{"cycle-b"}, DependencyRelation::Requires, "sample direct cycle"}},
        {},
        {},
    };

    PressurePackage cycleB{
        {"cycle-b"},
        PackageKind::CreativeRequirement,
        {"1.0"},
        {},
        {},
        {},
        {{{"cycle-a"}, DependencyRelation::Requires, "sample direct cycle"}},
        {},
        {},
    };

    return {
        liveRequirement,
        lowCostVideo,
        albumArc,
        studioOnlyLayering,
        flatTrackOrder,
        cycleA,
        cycleB,
    };
}

}

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

std::vector<PressureDiagnostic> EvaluatePressureDiagnostics(const std::vector<PressurePackage>& packages)
{
    std::vector<PressureDiagnostic> diagnostics;
    const auto providedSlots = ProvidedSlotNames(packages);

    for (const auto& package : packages) {
        for (const auto& dependency : package.dependencies) {
            if (dependency.relation == DependencyRelation::Requires && !ContainsPackageId(packages, dependency.packageId)) {
                AddDiagnostic(
                    diagnostics,
                    DiagnosticSeverity::Error,
                    DiagnosticCategory::MissingDependency,
                    package.id,
                    dependency.packageId.value + " is missing: " + dependency.reason);
            }

            if (dependency.relation == DependencyRelation::ConflictsWith && ContainsPackageId(packages, dependency.packageId)) {
                AddDiagnostic(
                    diagnostics,
                    DiagnosticSeverity::Error,
                    DiagnosticCategory::FlagConflict,
                    package.id,
                    dependency.packageId.value + " conflicts with selected package: " + dependency.reason);
            }
        }

        for (const auto& blocker : package.blockers) {
            if (ContainsPackageId(packages, blocker.packageId)) {
                AddDiagnostic(
                    diagnostics,
                    DiagnosticSeverity::Error,
                    DiagnosticCategory::Blocker,
                    package.id,
                    blocker.packageId.value + " blocks this package: " + blocker.reason);
            }
        }

        for (const auto& conflict : package.flagConflicts) {
            if (HasEnabledFlag(package, conflict.firstFlag) && HasEnabledFlag(package, conflict.secondFlag)) {
                AddDiagnostic(
                    diagnostics,
                    DiagnosticSeverity::Error,
                    DiagnosticCategory::FlagConflict,
                    package.id,
                    conflict.firstFlag + " conflicts with " + conflict.secondFlag + ": " + conflict.reason);
            }
        }

        for (const auto& requiredSlot : package.requiredSlots) {
            if (providedSlots.find(requiredSlot.slotName) == providedSlots.end()) {
                AddDiagnostic(
                    diagnostics,
                    DiagnosticSeverity::Warning,
                    DiagnosticCategory::MissingSlot,
                    package.id,
                    requiredSlot.slotName + " is missing: " + requiredSlot.reason);
            }
        }
    }

    for (std::size_t firstIndex = 0; firstIndex < packages.size(); ++firstIndex) {
        for (std::size_t secondIndex = firstIndex + 1; secondIndex < packages.size(); ++secondIndex) {
            if (HasDirectRequiresCycle(packages[firstIndex], packages[secondIndex])) {
                AddDiagnostic(
                    diagnostics,
                    DiagnosticSeverity::Error,
                    DiagnosticCategory::CircularDependency,
                    packages[firstIndex].id,
                    packages[firstIndex].id.value + " directly requires " + packages[secondIndex].id.value
                        + ", and " + packages[secondIndex].id.value + " directly requires " + packages[firstIndex].id.value);
            }
        }
    }

    return diagnostics;
}

std::vector<std::string> SamplePressureDiagnosticOutput()
{
    std::vector<std::string> formatted;
    for (const auto& diagnostic : EvaluatePressureDiagnostics(SampleWorldUpdatePackages())) {
        formatted.push_back(FormatDiagnosticMessage(diagnostic));
    }
    return formatted;
}

WorldUpdateSummary BuildWorldUpdateSummary(const std::vector<PressurePackage>& packages)
{
    WorldUpdateSummary summary;
    summary.packageCount = packages.size();
    summary.diagnostics = EvaluatePressureDiagnostics(packages);

    std::set<std::string> packagesNeedingAttention;
    for (const auto& diagnostic : summary.diagnostics) {
        switch (diagnostic.severity) {
        case DiagnosticSeverity::Info:
            ++summary.infoCount;
            break;
        case DiagnosticSeverity::Warning:
            ++summary.warningCount;
            break;
        case DiagnosticSeverity::Error:
            ++summary.errorCount;
            break;
        }

        AddGroupedDiagnostic(summary, diagnostic);
        if (!diagnostic.packageId.value.empty()) {
            packagesNeedingAttention.insert(diagnostic.packageId.value);
        }
    }

    summary.packagesNeedingAttention.assign(packagesNeedingAttention.begin(), packagesNeedingAttention.end());
    return summary;
}

std::string FormatWorldUpdateSummary(const WorldUpdateSummary& summary)
{
    std::ostringstream output;
    output << "ArtForge World Update Summary\n";
    output << "Packages: " << summary.packageCount << "\n";
    output << "Diagnostics: " << summary.errorCount << " error, "
           << summary.warningCount << " warning, "
           << summary.infoCount << " info\n";

    output << "\nPackages needing attention (" << summary.packagesNeedingAttention.size() << ")\n";
    if (summary.packagesNeedingAttention.empty()) {
        output << "- none\n";
    } else {
        for (const auto& packageId : summary.packagesNeedingAttention) {
            output << "- " << packageId << "\n";
        }
    }

    AppendDiagnosticGroup(output, "Missing dependencies", summary.missingDependencies);
    AppendDiagnosticGroup(output, "Blockers", summary.blockers);
    AppendDiagnosticGroup(output, "Flag conflicts", summary.flagConflicts);
    AppendDiagnosticGroup(output, "Missing slots", summary.missingSlots);
    AppendDiagnosticGroup(output, "Circular dependency placeholders", summary.circularDependencies);

    output << "\nNo dependency solver was run.\n";
    return output.str();
}

std::string SampleWorldUpdateSummaryOutput()
{
    return FormatWorldUpdateSummary(BuildWorldUpdateSummary(SampleWorldUpdatePackages()));
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
            {"lead_performer", "live solo setup needs one compatible performer slot"},
        },
        {
            {{"singable-range"}, DependencyRelation::Requires, "melody must stay within the artist's reliable live range"},
            {{"portable-instrumentation"}, DependencyRelation::Requires, "arrangement must work without unavailable collaborators"},
        },
        {
            {{"studio-only-layering"}, "dense layered parts would block a solo live version"},
        },
        {
            {"solo_performer", "studio_layering", "dense studio-only layering conflicts with a solo performance flag"},
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
            {"location_plan", "low-cost production needs an explicit location plan slot"},
        },
        {
            {{"few-location-plan"}, DependencyRelation::Requires, "concept must fit a small number of locations"},
            {{"daylight-availability"}, DependencyRelation::Requires, "shooting plan assumes available daylight"},
        },
        {
            {{"licensed-prop-dependency"}, "licensed or rented props conflict with the low-cost constraint"},
        },
        {},
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
            {"opening_work", "series arc needs one work assigned to the opening role"},
            {"closing_work", "series arc needs one work assigned to the closing role"},
        },
        {
            {{"opening-contrast"}, DependencyRelation::Requires, "first work must establish contrast for the arc"},
            {{"midpoint-escalation"}, DependencyRelation::Requires, "middle works must raise pressure"},
            {{"closing-resolution"}, DependencyRelation::Requires, "final work must resolve selected pressures"},
        },
        {
            {{"flat-track-order"}, "an order without contrast blocks the intended arc"},
        },
        {},
    };
}

}
