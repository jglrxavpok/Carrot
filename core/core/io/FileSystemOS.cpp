//
// Created by jglrxavpok on 08/05/2022.
//

#include "FileSystemOS.h"
#include "core/utils/stringmanip.h"

#ifdef _WIN32
    #include <windows.h>
#else

#endif

namespace Carrot::IO {
    std::filesystem::path getExecutablePath() {
#ifdef _WIN32
        std::size_t bufferSize = 256;

        std::string buffer;
        do {
            buffer.reserve(bufferSize);
            GetModuleFileName(nullptr, buffer.data(), bufferSize);
            DWORD error = GetLastError();
            if(error == ERROR_INSUFFICIENT_BUFFER) {
                bufferSize *= 2;
            } else if(error == 0) {
                break;
            } else {
                throw std::runtime_error(Carrot::sprintf("Failed to get exe path, error is 0x%x", error));
            }
        } while(true);

        return {buffer.c_str()};
#else
        static_assert(false, "Not supported on this OS. Please fix.");
#endif
    }
};