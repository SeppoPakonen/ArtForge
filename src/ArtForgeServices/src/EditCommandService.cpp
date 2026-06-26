#include "ArtForge/Services/EditCommandService.hpp"

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

}
