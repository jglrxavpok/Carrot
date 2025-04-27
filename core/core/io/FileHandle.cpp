//
// Created by jglrxavpok on 11/06/2021.
//

#include "FileHandle.h"
#include <stdexcept>
#include <cassert>
#include "core/utils/Assert.h"
#include "core/utils/stringmanip.h"
#include "core/io/IO.h"


#ifdef _WIN32
#include "windows/PlatformFileHandle.h"
#elifdef __linux__
#include "linux/PlatformFileHandle.h"
#else
#error No implementation of PlatformFileHandle for this platform.
#endif


namespace Carrot::IO {
    template<typename T>
    concept IsPlatformFileFormatValid = requires(T t, OpenMode m, const std::filesystem::path& filename, std::int64_t seek, int seekDirection, std::span<const std::uint8_t> constdata, std::span<std::uint8_t> data) {
        { T::open(filename, m) } -> std::convertible_to<PlatformFileHandle*>; // opens a file, throws if error
        { t.close() } -> std::convertible_to<void>; // closes a file
        { t.read(data) } -> std::convertible_to<void>; // fills the span with the data, advances cursor, throws if error
        { t.write(constdata) } -> std::convertible_to<void>; // fills the file with the data, advances cursor, throws if error
        { t.seek(seek, seekDirection) } -> std::convertible_to<void>; // fills the file with the data, advances cursor, throws if error
        { t.tell() } -> std::convertible_to<std::uint64_t>; // where is the cursor?
    };

    static_assert(IsPlatformFileFormatValid<PlatformFileHandle>, "Platform file handle for this configuration is not valid.");

#define PLATFORM_HANDLE ((PlatformFileHandle*)handle)

    FileHandle::FileHandle(const std::filesystem::path& filepath, OpenMode openMode) {
        open(filepath, openMode);
    }

    FileHandle::FileHandle(const std::string& filename, OpenMode openMode) {
        open(std::filesystem::path{ filename }, openMode);
    }

    FileHandle::FileHandle(const char* const filename, OpenMode openMode) {
        open(std::filesystem::path{ filename }, openMode);
    }

    FileHandle::FileHandle(FileHandle&& toMove) {
        handle = toMove.handle;
        opened = toMove.opened;
        fileSize = toMove.fileSize;
        currentOpenMode = toMove.currentOpenMode;
        currentFilename = std::move(toMove.currentFilename);

        toMove.opened = false;
        toMove.handle = nullptr;
        toMove.fileSize = 0;
        toMove.currentOpenMode = OpenMode::Invalid;
    }

    void FileHandle::open(const std::filesystem::path& filename, OpenMode openMode) {
        currentFilename = filename;
        handle = PlatformFileHandle::open(filename, openMode);
        opened = true;
        currentOpenMode = openMode;

        seekEnd();
        fileSize = getCurrentPosition();
        seek(0);
    }

    FileHandle::~FileHandle() {
        if(opened)
            close();
    }

    void FileHandle::close() {
        assert(opened);
        currentFilename = "";
        currentOpenMode = OpenMode::Invalid;
        PLATFORM_HANDLE->close();
        delete PLATFORM_HANDLE;
        handle = nullptr;
    }

    void FileHandle::seek(size_t position) {
        assert(opened);
        PLATFORM_HANDLE->seek(position, SEEK_SET);
    }

    void FileHandle::seekEnd() {
        assert(opened);
        PLATFORM_HANDLE->seek(0, SEEK_END);
    }

    void FileHandle::skip(size_t bytes) {
        assert(opened);
        PLATFORM_HANDLE->seek(bytes, SEEK_CUR);
    }

    void FileHandle::rewind(size_t bytes) {
        assert(opened);
        PLATFORM_HANDLE->seek(-bytes, SEEK_CUR);
    }

    uint64_t FileHandle::getCurrentPosition() const {
        assert(opened);
        return PLATFORM_HANDLE->tell();
    }

    uint64_t FileHandle::getSize() const {
        assert(opened);
        return fileSize;
    }

    const std::filesystem::path& FileHandle::getCurrentFilename() const {
        assert(opened);
        return currentFilename;
    }

    void FileHandle::write(std::span<const uint8_t> toWrite, uint64_t offset) {
        assert(opened);
        assert(isWriteableMode(currentOpenMode));
        uint64_t previousPosition = getCurrentPosition();
        seek(offset);
        PLATFORM_HANDLE->write(toWrite);
        seek(previousPosition);
    }

    void FileHandle::read(void* buffer, uint64_t size, uint64_t offset) {
        verify(size + offset <= getSize(), "Out-of-bounds!");
        assert(opened);
        assert(isReadableMode(currentOpenMode));
        uint64_t previousPosition = getCurrentPosition();
        seek(offset);
        PLATFORM_HANDLE->read(std::span{ (std::uint8_t*)buffer, size });
        seek(previousPosition);
    }

    std::unique_ptr<uint8_t[]> FileHandle::read(uint64_t size, uint64_t offset) {
        auto ptr = std::make_unique<uint8_t[]>(size);
        read(ptr.get(), size, offset);
        return std::move(ptr);
    }

    std::unique_ptr<FileHandle> FileHandle::copyReadable() const {
        auto result = std::make_unique<FileHandle>();
        if(opened) {
            assert(isReadableMode(currentOpenMode));
            result->open(currentFilename, currentOpenMode);
            result->seek(getCurrentPosition());
        }
        return std::move(result);
    }

    void FileHandle::readAll(void* buffer) {
        read(buffer, getSize());
    }

    std::unique_ptr<uint8_t[]> FileHandle::readAll() {
        return read(getSize());
    }
}
