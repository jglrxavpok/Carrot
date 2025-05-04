//
// Created by jglrxavpok on 24/04/25.
//

#include "PlatformFileHandle.h"

#ifdef __linux__
namespace Carrot::IO {

    PlatformFileHandle* PlatformFileHandle::open(const std::filesystem::path& path, OpenMode openModeEnum) {
        const char* openMode = "";
        switch (openModeEnum) {
        case OpenMode::AlreadyExistingReadWrite:
            openMode = "r+b";
            break;

        case OpenMode::NewOrExistingReadWrite:
        case OpenMode::NewReadWrite:
            // TODO: check if file already exists?
            openMode = "w+b";
            break;

        case OpenMode::Append:
            openMode = "a";
            break;

        case OpenMode::Read:
            openMode = "rb";
            break;

        case OpenMode::Write:
            openMode = "wb";
            break;

        default:
            TODO; // missing case
        }
        FILE* f = fopen64(path.c_str(), openMode);
        if (!f) {
            std::error_code ec; // TODO
            throw std::filesystem::filesystem_error("Failed to open file", path, ec);
        }

        auto* pFile = new PlatformFileHandle{};
        pFile->file = f;
        return pFile;
    }

    void PlatformFileHandle::seek(std::int64_t position, int seekDirection) {
        verify(file, "File is not open!");
        fseeko64(file, position, seekDirection);
    }

    void PlatformFileHandle::read(std::span<std::uint8_t> data) const {
        verify(file, "File is not open!");
        fread(data.data(), data.size(), 1, file);
    }

    void PlatformFileHandle::write(std::span<const std::uint8_t> data) {
        verify(file, "File is not open!");
        fwrite(data.data(), data.size(), 1, file);
    }

    std::uint64_t PlatformFileHandle::tell() const {
        verify(file, "File is not open!");
        return ftell(file);
    }

    void PlatformFileHandle::close() {
        fclose(file);
        file = nullptr;
    }

    PlatformFileHandle::~PlatformFileHandle() {
        if (file) {
            fclose(file);
        }
    }


}
#endif