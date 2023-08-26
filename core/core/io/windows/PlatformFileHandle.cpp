//
// Created by jglrxavpok on 25/08/2023.
//

#include "PlatformFileHandle.h"
#include "core/utils/Assert.h"

#ifdef _WIN32
#include <core/utils/stringmanip.h>

namespace Carrot::IO {
    PlatformFileHandle* PlatformFileHandle::open(const std::filesystem::path& _path, OpenMode openMode) {
        const auto& path = _path; // TODO: systematically normalize, canonize and add \\?\ prefix
        DWORD flags = FILE_ATTRIBUTE_NORMAL;
        DWORD desiredAccess = 0;
        DWORD creationDisposition = 0;
        DWORD shareMode = 0;
        switch(openMode) {
            case OpenMode::Read:
                creationDisposition = OPEN_EXISTING;
                desiredAccess = GENERIC_READ;
                shareMode = FILE_SHARE_READ;
                break;

            case OpenMode::NewReadWrite:
                desiredAccess = GENERIC_WRITE;
                creationDisposition = CREATE_NEW;
                break;

            case OpenMode::AlreadyExistingReadWrite:
                desiredAccess = GENERIC_WRITE;
                creationDisposition = OPEN_EXISTING;
                break;

            case OpenMode::Write:
                desiredAccess = GENERIC_WRITE;
                creationDisposition = TRUNCATE_EXISTING;
                break;

            case OpenMode::Append:
                desiredAccess = GENERIC_WRITE;
                creationDisposition = OPEN_EXISTING;
                break;

            case OpenMode::Invalid:
            default:
                verify(false, "Invalid parameter");
                break;
        }
        HANDLE result = CreateFile(Carrot::toString(path.u8string()).c_str(), desiredAccess, shareMode, nullptr, creationDisposition, flags, NULL);
        if(result != INVALID_HANDLE_VALUE) {
            PlatformFileHandle* fileHandle = new PlatformFileHandle;
            fileHandle->handle = result;
            return fileHandle;
        } else {
            throw std::filesystem::filesystem_error("Could not open", path, std::error_code{ static_cast<int>(GetLastError()), std::system_category() });
        }
    }

    void PlatformFileHandle::close() {
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }

    void PlatformFileHandle::write(std::span<const std::uint8_t> data) {
        verify(handle != INVALID_HANDLE_VALUE, "File is not open!");
        DWORD bytesWritten;
        bool result = WriteFile(handle, data.data(), data.size(), &bytesWritten, nullptr);
        if(!result) {
            throw std::filesystem::filesystem_error("Could not write to file", std::error_code{ static_cast<int>(GetLastError()), std::system_category() });
        } else if(bytesWritten != data.size()) {
            throw std::filesystem::filesystem_error("Could not write all bytes to file", std::error_code{ static_cast<int>(GetLastError()), std::system_category() });
        }
    }

    void PlatformFileHandle::seek(std::int64_t position, int seekDirection) {
        LARGE_INTEGER offset;
        offset.QuadPart = position;

        bool result = false;
        switch(seekDirection) {
            case SEEK_CUR:
                result = SetFilePointerEx(handle, offset, nullptr, FILE_CURRENT);
                break;
            case SEEK_SET:
                result = SetFilePointerEx(handle, offset, nullptr, FILE_BEGIN);
                break;
            case SEEK_END:
                result = SetFilePointerEx(handle, offset, nullptr, FILE_END);
                break;
            default:
                verify(false, "Unsupported seek operation");
                break;
        }
        if(!result) {
            throw std::filesystem::filesystem_error("Could not seek", std::error_code{ static_cast<int>(GetLastError()), std::system_category() });
        }
    }

    void PlatformFileHandle::read(std::span<std::uint8_t> data) const {
        verify(handle != INVALID_HANDLE_VALUE, "File is not open!");

        verify(data.size_bytes() < (std::numeric_limits<DWORD>::max)(), Carrot::sprintf("Cannot read more than %llu bytes at once (platform limitation)", data.size_bytes()));
        DWORD readCount;
        bool result = ReadFile(handle, data.data(), data.size(), &readCount, nullptr);
        if(!result) {
            throw std::filesystem::filesystem_error("Could not queue read", std::error_code{ static_cast<int>(GetLastError()), std::system_category() });
        } else if(readCount != data.size_bytes()) {
            throw std::filesystem::filesystem_error("Could not read all bytes", std::error_code{ static_cast<int>(GetLastError()), std::system_category() });
        }
    }


    std::uint64_t PlatformFileHandle::tell() const {
        LARGE_INTEGER toMove;
        toMove.QuadPart = 0;

        LARGE_INTEGER position;
        if(!SetFilePointerEx(handle, toMove, &position, FILE_CURRENT)) {
            throw std::filesystem::filesystem_error("Could not queue read", std::error_code{ static_cast<int>(GetLastError()), std::system_category() });
        }

        return position.QuadPart;
    }

    PlatformFileHandle::~PlatformFileHandle() {
        if(handle != INVALID_HANDLE_VALUE) {
            close();
        }
    }
} // Carrot::IO

#endif // _WIN32