#include "Platform.h"

namespace hydra
{
namespace platform
{

#ifdef _MSC_VER

MappedFile::MappedFile(const std::string &filename)
{
    File = CreateFileA(
        filename.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (File == INVALID_HANDLE_VALUE)
    {
        auto error = GetLastError();
        hydra_trap("Open file failed: " + std::to_string(error));
    }

    Mapping = CreateFileMappingA(
        File,
        NULL,
        PAGE_READONLY,
        0,
        0,
        NULL
    );

    if (Mapping == NULL)
    {
        auto error = GetLastError();
        hydra_trap("Mapping file failed: " + std::to_string(error));
    }

    View = MapViewOfFile(
        Mapping,
        FILE_MAP_READ,
        0,
        0,
        0
    );

    if (View == NULL)
    {
        auto error = GetLastError();
        hydra_trap("Map view of file failed: " + std::to_string(error));
    }
}

MappedFile::~MappedFile()
{
    auto result = UnmapViewOfFile(View);
    CloseHandle(Mapping);
    CloseHandle(File);
}

void Break()
{
    DebugBreak();
}

#endif

} // namespace platform
} // namepsace hydra