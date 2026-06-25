#pragma once

#include <iosfwd>
#include <string_view>

namespace ArtForge::Core {

void PrintBootstrapMessage(
    std::wostream& output,
    std::wstring_view applicationName,
    std::wstring_view expectedScope,
    int argumentCount,
    wchar_t* arguments[]);

}
