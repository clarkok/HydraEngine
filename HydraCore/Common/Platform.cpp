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

    hydra_assert(File != INVALID_HANDLE_VALUE,
        "Open file failed");

    Mapping = CreateFileMappingA(
        File,
        NULL,
        PAGE_READONLY,
        0,
        0,
        filename.c_str()
    );

    hydra_assert(Mapping != INVALID_HANDLE_VALUE,
        "File mapping failed");

    View = MapViewOfFile(
        Mapping,
        FILE_MAP_READ,
        0,
        0,
        0
    );

    hydra_assert(View != nullptr,
        "Map view of file failed");
}

MappedFile::~MappedFile()
{
    auto result = UnmapViewOfFile(View);
    CloseHandle(Mapping);
    CloseHandle(File);
}

#endif

} // namespace platform
} // namepsace hydra