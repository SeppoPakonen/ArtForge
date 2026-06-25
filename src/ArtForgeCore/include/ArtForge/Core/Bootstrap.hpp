#pragma once

#include "ArtForge/Core/ShellModel.hpp"

#include <iosfwd>
#include <string_view>

namespace ArtForge::Core {

void PrintBootstrapMessage(
    std::wostream& output,
    std::wstring_view applicationName,
    std::wstring_view expectedScope,
    int argumentCount,
    wchar_t* arguments[]);

void PrintBootstrapMessage(
    std::wostream& output,
    ScopeShellDescriptor shell,
    int argumentCount,
    wchar_t* arguments[]);

}
