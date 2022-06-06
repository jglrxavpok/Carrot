//
// Created by jglrxavpok on 11/06/2021.
//

#include "FileHandle.h"
#include <stdexcept>
#include <cassert>
#include "core/utils/Assert.h"
#include "core/utils/stringmanip.h"

namespace Carrot::IO {
    FileHandle::FileHandle(const std::filesystem::path& filepath, OpenMode openMode) {
        open(Carrot::toString(filepath.u8string()), openMode);
    }

    FileHandle::FileHandle(const std::string filename, OpenMode openMode) {
        open(filename, openMode);
    }

    FileHandle::FileHandle(const char* const filename, OpenMode openMode) {
        open(filename, openMode);
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

    void FileHandle::open(const std::string& filename, OpenMode openMode) {
        const char* mode = "";
        switch (openMode) {
            case OpenMode::Read:
                mode = "rb";
                break;
            case OpenMode::Write:
                mode = "wb";
                break;
            case OpenMode::Append:
                mode = "ab";
                break;
            case OpenMode::AlreadyExistingReadWrite:
                mode = "r+b";
                break;
            case OpenMode::NewReadWrite:
                mode = "w+b";
                break;
            case OpenMode::Invalid:
                throw std::runtime_error("Cannot open file with mode 'Invalid'");
        }
        currentFilename = filename;
        checkStdError(fopen_s(&handle, filename.c_str(), mode), "open");
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
        checkStdError(fclose(handle), "close");
        handle = nullptr;
    }

    void FileHandle::seek(size_t position) {
        assert(opened);
        checkStdError(fseek(handle, position, SEEK_SET), "seek");
    }

    void FileHandle::seekEnd() {
        assert(opened);
        checkStdError(fseek(handle, 0, SEEK_END), "seekEnd");
    }

    void FileHandle::skip(size_t bytes) {
        assert(opened);
        checkStdError(fseek(handle, bytes, SEEK_CUR), "skip");
    }

    void FileHandle::rewind(size_t bytes) {
        assert(opened);
        checkStdError(fseek(handle, -bytes, SEEK_CUR), "rewind");
    }

    uint64_t FileHandle::getCurrentPosition() const {
        assert(opened);
        return ftell(handle);
    }

    uint64_t FileHandle::getSize() const {
        assert(opened);
        return fileSize;
    }

    const std::string& FileHandle::getCurrentFilename() const {
        assert(opened);
        return currentFilename;
    }

    void FileHandle::write(const std::span<uint8_t> toWrite, uint64_t offset) {
        assert(opened);
        assert(isWriteableMode(currentOpenMode));
        uint64_t previousPosition = getCurrentPosition();
        seek(offset);
        auto written = fwrite(toWrite.data(), sizeof(uint8_t), toWrite.size(), handle);
        verify(written == toWrite.size(), "write error");
        seek(previousPosition);
    }

    void FileHandle::read(void* buffer, uint64_t size, uint64_t offset) {
        verify(size + offset <= getSize(), "Out-of-bounds!");
        assert(opened);
        assert(isReadableMode(currentOpenMode));
        uint64_t previousPosition = getCurrentPosition();
        seek(offset);
        auto read = fread(buffer, sizeof(uint8_t), size, handle);
        checkStdError(ferror(handle), "read");
        verify(read == size, "hit unexpected EOF");
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

    void FileHandle::checkStdError(int err, const std::string& operation) {
        if(err) {
            throw std::runtime_error("Failed to perform '" + operation + "', error code is " + std::to_string(err) + ", " + currentFilename);
        }
    }
}
