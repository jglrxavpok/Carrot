//
// Created by jglrxavpok on 11/06/2021.
//

#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <memory>
#include "FileHandle.h"
#include "vfs/VirtualFileSystem.h"

namespace Carrot::IO {
    class VirtualFileSystem;

    /**
     * Represents a read-only file that can be on disk, or in memory
     */
    class Resource {
    public:
        static VirtualFileSystem* vfsToUse;

        /// Creates empty resource
        explicit Resource();

        Resource(const char* path);
        Resource(const std::string& path);
        Resource(const VFS::Path& path);

        /// Copies data inside a shared vector
        explicit Resource(const std::vector<std::uint8_t>& data);

        /// Creates an in-memory resource, without copying the data, but moving it instead
        explicit Resource(std::vector<std::uint8_t>&& data);

        explicit Resource(const std::span<const std::uint8_t>& data);

        /// Copies the other resource. Raw data is shared, file handles are cloned.
        Resource(const Resource& toCopy);
        Resource(Resource&& toMove);

    public:
        /**
         * If this resource represents a disk file, tries to open the file.
         * Subsequent file reads will use the opened handle instead of reopening the file each time.
         * Throws on errors.
         *
         * If this resources represents a memory file, does nothing.
         */
        void open();

        /**
         * If this resource represents a disk file, closes the file.
         * Subsequent file reads will thefore reopen the file each time.
         * Throws on errors.
         *
         * If this resources represents a memory file, does nothing.
         */
        void close();

    public:
        /// Constructs an in-memory resource with the given text
        static Carrot::IO::Resource inMemory(const std::string& text);

    public:
        bool isFile() const;
        uint64_t getSize() const;
        const std::string& getName() const;

    public:
        /**
         * Reads some data from this resource. If the resource was already open, will reuse the file handle
         * Throws if the buffer+offset try to access data out of bounds of this resource
         */
        void read(std::span<std::uint8_t> buffer, uint64_t offset = 0) const;

        /**
         * Reads some data from this resource. If the resource was already open, will reuse the file handle
         * Throws if the size+offset try to access data out of bounds of this resource
         */
        std::unique_ptr<uint8_t[]> read(uint64_t size, uint64_t offset = 0) const;

        /**
         * Reads all data from this resource. If the resource was already open, will reuse the file handle
         * Make sure there is enough space inside the buffer
         */
        void readAll(void* buffer) const;

        /**
         * Reads all data from this resource. If the resource was already open, will reuse the file handle
         */
        std::unique_ptr<uint8_t[]> readAll() const;

        /**
         * 'readAll' + conversion to std::string
         */
        std::string readText() const;

    public:
        /// Copies this resource to a new in-memory Resource.
        /// For files, this reads the entire file to memory
        /// For in-memory resources, this copies to another memory are inside the heap
        [[nodiscard]] Carrot::IO::Resource copyToMemory() const;

        /// Gets a relative resource based on this resource path.
        /// If this resource is memory-based, gets the file with the corresponding name
        /// If the path given is absolute, the file will instead use that absolute path
        Carrot::IO::Resource relative(const std::filesystem::path& path) const;

    public:
        void name(const std::filesystem::path& _filename, const std::string& _name);

    public:
        Resource& operator=(const Resource& toCopy);
        Resource& operator=(Resource&& toMove);

    public:
        /// Resources are equal if they represent the same data: same filename, or same pointer to shared data (not data equality!)
        bool operator==(const Resource& rhs) const;
        bool operator!=(const Resource& rhs) const {
            return !(*this == rhs);
        }

    private:
        struct Data {
            bool isRawData;
            union {
                std::shared_ptr<std::vector<uint8_t>> raw = nullptr;
                std::unique_ptr<Carrot::IO::FileHandle> fileHandle;
            };

            std::size_t fileSize = 0;

            explicit Data(bool isRawData);
            Data(Data&& toMove);

            Data& operator=(const Data& toCopy);
            Data& operator=(Data&& toMove);

            ~Data();
        } data;

        std::string debugName;
        std::filesystem::path filename;
    };
}
