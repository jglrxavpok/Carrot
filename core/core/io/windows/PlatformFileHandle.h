//
// Created by jglrxavpok on 25/08/2023.
//

#pragma once
#ifdef _WIN32
#include <core/io/FileHandle.h>
#include <core/async/Counter.h>
#include <windows.h>

namespace Carrot::IO {

    /**
     * Platform file handle for Windows
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
        HANDLE handle = INVALID_HANDLE_VALUE;
    };

} // Carrot::IO

#endif