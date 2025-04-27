//
// Created by jglrxavpok on 24/04/25.
//

#pragma once
#ifdef __linux__
#include <core/io/FileHandle.h>
#include <core/async/Counter.h>

namespace Carrot::IO {

    /**
     * Platform file handle for Linux
     */
    class PlatformFileHandle {
    public:
        /**
         * Responsibility of user code to delete pointer
         */
        static PlatformFileHandle* open(const std::filesystem::path& path, OpenMode openMode);
        ~PlatformFileHandle();

        void close();
        void write(std::span<const std::uint8_t> data);
        void seek(std::int64_t position, int seekDirection);

        void read(std::span<std::uint8_t> data) const;
        std::uint64_t tell() const;

    private:
        FILE* file = nullptr;
    };

} // Carrot::IO

#endif