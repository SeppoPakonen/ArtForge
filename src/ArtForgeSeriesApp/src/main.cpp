#include "ArtForge/Core/Bootstrap.hpp"

#include <iostream>

int wmain(int argc, wchar_t* argv[])
{
    ArtForge::Core::PrintBootstrapMessage(
        std::wcout,
        L"ArtForgeSeriesApp",
        L"project/album/series",
        argc,
        argv);
    return 0;
}
