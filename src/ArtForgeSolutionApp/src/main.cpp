#include "ArtForge/Deps/PressureModel.hpp"
#include "ArtForge/Files/ProjectGraph.hpp"
#include "ArtForge/Prompting/PromptPackage.hpp"
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

bool IsBuildPromptPackageCommand(int argumentCount, wchar_t** arguments)
{
    return argumentCount >= 9 && std::wstring_view{arguments[1]} == L"--build-prompt-package";
}

bool IsWorldUpdateSampleCommand(int argumentCount, wchar_t** arguments)
{
    return argumentCount >= 2 && std::wstring_view{arguments[1]} == L"--world-update-sample";
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
        return {};
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
    if (arguments != nullptr && IsBuildPromptPackageCommand(argumentCount, arguments)) {
        ArtForge::Prompting::PromptPackageBuildRequest request;
        request.solutionPath = arguments[2];
        request.workId = WideToUtf8(arguments[3]);
        request.generalRuleFiles.push_back(arguments[4]);
        request.domainRuleFiles.push_back(arguments[5]);
        request.projectRuleFiles.push_back(arguments[6]);
        request.taskInstructionPath = arguments[7];
        request.outputSchemaPath = arguments[8];

        const auto result = ArtForge::Prompting::BuildPromptPackageFromWorkContext(request);
        WriteStdout(ArtForge::Prompting::SerializePromptPackageDebugDump(result));
        LocalFree(arguments);
        return result.ok ? 0 : 2;
    }
    if (arguments != nullptr && IsWorldUpdateSampleCommand(argumentCount, arguments)) {
        WriteStdout(ArtForge::Deps::SampleWorldUpdateSummaryOutput());
        LocalFree(arguments);
        return 0;
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
