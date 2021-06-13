//
// Created by jglrxavpok on 11/06/2021.
//

#pragma once
#include <string>
#include <vector>
#include <memory>
#include "FileHandle.h"

namespace Carrot::IO {
    class Resource {
    public:
        /// Creates empty resource
        explicit Resource();

        Resource(const std::string filename);
        Resource(const char* const filename);

        /// Copies data inside a shared vector
        explicit Resource(const std::vector<std::uint8_t>& data);

        /// Copies data inside a shared vector
        explicit Resource(const std::span<std::uint8_t>& data);

        /// Copies the other resource. Raw data is shared, file handles are cloned.
        Resource(const Resource& toCopy);

    public:
        uint64_t getSize() const;

    public:
        void write(const std::span<uint8_t> toWrite, uint64_t offset = 0);
        void read(void* buffer, uint64_t size, uint64_t offset = 0) const;
        std::unique_ptr<uint8_t[]> read(uint64_t size, uint64_t offset = 0) const;
        void readAll(void* buffer) const;
        std::unique_ptr<uint8_t[]> readAll() const;

    public:
        void writeToFile(const std::string& filename, uint64_t offset = 0) const;
        void writeToFile(FileHandle& file, uint64_t offset = 0) const;

    public:
        Resource& operator=(const Resource& toCopy);

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

            ~Data();
        } data;
    };
}
