#include "ArtForge/Services/EditCommandService.hpp"

#include "ArtForge/Files/DomainWorkViewModels.hpp"

#include <sstream>
#include <utility>

namespace ArtForge::Services {

namespace {

std::string DescribeTarget(const EditTarget& target)
{
    std::ostringstream output;
    output << target.domain << "/" << target.itemType;
    if (!target.itemId.empty()) {
        output << "#" << target.itemId;
    } else if (target.itemIndex >= 0) {
        output << "[" << target.itemIndex << "]";
    }
    if (!target.field.empty()) {
        output << "." << target.field;
    }
    return output.str();
}

std::string FileItemTypeForSelection(std::string_view domain, std::string_view itemType, std::string_view itemId)
{
    if (domain == "lyrics" && (itemType == "lyricLine" || itemType == "lyric_line")) {
        return "lyric_line";
    }
    if (domain == "visualArt" && (itemType == "visualLayer" || itemType == "visual_layer")) {
        if (itemId.find("paint.") == 0) {
            return "paint_layer";
        }
        return "viewer_layer";
    }
    if (domain == "scriptStoryboard" && (itemType == "scriptBlock" || itemType == "script_block")) {
        return "script_block";
    }
    return {};
}

std::string EditableFieldForSelection(std::string_view domain, std::string_view itemType)
{
    if (domain == "lyrics" && (itemType == "lyricLine" || itemType == "lyric_line")) {
        return "text";
    }
    if (domain == "visualArt" && (itemType == "visualLayer" || itemType == "visual_layer")) {
        return "intent";
    }
    if (domain == "scriptStoryboard" && (itemType == "scriptBlock" || itemType == "script_block")) {
        return "text";
    }
    return {};
}

EditTarget MakeTarget(
    std::string domain,
    std::string itemType,
    std::string itemId,
    int itemIndex,
    std::string field)
{
    EditTarget target;
    target.scopePath = "examples/work.afwork";
    target.domain = std::move(domain);
    target.itemType = std::move(itemType);
    target.itemId = std::move(itemId);
    target.itemIndex = itemIndex;
    target.field = std::move(field);
    return target;
}

}

EditCommand MakeReplaceTextCommand(
    std::string commandId,
    EditTarget target,
    std::string replacementText,
    std::string actor,
    std::string source)
{
    EditCommand command;
    command.commandId = std::move(commandId);
    command.operation = EditOperation::ReplaceText;
    command.target = std::move(target);
    command.replacementText = std::move(replacementText);
    command.actor = std::move(actor);
    command.source = std::move(source);
    return command;
}

EditCommandResult PreviewReplaceTextCommand(
    const EditCommand& command,
    std::string currentValue)
{
    EditCommandResult result;
    result.command = command;

    if (command.operation != EditOperation::ReplaceText) {
        result.status = MakeErrorStatus("unsupported_edit_operation", "Only text replacement commands are supported.");
        result.changeSet.state = ChangeSetState::Rejected;
        return result;
    }

    if (command.target.domain.empty() || command.target.itemType.empty() || command.target.field.empty()) {
        result.status = MakeErrorStatus("invalid_edit_target", "Edit target requires domain, item type, and field.");
        result.changeSet.state = ChangeSetState::Rejected;
        return result;
    }

    result.status = MakeOkStatus("Edit command validated as a pending change set.");
    result.changeSet.changeSetId = command.commandId.empty() ? "changeset.preview" : command.commandId + ".changeset";
    result.changeSet.commandId = command.commandId;
    result.changeSet.state = ChangeSetState::Pending;
    result.changeSet.actor = command.actor;
    result.changeSet.changes.push_back({command.target, std::move(currentValue), command.replacementText});
    result.dirtyState.isDirty = true;
    result.dirtyState.canSave = false;
    result.dirtyState.path = command.target.scopePath;
    result.dirtyState.pendingChangeCount = static_cast<int>(result.changeSet.changes.size());
    return result;
}

std::string DescribeEditOperation(EditOperation operation)
{
    switch (operation) {
    case EditOperation::ReplaceText:
        return "replace text";
    }
    return "unknown";
}

std::string DescribeChangeSetState(ChangeSetState state)
{
    switch (state) {
    case ChangeSetState::Pending:
        return "pending";
    case ChangeSetState::Applied:
        return "applied";
    case ChangeSetState::Rejected:
        return "rejected";
    }
    return "unknown";
}

std::string DescribeEditCommandResult(const EditCommandResult& result)
{
    std::ostringstream output;
    output << "Command: " << result.command.commandId << "\n";
    output << "Operation: " << DescribeEditOperation(result.command.operation) << "\n";
    output << "Target: " << DescribeTarget(result.command.target) << "\n";
    output << "Status: " << (result.status.ok ? "OK" : "failed") << "\n";
    if (!result.status.summary.empty()) {
        output << "Summary: " << result.status.summary << "\n";
    }
    output << "Change set: " << result.changeSet.changeSetId << "\n";
    output << "State: " << DescribeChangeSetState(result.changeSet.state) << "\n";
    for (const auto& change : result.changeSet.changes) {
        output << "Change: " << DescribeTarget(change.target) << "\n";
        output << "Before: " << change.beforeValue << "\n";
        output << "After: " << change.afterValue << "\n";
    }
    output << "Dirty: " << (result.dirtyState.isDirty ? "yes" : "no") << "\n";
    output << "Can save: " << (result.dirtyState.canSave ? "yes" : "no") << "\n";
    output << "Pending changes: " << result.dirtyState.pendingChangeCount << "\n";
    return output.str();
}

std::string DescribeEditCommandSmokeExamples()
{
    std::ostringstream output;

    const auto lyric = PreviewReplaceTextCommand(
        MakeReplaceTextCommand(
            "edit.lyric.001",
            MakeTarget("lyrics", "lyric_line", "line.v1.001", -1, "text"),
            "I keep the doorway lit tonight",
            "codex",
            "smoke"),
        "I leave the porch light on tonight");
    output << "Example: lyric line text\n" << DescribeEditCommandResult(lyric) << "\n";

    const auto visual = PreviewReplaceTextCommand(
        MakeReplaceTextCommand(
            "edit.visual.001",
            MakeTarget("visualArt", "visual_layer", "viewer.foreground", -1, "intent"),
            "Lead the eye to the figure before the background.",
            "codex",
            "smoke"),
        "Foreground establishes scale.");
    output << "Example: visual layer intent\n" << DescribeEditCommandResult(visual) << "\n";

    const auto script = PreviewReplaceTextCommand(
        MakeReplaceTextCommand(
            "edit.script.001",
            MakeTarget("scriptStoryboard", "script_block", "block.opening.dialogue", -1, "dialogue"),
            "We start before the room understands what changed.",
            "codex",
            "smoke"),
        "We start before anyone knows what changed.");
    output << "Example: script block text\n" << DescribeEditCommandResult(script);

    return output.str();
}

SaveWorkDocumentResult SaveWorkDocumentCommand(const SaveWorkDocumentRequest& request)
{
    SaveWorkDocumentResult result;
    result.dirtyState = request.dirtyState;
    result.dirtyState.path = request.path.generic_string();
    result.command.commandName = "save-work-document";
    result.command.outputPath = request.path.generic_string();

    if (request.path.empty()) {
        result.command.status = MakeErrorStatus("missing_work_path", "Save requires an opened work file path.");
        result.command.debugSummary = "Save failed: missing work file path.";
        result.dirtyState.canSave = false;
        result.dirtyState.lastSaveError = result.command.status.summary;
        return result;
    }

    if (!request.dirtyState.isDirty && request.dirtyState.pendingChangeCount == 0) {
        result.command.status = MakeOkStatus("No changes to save.");
        result.command.debugSummary = "Save no-op: document is clean.";
        result.dirtyState.isDirty = false;
        result.dirtyState.canSave = false;
        result.dirtyState.lastSaveError.clear();
        return result;
    }

    result.command.status = MakeErrorStatus("save_transaction_not_available", "Dirty save transactions are not available until edits are applied through change sets.");
    result.command.debugSummary = "Save failed: dirty transaction support is not active.";
    result.dirtyState.canSave = false;
    result.dirtyState.lastSaveError = result.command.status.summary;
    return result;
}

SelectedTextEditCommandResult ApplySelectedTextEditCommand(const SelectedTextEditCommandRequest& request)
{
    SelectedTextEditCommandResult result;

    const auto fileItemType = FileItemTypeForSelection(request.domain, request.itemType, request.itemId);
    const auto editableField = EditableFieldForSelection(request.domain, request.itemType);
    if (fileItemType.empty() || editableField.empty()) {
        result.edit.status = MakeErrorStatus("unsupported_selected_text_edit", "Selected item does not expose a supported text edit field.");
        result.save.status = result.edit.status;
        result.save.commandName = "apply-selected-text-edit";
        return result;
    }

    EditTarget target;
    target.scopePath = request.workPath.generic_string();
    target.domain = request.domain;
    target.itemType = request.itemType;
    target.itemId = request.itemId;
    target.itemIndex = request.itemIndex;
    target.field = editableField;

    auto command = MakeReplaceTextCommand(
        "edit.selected-text",
        target,
        request.replacementText,
        request.actor.empty() ? "user" : request.actor,
        "workapp");
    result.edit = PreviewReplaceTextCommand(command, "");
    if (!result.edit.status.ok) {
        result.save.status = result.edit.status;
        result.save.commandName = "apply-selected-text-edit";
        return result;
    }

    const auto update = ArtForge::Files::UpdateWorkDomainTextField({
        request.workPath,
        request.domain,
        fileItemType,
        request.itemId,
        editableField,
        request.replacementText,
    });

    result.save.commandName = "apply-selected-text-edit";
    result.save.outputPath = request.workPath.generic_string();
    if (!update.status.ok) {
        const auto message = update.status.issues.empty() ? std::string{"Selected text edit failed."} : update.status.issues.front().message;
        result.save.status = MakeErrorStatus("selected_text_edit_failed", message);
        result.save.debugSummary = "Selected text edit failed before save completion.";
        result.edit.changeSet.state = ChangeSetState::Rejected;
        result.edit.status = result.save.status;
        return result;
    }

    result.edit.changeSet.state = ChangeSetState::Applied;
    if (!result.edit.changeSet.changes.empty()) {
        result.edit.changeSet.changes.front().beforeValue = update.previousText;
    }
    result.edit.dirtyState.isDirty = false;
    result.edit.dirtyState.canSave = false;
    result.edit.dirtyState.pendingChangeCount = 0;
    result.save.status = MakeOkStatus("Selected text edit saved.");
    result.save.debugSummary = "Selected text edit applied through edit command service and safe file roundtrip.";
    return result;
}

}
