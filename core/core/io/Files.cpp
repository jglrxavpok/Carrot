//
// Created by jglrxavpok on 13/10/2021.
//

#include "Files.h"
#include "core/Macros.h"

#ifdef _WIN32
    #include <windows.h>
#endif

namespace Carrot::IO::Files {
    bool isRootFolder(const std::filesystem::path& path) {
        return path.parent_path() == path;
    }

    std::vector<std::filesystem::path> enumerateRoots() {
#ifdef _WIN32
        WCHAR buffer[256];
        DWORD length = GetLogicalDriveStringsW(sizeof(buffer), buffer);
        verify(length < sizeof(buffer), "Buffer is too small");
        std::wstring currentDrive;
        std::vector<std::filesystem::path> result;
        for (int i = 0; i < length; ++i) {
            wchar_t c = buffer[i];
            if(c == '\0') {
                result.emplace_back(currentDrive);
                currentDrive.clear();
            } else {
                currentDrive += c;
            }
        }
        return result;
#else
        return std::vector<std::filesystem::path>{"/"};
#endif
    }
}