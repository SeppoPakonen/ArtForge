#include "ArtForge/Files/DomainWorkViewModels.hpp"
#include "ArtForge/Files/ScopeFiles.hpp"
#include "ArtForge/History/EventLog.hpp"
#include "ArtForge/Prompting/PromptPackage.hpp"
#include "ArtForge/UiWin32/Shell.hpp"

#include <shellapi.h>
#include <windows.h>

#include <filesystem>
#include <sstream>
#include <vector>
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
    const auto work = ArtForge::Files::LoadWorkScopeFile(workPath);
    const auto itemType = WideToUtf8(arguments[3]);
    const auto itemIndex = std::stoi(std::wstring{arguments[4]});
    const auto requestedOperation = WideToUtf8(arguments[5]);
    const auto selectedItemId = [&]() {
        const auto index = itemIndex >= 0 ? static_cast<std::size_t>(itemIndex) : std::size_t{};
        if (work.file.workDomain == "lyrics") {
            const auto model = ArtForge::Files::LoadLyricsWorkViewModel(workPath);
            return index < model.lyricLines.size() ? model.lyricLines[index].id : std::string{};
        }
        if (work.file.workDomain == "visualArt") {
            const auto model = ArtForge::Files::LoadVisualArtWorkViewModel(workPath);
            std::vector<ArtForge::Files::VisualArtLayerView> layers = model.viewerLayers;
            layers.insert(layers.end(), model.paintLayers.begin(), model.paintLayers.end());
            return index < layers.size() ? layers[index].id : std::string{};
        }
        if (work.file.workDomain == "scriptStoryboard") {
            const auto model = ArtForge::Files::LoadScriptStoryboardWorkViewModel(workPath);
            return index < model.blocks.size() ? model.blocks[index].id : std::string{};
        }
        return std::string{};
    }();

    ArtForge::Prompting::SelectedDomainItemPromptRequest request{
        workPath,
        work.file.id,
        work.file.workDomain,
        itemType,
        selectedItemId,
        itemIndex,
        requestedOperation,
    };
    const auto result = ArtForge::Prompting::BuildPromptPackageFromSelectedDomainItem(request);
    const auto historyStatus = ArtForge::History::RecordDomainItemHistoryEvent(
        workPath,
        ArtForge::History::HistoryOperation::PromptRequested,
        {
            work.file.id,
            workPath.generic_string(),
            work.file.workDomain,
            itemType,
            selectedItemId,
            itemIndex,
            "Prompt requested: " + requestedOperation,
        });
    (void)historyStatus;
    return ArtForge::Prompting::SerializePromptPackageDebugDump(result);
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
    if (arguments != nullptr) {
        LocalFree(arguments);
    }

    return ArtForge::UiWin32::RunScopeShell(
        instance,
        showCommand,
        ArtForge::Core::WorkShellDescriptor(),
        commandLine);
}
