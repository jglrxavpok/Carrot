//
// Created by jglrxavpok on 11/06/2021.
//

#include "Resource.h"
#include "engine/utils/Assert.h"

namespace Carrot::IO {
    Resource::Resource(): data(true) {
        data.raw = std::make_shared<std::vector<std::uint8_t>>();
    }

    Resource::Resource(const char *const filename): data(false) {
        data.fileHandle = std::make_unique<FileHandle>(filename, OpenMode::Read);
    }

    Resource::Resource(const std::string filename): data(false) {
        data.fileHandle = std::make_unique<FileHandle>(filename, OpenMode::Read);
    }

    Resource::Resource(const std::vector<std::uint8_t>& data): data(true) {
        auto container = std::make_shared<std::vector<std::uint8_t>>(data.size());
        std::memcpy(container->data(), data.data(), data.size());
        this->data.raw = container;
    }

    Resource::Resource(const Resource& toCopy): data(toCopy.data.isRawData) {
        if(data.isRawData) {
            data.raw = toCopy.data.raw;
        } else {
            data.fileHandle = toCopy.data.fileHandle->copyReadable();
        }
    }

    Resource& Resource::operator=(const Resource& toCopy) {
        if(data.isRawData) {
            data.raw = nullptr;
        } else {
            data.fileHandle = nullptr;
        }

        data.isRawData = toCopy.data.isRawData;

        if(data.isRawData) {
            data.raw = toCopy.data.raw;
        } else {
            data.fileHandle = toCopy.data.fileHandle->copyReadable();
        }
        return *this;
    }

    bool Resource::operator==(const Resource& rhs) const {
        if(data.isRawData != rhs.data.isRawData)
            return false;

        if(data.isRawData) {
            return data.raw == rhs.data.raw;
        } else {
            return data.fileHandle->getCurrentFilename() == rhs.data.fileHandle->getCurrentFilename();
        }
    }

    uint64_t Resource::getSize() const {
        if(data.isRawData) {
            return data.raw->size();
        } else {
            return data.fileHandle->getSize();
        }
    }

    void Resource::write(const std::span<uint8_t> toWrite, uint64_t offset) {
        runtimeAssert(toWrite.size() + offset <= getSize(), "Overflow!");
        if(data.isRawData) {
            auto& vec = *data.raw;
            std::memcpy(vec.data(), toWrite.data() + offset, toWrite.size());
        } else {
            data.fileHandle->write(toWrite, offset);
        }
    }

    void Resource::read(void* buffer, uint64_t size, uint64_t offset) const {
        runtimeAssert(size + offset <= getSize(), "Out-of-bounds!");
        if(data.isRawData) {
            std::memcpy(buffer, data.raw->data() + offset, size);
        } else {
            data.fileHandle->read(buffer, size, offset);
        }
    }

    std::unique_ptr<uint8_t[]> Resource::read(uint64_t size, uint64_t offset) const {
        auto ptr = std::make_unique<uint8_t[]>(size);
        read(ptr.get(), size, offset);
        return std::move(ptr);
    }

    void Resource::writeToFile(const std::string& filename, uint64_t offset) const {
        FileHandle handle(filename, OpenMode::Write);
        writeToFile(handle, offset);
    }

    void Resource::writeToFile(FileHandle& file, uint64_t offset) const {
        if(data.isRawData) {
            // TODO: resize?
            file.write({data.raw->data(), getSize()}, offset);
        } else {
            // perform copy
            auto contents = data.fileHandle->read(getSize(), offset);
            file.write({contents.get(), getSize()}, offset);
        }
    }

    void Resource::readAll(void* buffer) const {
        read(buffer, getSize());
    }

    std::unique_ptr<uint8_t[]> Resource::readAll() const {
        return read(getSize());
    }

    Resource::Data::Data(bool isRawData): isRawData(isRawData) {
    }

    Resource::Data::~Data() {
        if(isRawData) {
            raw = nullptr;
        } else {
            fileHandle = nullptr;
        }
    }
}
