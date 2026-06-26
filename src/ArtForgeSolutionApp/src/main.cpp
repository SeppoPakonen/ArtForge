#include "ArtForge/Files/ProjectGraph.hpp"
#include "ArtForge/UiWin32/Shell.hpp"

#include <shellapi.h>
#include <windows.h>

#include <string>
#include <string_view>

namespace {

void WriteStdout(std::string_view text)
{
    AttachConsole(ATTACH_PARENT_PROCESS);
    const auto output = GetStdHandle(STD_OUTPUT_HANDLE);
    if (output == INVALID_HANDLE_VALUE || output == nullptr) {
        return;
    }

    DWORD written = 0;
    WriteFile(output, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
}

bool IsDescribeGraphCommand(int argumentCount, wchar_t** arguments)
{
    return argumentCount >= 3 && std::wstring_view{arguments[1]} == L"--describe-graph";
}

}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, wchar_t* commandLine, int showCommand)
{
    int argumentCount = 0;
    const auto arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
    if (arguments != nullptr && IsDescribeGraphCommand(argumentCount, arguments)) {
        const auto graph = ArtForge::Files::LoadSolutionProjectGraph(arguments[2]);
        WriteStdout(ArtForge::Files::DescribeProjectGraph(graph));
        const auto issueCount = ArtForge::Files::FlattenGraphIssues(graph).size();
        LocalFree(arguments);
        return issueCount == 0 ? 0 : 2;
    }

    if (arguments != nullptr) {
        LocalFree(arguments);
    }

    return ArtForge::UiWin32::RunScopeShell(
        instance,
        showCommand,
        ArtForge::Core::SolutionShellDescriptor(),
        commandLine);
}
