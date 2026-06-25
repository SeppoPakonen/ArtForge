#pragma once

#include "ArtForge/Core/ShellModel.hpp"

#include <windows.h>

namespace ArtForge::UiWin32 {

int RunScopeShell(
    HINSTANCE instance,
    int showCommand,
    ArtForge::Core::ScopeShellDescriptor descriptor,
    wchar_t* commandLine);

}
