#include "ArtForge/Core/Bootstrap.hpp"

#include "ArtForge/Core/ProcessModel.hpp"

#include <ostream>

namespace ArtForge::Core {

void PrintBootstrapMessage(
    std::wostream& output,
    std::wstring_view applicationName,
    std::wstring_view expectedScope,
    int argumentCount,
    wchar_t* arguments[])
{
    output << L"Application: " << applicationName << L'\n';
    output << L"Expected scope: " << expectedScope << L'\n';
    output << L"Path argument: ";
    if (argumentCount > 1 && arguments != nullptr && arguments[1] != nullptr) {
        output << arguments[1];
    } else {
        output << L"(none)";
    }
    output << L'\n';
    output << L"Process model: " << ProcessModelStatus() << L'\n';
    output << L"Future event boundary: " << SupportedProcessEventCount()
           << L" event types stubbed" << L'\n';
    output << L"ArtForge bootstrap OK" << L'\n';
}

}
