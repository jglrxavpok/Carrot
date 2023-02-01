//
// Created by jglrxavpok on 11/06/2021.
//

#pragma once
#include <filesystem>
#include <cstdio>
#include <memory>
#include <span>
#include <string>
#include <stdexcept>

namespace Carrot::IO {
    enum class OpenMode {
        Read,
        NewReadWrite,
        AlreadyExistingReadWrite,
        Write,
        Append,

        Invalid,
    };

    inline bool isWriteableMode(OpenMode mode) {
        switch (mode) {
            case OpenMode::Read:
                return false;
            case OpenMode::NewReadWrite:
            case OpenMode::AlreadyExistingReadWrite:
            case OpenMode::Write:
            case OpenMode::Append:
                return true;

            default: {}
        }
        throw std::runtime_error("Invalid open mode");
    }

    inline bool isReadableMode(OpenMode mode) {
        switch (mode) {
            case OpenMode::Write:
                return false;
            case OpenMode::NewReadWrite:
            case OpenMode::AlreadyExistingReadWrite:
            case OpenMode::Read:
            case OpenMode::Append:
                return true;

            default: {}
        }
        throw std::runtime_error("Invalid open mode");
    }

    class FileHandle {
    public:
        explicit FileHandle() = default;
        explicit FileHandle(const std::filesystem::path& filename, OpenMode openMode);
        explicit FileHandle(const std::string filename, OpenMode openMode);
        explicit FileHandle(const char* const filename, OpenMode openMode);
        FileHandle(FileHandle&& toMove);
        FileHandle(const FileHandle&) = delete;

        ~FileHandle();

        std::unique_ptr<FileHandle> copyReadable() const;

    public:
        bool isOpen() const {
            return opened;
        }

        void open(const std::string& filename, OpenMode mode);
        void close();
        void seek(size_t position);
        void seekEnd();
        void skip(size_t bytes);
        void rewind(size_t bytes);

        uint64_t getCurrentPosition() const;
        uint64_t getSize() const;
        const std::string& getCurrentFilename() const;

    public:
        void write(const std::span<const uint8_t> toWrite, uint64_t offset = 0);
        void read(void* buffer, uint64_t size, uint64_t offset = 0);
        std::unique_ptr<uint8_t[]> read(uint64_t size, uint64_t offset = 0);

        void readAll(void* buffer);
        std::unique_ptr<uint8_t[]> readAll();

    private:
        void checkStdError(int err, const std::string& operation);

    private:
        std::FILE* handle = nullptr;
        bool opened = false;
        uint64_t fileSize = 0;
        OpenMode currentOpenMode = OpenMode::Invalid;
        std::string currentFilename = "";
    };
}
