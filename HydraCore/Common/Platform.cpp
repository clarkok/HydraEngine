#include "Platform.h"

#ifdef _MSC_VER
#include "Shlwapi.h"
#endif

namespace hydra
{
namespace platform
{

#ifdef _MSC_VER

std::string NormalizePath(std::initializer_list<std::string> strs)
{
    char buffer1[MAX_PATH];
    char buffer2[MAX_PATH];

    char *buf1 = buffer1, *buf2 = buffer2;

    auto result = GetCurrentDirectory(MAX_PATH, buf1);
    hydra_assert(result, "GetCurrentDirectory failed");

    for (auto &str : strs)
    {
        auto result = PathCombine(buf2, buf1, str.c_str());
        hydra_assert(result, "PathCombine failed");
        std::swap(buf1, buf2);
    }

    result = PathCanonicalize(buf2, buf1);
    hydra_assert(result, "PathCanonicalize failed");

    return buf2;
}

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

#endif

} // namespace platform
} // namepsace hydra