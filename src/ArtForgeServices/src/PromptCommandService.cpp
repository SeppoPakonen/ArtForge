#include "ArtForge/Services/PromptCommandService.hpp"

#include "ArtForge/Files/ScopeFiles.hpp"
#include "ArtForge/Prompting/PromptPackage.hpp"

namespace ArtForge::Services {

namespace {

std::string AutoDomain(const SelectedItemPromptCommandRequest& request)
{
    if (!request.domain.empty() && request.domain != "auto") {
        return request.domain;
    }

    const auto work = ArtForge::Files::LoadWorkScopeFile(request.workPath);
    return work.file.workDomain;
}

ServiceStatus ValidateRequest(const SelectedItemPromptCommandRequest& request, const std::string& domain)
{
    ServiceStatus status = MakeOkStatus("Selected-item prompt package built.");
    if (request.workPath.empty()) {
        status.ok = false;
        status.diagnostics.push_back({ServiceSeverity::Error, "missing_work_path", "work path is required"});
    }
    if (domain.empty()) {
        status.ok = false;
        status.diagnostics.push_back({ServiceSeverity::Error, "missing_domain", "domain is required or must be auto-detectable"});
    }
    if (request.itemType.empty()) {
        status.ok = false;
        status.diagnostics.push_back({ServiceSeverity::Error, "missing_item_type", "selected item type is required"});
    }
    if (request.operation.empty()) {
        status.ok = false;
        status.diagnostics.push_back({ServiceSeverity::Error, "missing_operation", "requested operation is required"});
    }
    if (!status.ok) {
        status.summary = "Selected-item prompt package could not be built.";
    }
    return status;
}

}

SelectedItemPromptCommandResult BuildSelectedItemPromptCommand(
    const SelectedItemPromptCommandRequest& request)
{
    SelectedItemPromptCommandResult result;
    result.command.commandName = "build-selected-item-prompt";

    const auto work = ArtForge::Files::LoadWorkScopeFile(request.workPath);
    const auto domain = AutoDomain(request);
    result.command.status = ValidateRequest(request, domain);
    if (!work.status.ok) {
        result.command.status.ok = false;
        result.command.status.summary = "Selected-item prompt package could not be built.";
        for (const auto& issue : work.status.issues) {
            result.command.status.diagnostics.push_back({ServiceSeverity::Error, "work_load_failed", issue.message});
        }
    }

    ArtForge::Prompting::SelectedDomainItemPromptRequest promptRequest{
        request.workPath,
        work.file.id,
        domain,
        request.itemType,
        request.itemId,
        request.itemIndex,
        request.operation,
    };

    const auto prompt = ArtForge::Prompting::BuildPromptPackageFromSelectedDomainItem(promptRequest);
    for (const auto& issue : prompt.issues) {
        result.command.status.ok = false;
        result.command.status.diagnostics.push_back({ServiceSeverity::Error, "prompt_build_issue", issue});
    }

    result.promptDebugDump = ArtForge::Prompting::SerializePromptPackageDebugDump(prompt);
    result.command.debugSummary = "Prompt layers: " + std::to_string(prompt.layers.size());
    return result;
}

}
