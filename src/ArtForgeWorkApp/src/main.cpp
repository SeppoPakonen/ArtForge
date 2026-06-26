#include "ArtForge/Files/DomainWorkViewModels.hpp"
#include "ArtForge/Files/ScopeFiles.hpp"
#include "ArtForge/History/EventLog.hpp"
#include "ArtForge/Services/EditCommandService.hpp"
#include "ArtForge/Services/PromptCommandService.hpp"
#include "ArtForge/UiWin32/Shell.hpp"

#include <shellapi.h>
#include <windows.h>

#include <filesystem>
#include <sstream>
#include <string_view>

namespace {

void WriteStdout(std::string_view text)
{
    auto output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == INVALID_HANDLE_VALUE || output == nullptr) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        output = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (output == INVALID_HANDLE_VALUE || output == nullptr) {
        return;
    }

    DWORD written = 0;
    WriteFile(output, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
}

bool IsDescribeWorkDomainCommand(int argumentCount, wchar_t** arguments)
{
    return argumentCount >= 3 && std::wstring_view{arguments[1]} == L"--describe-work-domain";
}

bool IsBuildSelectedPromptCommand(int argumentCount, wchar_t** arguments)
{
    return argumentCount >= 6 && std::wstring_view{arguments[1]} == L"--build-selected-prompt";
}

bool IsDescribeEditCommandFoundationCommand(int argumentCount, wchar_t** arguments)
{
    return argumentCount >= 2 && std::wstring_view{arguments[1]} == L"--describe-edit-command-foundation";
}

bool IsUpdateWorkTextCommand(int argumentCount, wchar_t** arguments)
{
    return argumentCount >= 8 && std::wstring_view{arguments[1]} == L"--update-work-text";
}

bool IsRecordChangeSetHistoryCommand(int argumentCount, wchar_t** arguments)
{
    return argumentCount >= 11 && std::wstring_view{arguments[1]} == L"--record-change-set-history";
}

std::string WorkspaceLabel(std::string_view workDomain)
{
    if (workDomain == "lyrics") {
        return "lyrics/music: technical base, lyric line table, rule analytics";
    }
    if (workDomain == "visualArt") {
        return "visual art: viewer layers, paint layers, layer analytics";
    }
    if (workDomain == "scriptStoryboard") {
        return "script/storyboard: scene/block list, dialogue/action, storyboard visualization";
    }
    if (workDomain.empty()) {
        return "unsupported: missing workDomain";
    }
    return "unsupported: unknown workDomain";
}

std::string DescribeWorkDomain(const std::filesystem::path& path)
{
    const auto result = ArtForge::Files::LoadWorkScopeFile(path);
    std::ostringstream output;
    output << "Path: " << path.generic_string() << "\n";
    output << "Load status: " << (result.status.ok ? "OK" : "failed") << "\n";
    if (!result.status.ok && !result.status.issues.empty()) {
        output << "Issue: " << result.status.issues.front().message << "\n";
    }
    output << "Work id: " << result.file.id << "\n";
    output << "Work domain: " << (result.file.workDomain.empty() ? "(unspecified)" : result.file.workDomain) << "\n";
    output << "Workspace: " << WorkspaceLabel(result.file.workDomain) << "\n";
    output << ArtForge::Files::DescribeDomainWorkViewModel(path);
    return output.str();
}

std::string WideToUtf8(std::wstring_view value)
{
    if (value.empty()) {
        return {};
    }

    const auto requiredLength = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (requiredLength <= 0) {
        std::string fallback;
        for (const auto character : value) {
            fallback += character <= 0x7f ? static_cast<char>(character) : '?';
        }
        return fallback;
    }

    std::string converted(static_cast<std::size_t>(requiredLength), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.data(),
        static_cast<int>(value.size()),
        converted.data(),
        requiredLength,
        nullptr,
        nullptr);
    return converted;
}

std::string BuildSelectedPromptDebugDump(wchar_t** arguments)
{
    const std::filesystem::path workPath{arguments[2]};
    const auto itemType = WideToUtf8(arguments[3]);
    const auto itemIndex = std::stoi(std::wstring{arguments[4]});
    const auto requestedOperation = WideToUtf8(arguments[5]);

    const auto result = ArtForge::Services::BuildSelectedItemPromptCommand({
        workPath,
        "auto",
        itemType,
        "",
        itemIndex,
        requestedOperation,
    });
    const auto work = ArtForge::Files::LoadWorkScopeFile(workPath);
    const auto historyStatus = ArtForge::History::RecordPresentationCommandHistoryEvent(
        workPath,
        ArtForge::History::HistoryOperation::SelectedItemPromptPackageBuilt,
        {
            work.file.id,
            workPath.generic_string(),
            work.file.workDomain,
            itemType,
            "",
            itemIndex,
            "build-selected-item-prompt",
            result.command.status.ok ? "OK" : "failed",
        });
    (void)historyStatus;
    return result.promptDebugDump;
}

std::string UpdateWorkText(wchar_t** arguments)
{
    const auto result = ArtForge::Files::UpdateWorkDomainTextField({
        std::filesystem::path{arguments[2]},
        WideToUtf8(arguments[3]),
        WideToUtf8(arguments[4]),
        WideToUtf8(arguments[5]),
        WideToUtf8(arguments[6]),
        WideToUtf8(arguments[7]),
    });

    std::ostringstream output;
    output << "Path: " << result.path.generic_string() << "\n";
    output << "Update status: " << (result.status.ok ? "OK" : "failed") << "\n";
    output << "Previous: " << result.previousText << "\n";
    output << "Replacement: " << result.replacementText << "\n";
    for (const auto& issue : result.status.issues) {
        output << "Issue: " << issue.message << "\n";
    }
    return output.str();
}

std::string RecordChangeSetHistory(wchar_t** arguments)
{
    const auto path = std::filesystem::path{arguments[2]};
    const auto operationName = WideToUtf8(arguments[3]);
    auto operation = ArtForge::History::HistoryOperation::ChangeSetApplied;
    if (operationName == "edit-command-requested") {
        operation = ArtForge::History::HistoryOperation::EditCommandRequested;
    } else if (operationName == "change-set-validated") {
        operation = ArtForge::History::HistoryOperation::ChangeSetValidated;
    } else if (operationName == "save-requested") {
        operation = ArtForge::History::HistoryOperation::SaveRequested;
    } else if (operationName == "save-succeeded") {
        operation = ArtForge::History::HistoryOperation::SaveSucceeded;
    } else if (operationName == "save-failed") {
        operation = ArtForge::History::HistoryOperation::SaveFailed;
    }

    const auto load = ArtForge::Files::LoadWorkScopeFile(path);
    const auto status = ArtForge::History::RecordChangeSetHistoryEvent(
        path,
        operation,
        {
            load.file.id,
            path.generic_string(),
            WideToUtf8(arguments[4]),
            WideToUtf8(arguments[5]),
            WideToUtf8(arguments[6]),
            std::stoi(std::wstring{arguments[7]}),
            WideToUtf8(arguments[8]),
            std::stoi(std::wstring{arguments[9]}),
            WideToUtf8(arguments[10]),
        });

    std::ostringstream output;
    output << "History status: " << (status.ok ? "OK" : "failed") << "\n";
    for (const auto& issue : status.issues) {
        output << "Issue: " << issue.message << "\n";
    }
    return output.str();
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, wchar_t* commandLine, int showCommand)
{
    int argumentCount = 0;
    const auto arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments != nullptr && IsDescribeWorkDomainCommand(argumentCount, arguments)) {
        const auto result = DescribeWorkDomain(arguments[2]);
        WriteStdout(result);
        const auto loadResult = ArtForge::Files::LoadWorkScopeFile(arguments[2]);
        LocalFree(arguments);
        return loadResult.status.ok ? 0 : 2;
    }
    if (arguments != nullptr && IsBuildSelectedPromptCommand(argumentCount, arguments)) {
        const auto result = BuildSelectedPromptDebugDump(arguments);
        WriteStdout(result);
        LocalFree(arguments);
        return 0;
    }
    if (arguments != nullptr && IsDescribeEditCommandFoundationCommand(argumentCount, arguments)) {
        const auto result = ArtForge::Services::DescribeEditCommandSmokeExamples();
        WriteStdout(result);
        LocalFree(arguments);
        return 0;
    }
    if (arguments != nullptr && IsUpdateWorkTextCommand(argumentCount, arguments)) {
        const auto result = UpdateWorkText(arguments);
        WriteStdout(result);
        LocalFree(arguments);
        return result.find("Update status: OK") != std::string::npos ? 0 : 2;
    }
    if (arguments != nullptr && IsRecordChangeSetHistoryCommand(argumentCount, arguments)) {
        const auto result = RecordChangeSetHistory(arguments);
        WriteStdout(result);
        LocalFree(arguments);
        return result.find("History status: OK") != std::string::npos ? 0 : 2;
    }
    if (arguments != nullptr) {
        LocalFree(arguments);
    }

    return ArtForge::UiWin32::RunScopeShell(
        instance,
        showCommand,
        ArtForge::Core::WorkShellDescriptor(),
        commandLine);
}
