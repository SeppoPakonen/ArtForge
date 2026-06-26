#include "ArtForge/Files/DomainWorkViewModels.hpp"
#include "ArtForge/Files/ScopeFiles.hpp"
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
    if (arguments != nullptr) {
        LocalFree(arguments);
    }

    return ArtForge::UiWin32::RunScopeShell(
        instance,
        showCommand,
        ArtForge::Core::WorkShellDescriptor(),
        commandLine);
}
