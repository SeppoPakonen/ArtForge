#pragma once

#include "ArtForge/Services/ServiceResult.hpp"
#include "ArtForge/Prompting/PromptPackage.hpp"

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

struct SelectedTextEditCommandRequest {
    std::filesystem::path workPath;
    std::string domain;
    std::string itemType;
    std::string itemId;
    int itemIndex{-1};
    std::string replacementText;
    std::string actor;
};

struct SelectedTextEditCommandResult {
    EditCommandResult edit;
    ServiceCommandResult save;
};

struct AcceptPendingSuggestionRequest {
    ArtForge::Prompting::PendingSuggestion suggestion;
    std::filesystem::path workPath;
    std::string expectedWorkId;
    std::string expectedCurrentText;
    std::string actor;
};

struct AcceptPendingSuggestionResult {
    ServiceStatus status;
    ArtForge::Prompting::PendingSuggestion suggestion;
    std::string currentText;
    std::vector<std::string> warnings;
    SelectedTextEditCommandResult edit;
};

struct RejectPendingSuggestionRequest {
    ArtForge::Prompting::PendingSuggestion suggestion;
    std::string reason;
    std::string actor;
};

struct RejectPendingSuggestionResult {
    ServiceStatus status;
    ArtForge::Prompting::PendingSuggestion suggestion;
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
SelectedTextEditCommandResult ApplySelectedTextEditCommand(const SelectedTextEditCommandRequest& request);
AcceptPendingSuggestionResult AcceptPendingSuggestionCommand(const AcceptPendingSuggestionRequest& request);
std::string DescribeAcceptPendingSuggestionResult(const AcceptPendingSuggestionResult& result);
std::string DescribeAcceptPendingSuggestionSmokeExamples();
RejectPendingSuggestionResult RejectPendingSuggestionCommand(const RejectPendingSuggestionRequest& request);
std::string DescribeRejectPendingSuggestionResult(const RejectPendingSuggestionResult& result);
std::string DescribeRejectPendingSuggestionSmokeExamples();

}
