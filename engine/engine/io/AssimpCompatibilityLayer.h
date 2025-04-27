//
// Created by jglrxavpok on 06/05/2022.
//

#pragma once

#include <core/io/vfs/VirtualFileSystem.h>
#include <core/io/Resource.h>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>

/// Allows Assimp to communicate with the VFS

namespace Carrot::IO {
    class CarrotIOStream: public Assimp::IOStream {
    public:
        virtual size_t Read( void* pvBuffer, size_t pSize, size_t pCount) override;
        virtual size_t Write( const void* pvBuffer, size_t pSize, size_t pCount) override;
        virtual aiReturn Seek( size_t pOffset, aiOrigin pOrigin) override;
        virtual size_t Tell() const override;
        virtual size_t FileSize() const override;
        virtual void Flush() override;

    protected:
        // Constructor protected for private usage by MyIOSystem
        CarrotIOStream(Carrot::IO::FileHandle&& resource);

    private:
        Carrot::IO::FileHandle file;
        std::size_t seekCursor = 0;

        friend class CarrotIOSystem;
    };

    class CarrotIOSystem: public Assimp::IOSystem {
    public:
        CarrotIOSystem(const Carrot::IO::Resource& sourceResource);

        virtual ~CarrotIOSystem();

        bool Exists(const char *pFile) const override;

        char getOsSeparator() const override;

        Assimp::IOStream *Open(const char *pFile, const char *pMode) override;

        void Close(Assimp::IOStream *pFile) override;

        bool ComparePaths(const char *one, const char *second) const override;

    private:
        std::optional<VFS::Path> getPath(const char* file) const;

        std::string vfsRoot;
    };
}
