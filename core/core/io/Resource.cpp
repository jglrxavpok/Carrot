//
// Created by jglrxavpok on 11/06/2021.
//

#include "Resource.h"
#include <cstring>
#include "core/utils/Assert.h"
#include "core/utils/stringmanip.h"

namespace Carrot::IO {
    VirtualFileSystem* Resource::vfsToUse = nullptr;

    Resource::Resource(): data(true) {
        data.raw = std::make_shared<std::vector<std::uint8_t>>();
    }

    Resource::Resource(const char* path): Resource::Resource(VFS::Path(path)) {}
    Resource::Resource(const std::string& path): Resource::Resource(VFS::Path(path)) {}

    Resource::Resource(const VFS::Path& path): data(false) {
        std::filesystem::path fullPath;
        if(vfsToUse != nullptr) {
            fullPath = vfsToUse->resolve(path);
        } else {
            fullPath = path.toString();
        }
        name(fullPath, path.toString());

        if(!std::filesystem::exists(fullPath)) {
            throw std::filesystem::filesystem_error("File does not exist", fullPath, std::error_code{ 1, std::system_category() });
        }
        data.fileSize = std::filesystem::file_size(fullPath);
    }

    Resource::Resource(const std::vector<std::uint8_t>& data): data(true) {
        auto container = std::make_shared<std::vector<std::uint8_t>>(data.size());
        std::memcpy(container->data(), data.data(), data.size());
        this->data.raw = container;
        name("", std::string("RawData <")+std::to_string((std::uint64_t)data.data())+", "+std::to_string(data.size())+">");
    }

    Resource::Resource(std::vector<std::uint8_t>&& dataToMove): data(true) {
        this->data.raw = std::make_shared<std::vector<std::uint8_t>>();
        *this->data.raw = std::move(dataToMove);
        name("", std::string("RawData <")+std::to_string((std::uint64_t)data.raw->data())+", "+std::to_string(data.raw->size())+">");
    }

    Resource::Resource(const std::span<const std::uint8_t>& data): data(true) {
        auto container = std::make_shared<std::vector<std::uint8_t>>(data.size());
        std::memcpy(container->data(), data.data(), data.size());
        this->data.raw = container;
        name("", std::string("RawData <")+std::to_string((std::uint64_t)data.data())+", "+std::to_string(data.size())+">");
    }

    Resource::Resource(const Resource& toCopy): data(toCopy.data.isRawData) {
        data = toCopy.data;
        name(toCopy.filename, toCopy.debugName);
    }

    Resource::Resource(Resource&& toMove): data(std::move(toMove.data)) {
        filename = std::move(toMove.filename);
        debugName = std::move(toMove.debugName);
    }

    void Resource::open() {
        if(!data.isRawData) {
            if(data.fileHandle) {
                return; // already open
            }

            data.fileHandle = std::make_unique<FileHandle>(filename, OpenMode::Read);
        }
    }

    void Resource::close() {
        if(!data.isRawData) {
            verify(data.fileHandle, "File was not open");
            data.fileHandle = nullptr;
        }
    }

    Resource& Resource::operator=(Resource&& toMove) {
        data = std::move(toMove.data);
        filename = std::move(toMove.filename);
        debugName = std::move(toMove.debugName);
        return *this;
    }

    Resource& Resource::operator=(const Resource& toCopy) {
        if(data.isRawData) {
            data.raw = nullptr;
        } else {
            data.fileHandle = nullptr;
        }

        data.isRawData = toCopy.data.isRawData;
        filename = toCopy.filename;
        debugName = toCopy.debugName;

        if(data.isRawData) {
            data.raw = toCopy.data.raw;
        } else {
            if(toCopy.data.fileHandle) {
                data.fileHandle = toCopy.data.fileHandle->copyReadable();
            }
        }
        return *this;
    }

    bool Resource::operator==(const Resource& rhs) const {
        if(data.isRawData != rhs.data.isRawData)
            return false;

        if(data.isRawData) {
            return data.raw == rhs.data.raw;
        } else {
            return filename == rhs.filename;
        }
    }

    uint64_t Resource::getSize() const {
        if(data.isRawData) {
            return data.raw->size();
        } else {
            return data.fileSize;
        }
    }

    void Resource::read(std::span<std::uint8_t> buffer, uint64_t offset) const {
        verify(buffer.size_bytes() + offset <= getSize(), "Out-of-bounds!");
        if(data.isRawData) {
            std::memcpy(buffer.data(), data.raw->data() + offset, buffer.size_bytes());
        } else {
            const bool opened = data.fileHandle != nullptr;

            FileHandle* handle;
            if(opened) {
                handle = data.fileHandle.get();
            } else {
                handle = new FileHandle(filename, OpenMode::Read);
            }
            handle->read(buffer.data(), buffer.size_bytes(), offset);

            if(!opened) {
                delete handle;
            }
        }
    }

    std::unique_ptr<uint8_t[]> Resource::read(uint64_t size, uint64_t offset) const {
        auto ptr = std::make_unique<uint8_t[]>(size);
        read(std::span { (std::uint8_t*)ptr.get(), size }, offset);
        return std::move(ptr);
    }

    void Resource::readAll(void* buffer) const {
        read(std::span { (std::uint8_t*)buffer, getSize() });
    }

    std::unique_ptr<uint8_t[]> Resource::readAll() const {
        return read(getSize());
    }

    std::string Resource::readText() const {
        auto buffer = readAll();
        const char* str = reinterpret_cast<const char *>(buffer.get());
        std::string result;
        result.resize(getSize());
        std::memcpy(result.data(), str, getSize());
        return result;
    }

    void Resource::name(const std::filesystem::path& _filename, const std::string& _name) {
        filename = _filename;
        debugName = _name;
    }

    const std::string& Resource::getName() const {
        return debugName;
    }

    Carrot::IO::Resource Resource::inMemory(const std::string& text) {
        std::vector<std::uint8_t> vec;
        vec.resize(text.size());
        std::memcpy(vec.data(), text.data(), text.size());
        return Carrot::IO::Resource(vec);
    }

    Carrot::IO::Resource Resource::copyToMemory() const {
        std::vector<std::uint8_t> dataCopy;
        dataCopy.resize(getSize());
        readAll(dataCopy.data());
        return Resource(std::move(dataCopy));
    }

    bool Resource::isFile() const {
        return !data.isRawData;
    }

    Carrot::IO::Resource Resource::relative(const std::filesystem::path& path) const {
        if(!isFile() || path.is_absolute()) {
            return Resource{ VFS::Path(path.string()) };
        }
        std::filesystem::path vfsPath = debugName;
        vfsPath = vfsPath.parent_path();
        vfsPath /= path;
        return Resource{ VFS::Path(vfsPath.string()) };
    }

    Resource::Data::Data(bool isRawData): isRawData(isRawData) {}

    Resource::Data::Data(Resource::Data&& toMove) {
        *this = std::move(toMove);
    }

    Resource::Data& Resource::Data::operator=(Resource::Data&& toMove) {
        bool wasRawData = isRawData;
        isRawData = toMove.isRawData;

        fileSize = toMove.fileSize;
        toMove.fileSize = 0;

        if(wasRawData && !isRawData) {
            raw = nullptr;
        } else if(!wasRawData && isRawData) {
            fileHandle = nullptr;
        }
        if(isRawData) {
            raw = std::move(toMove.raw);
        } else {
            fileHandle = std::move(toMove.fileHandle);
        }
        return *this;
    }

    Resource::Data& Resource::Data::operator=(const Resource::Data& toCopy) {
        bool wasRawData = isRawData;
        isRawData = toCopy.isRawData;

        fileSize = toCopy.fileSize;

        if(wasRawData && !isRawData) {
            raw = nullptr;
        } else if(!wasRawData && isRawData) {
            fileHandle = nullptr;
        }
        if(isRawData) {
            raw = toCopy.raw;
        } else {
            if(toCopy.fileHandle) {
                fileHandle = toCopy.fileHandle->copyReadable();
            }
        }
        return *this;
    }

    Resource::Data::~Data() {
        if(isRawData) {
            raw = nullptr;
        } else {
            fileHandle = nullptr;
        }
    }
}
