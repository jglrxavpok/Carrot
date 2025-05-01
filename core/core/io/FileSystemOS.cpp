//
// Created by jglrxavpok on 08/05/2022.
//

#include "FileSystemOS.h"

#include "core/utils/stringmanip.h"

#ifdef _WIN32
    #include <windows.h>
#elifdef __linux__
    #include <sys/stat.h>
#endif

namespace Carrot::IO {
    bool hasExecutableInWorkingDirectory(const char* exeName) {
#ifdef _WIN32
        return std::filesystem::exists(Carrot::sprintf("%s.exe", exeName));
#else
        struct stat sb{};
        if (stat(exeName, &sb) == 0) {
            return std::filesystem::exists(exeName) && sb.st_mode & S_IXUSR;
        } else {
            return false;
        }
#endif
    }

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
#elifdef __linux__
        struct stat64 stats{};
        if (lstat64("/proc/self/exe", &stats) == -1) {
            throw std::runtime_error("Could not lstat64 own exe??");
        }

        const i64 bufferSize = stats.st_size ? stats.st_size+1 : 1024;
        std::string buffer;
        buffer.resize(bufferSize);
        const i64 readSize = readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (readSize == -1) {
            throw std::runtime_error("Could not readlink own exe??");
        }

        buffer.resize(readSize);
        return buffer;
#else
        static_assert(false, "Not supported on this OS. Please fix.");
#endif
    }

    bool openFileInDefaultEditor(const std::filesystem::path& filepath) {
#ifdef _WIN32
        // https://stackoverflow.com/a/9115792
        HINSTANCE result = ShellExecuteW(nullptr, nullptr, filepath.c_str(), nullptr, nullptr, SW_SHOW);
        if((INT_PTR)result > 32) { // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutea
            return true;
        } else {
            return false;
        }
#elifdef __linux__
        int returnValue = system(Carrot::sprintf("open %s", filepath.c_str()).c_str());
        if (returnValue == -1) {
            return false;
        }
        if (returnValue == 127) {
            return false;
        }
        return true;
#else
        static_assert(false, "Not supported on this OS. Please fix.");
#endif
    }

};