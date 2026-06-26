#pragma once

#include "ArtForge/Services/ServiceResult.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace ArtForge::Services {

enum class EditOperation {
    ReplaceText,
};

enum class ChangeSetState {
    Pending,
    Applied,
    Rejected,
};

struct EditTarget {
    std::string scopePath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{-1};
    std::string field;
};

struct EditCommand {
    std::string commandId;
    EditOperation operation{EditOperation::ReplaceText};
    EditTarget target;
    std::string actor;
    std::string source;
    std::string replacementText;
};

struct Change {
    EditTarget target;
    std::string beforeValue;
    std::string afterValue;
};

struct ChangeSet {
    std::string changeSetId;
    std::string commandId;
    ChangeSetState state{ChangeSetState::Pending};
    std::string actor;
    std::vector<Change> changes;
    std::vector<ServiceDiagnostic> diagnostics;
};

struct DirtyState {
    bool isDirty{};
    bool canSave{};
    std::string path;
    int pendingChangeCount{};
    std::string lastSaveError;
};

struct EditCommandResult {
    ServiceStatus status;
    EditCommand command;
    ChangeSet changeSet;
    DirtyState dirtyState;
};

struct SaveWorkDocumentRequest {
    std::filesystem::path path;
    DirtyState dirtyState;
};

struct SaveWorkDocumentResult {
    ServiceCommandResult command;
    DirtyState dirtyState;
};

EditCommand MakeReplaceTextCommand(
    std::string commandId,
    EditTarget target,
    std::string replacementText,
    std::string actor,
    std::string source);

EditCommandResult PreviewReplaceTextCommand(
    const EditCommand& command,
    std::string currentValue);

std::string DescribeEditOperation(EditOperation operation);
std::string DescribeChangeSetState(ChangeSetState state);
std::string DescribeEditCommandResult(const EditCommandResult& result);
std::string DescribeEditCommandSmokeExamples();
SaveWorkDocumentResult SaveWorkDocumentCommand(const SaveWorkDocumentRequest& request);

}
