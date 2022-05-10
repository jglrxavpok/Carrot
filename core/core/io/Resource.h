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
        /// Constructs an in-memory resource with the given text
        static Carrot::IO::Resource inMemory(const std::string& text);

    public:
        bool isFile() const;
        uint64_t getSize() const;
        const std::string& getName() const;

    public:
        void write(const std::span<uint8_t> toWrite, uint64_t offset = 0);
        void read(void* buffer, uint64_t size, uint64_t offset = 0) const;
        std::unique_ptr<uint8_t[]> read(uint64_t size, uint64_t offset = 0) const;
        void readAll(void* buffer) const;
        std::unique_ptr<uint8_t[]> readAll() const;

        std::string readText() const;

    public:
        void writeToFile(const std::string& filename, uint64_t offset = 0) const;
        void writeToFile(FileHandle& file, uint64_t offset = 0) const;

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
        void name(const std::string& name);

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

            explicit Data(bool isRawData);
            Data(Data&& toMove);

            Data& operator=(Data&& toMove);

            ~Data();
        } data;

        std::string filename;
    };
}
