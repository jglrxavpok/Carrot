//
// Created by jglrxavpok on 06/05/2022.
//

#include "AssimpCompatibilityLayer.h"
#include "engine/utils/Profiling.h"
#include "core/io/Logging.hpp"

namespace Carrot::IO {
    CarrotIOStream::CarrotIOStream(Carrot::IO::FileHandle&& file) : file(std::move(file)) {

    }

    size_t CarrotIOStream::Read(void *pvBuffer, size_t pSize, size_t pCount) {
        std::size_t toRead = pSize * pCount;
        if (file.getCurrentPosition() + toRead > file.getSize()) {
            toRead = file.getSize() - file.getCurrentPosition();
        }
        file.read(pvBuffer, pSize * pCount);
        return toRead;
    }

    aiReturn CarrotIOStream::Seek(size_t pOffset, aiOrigin pOrigin) {
        std::size_t seekCursor = 0;
        switch (pOrigin) {
            case aiOrigin_SET:
                seekCursor = pOffset;
                break;
            case aiOrigin_CUR:
                seekCursor += pOffset;
                break;
            case aiOrigin_END:
                static_assert(sizeof(size_t) == sizeof(std::int64_t));
                seekCursor = file.getSize() + (std::bit_cast<std::int64_t>(pOffset));
                break;
        }
        if (seekCursor > file.getSize()) {
            return aiReturn_FAILURE;
        }
        file.seek(seekCursor);
        return aiReturn_SUCCESS;
    }

    size_t CarrotIOStream::Tell() const {
        return seekCursor;
    }

    size_t CarrotIOStream::FileSize() const {
        return file.getSize();
    }

    size_t CarrotIOStream::Write(const void *pvBuffer, size_t pSize, size_t pCount) {
        verify(false, "Unsupported operation");
    }

    void CarrotIOStream::Flush() {
        verify(false, "Unsupported operation");
    }

    // --

    CarrotIOSystem::CarrotIOSystem(const Carrot::IO::Resource& sourceResource) {
        verify(sourceResource.isFile(), "Source resource must be a file.");
        auto vfsPath = VFS::Path(sourceResource.getName());
        vfsRoot = vfsPath.getRoot();
    }

    CarrotIOSystem::~CarrotIOSystem() = default;

    std::optional<VFS::Path> CarrotIOSystem::getPath(const char* file) const {
        try {
            VFS::Path path = VFS::Path(file);
            if(path.isGeneric()) {
                return VFS::Path(vfsRoot, file);
            }
            return path;
        } catch(const std::invalid_argument&) {
            return std::optional<VFS::Path>();
        }
    }

    bool CarrotIOSystem::Exists(const char *pFile) const {
        auto p = getPath(pFile);
        if(!p.has_value()) {
            return false;
        }
        return GetVFS().exists(p.value());
    }

    char CarrotIOSystem::getOsSeparator() const {
        return IO::Path::Separator;
    }

    Assimp::IOStream *CarrotIOSystem::Open(const char *pFile, const char *pMode) {
        ZoneScoped;
        auto p = getPath(pFile);
        if(!p.has_value()) {
            return nullptr;
        }
        std::filesystem::path fullPath = GetVFS().resolve(p.value());
        if(fullPath.empty()) {
            return nullptr;
        }
        if(!std::filesystem::exists(fullPath)) {
            return nullptr;
        }
        verify(strcmp(pMode, "rb") == 0, "Does not support anything else than binary reads.");
        return new CarrotIOStream(std::move(FileHandle(fullPath, OpenMode::Read)));
    }

    void CarrotIOSystem::Close(Assimp::IOStream *pFile) {
        delete pFile;
    }

    bool CarrotIOSystem::ComparePaths(const char *one, const char *second) const {
        auto p1 = getPath(one);
        if(!p1.has_value()) {
            return false;
        }
        auto p2 = getPath(second);
        if(!p2.has_value()) {
            return false;
        }
        return GetVFS().resolve(p1.value()) == GetVFS().resolve(p2.value());
    }
}