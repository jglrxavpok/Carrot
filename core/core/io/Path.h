//
// Created by jglrxavpok on 01/05/2022.
//

#pragma once

#include <cstdint>
#include <string>
#include <iostream>

namespace Carrot::IO {
    class Path;
    class NormalizedPath;

    class PathPartIterator {
    public:
        bool operator==(const PathPartIterator& other) const;
        PathPartIterator operator++();
        PathPartIterator operator++(int);

        std::string_view operator*() const;

    private:
        explicit PathPartIterator(const Path& path, std::size_t startIndex, std::size_t endIndex);

    private:
        const Path& path;
        std::size_t startIndex = std::string::npos;
        std::size_t endIndex = std::string::npos;

        friend class Path;
    };

    /// Goal is to represent very basic paths, not encapsulate the intricacies of each OS paths. Not a replacement for std::filesystem::path either
    /// Paths can be absolute or relative, the only difference is that absolute paths start with '/'
    /// These paths support folders, with the separator being '/'
    /// Paths are case-sensitive and immutable
    /// A part is a subset of the path, delimited by the bounds of the path, or a path separator ('/'). In other words, it's the name of a folder or file.
    class Path {
    public:
        constexpr static const char Separator = '/';
        constexpr static const char* const SeparatorStr = "/";

        Path();
        Path(std::string_view path);
        Path(const char* path): Path(std::string_view(path)) {};

        Path(const Path& toCopy) = default;
        Path(Path&& toMove) = default;

        virtual Path& operator=(const Path& toCopy) = default;
        Path& operator=(Path&& toMove) = default;

        bool operator==(const Path& other) const;

        virtual ~Path() = default;

    public:
        bool isEmpty() const;
        bool isAbsolute() const;
        bool isRelative() const;

        const char* get() const { return path.c_str(); }
        const char* c_str() const { return get(); }
        const std::string& asString() const { return path; }

        /**
         * Returns a string_view representing the filename inside the path, extracted from the last separator to the end of the path
         * @return
         */
        std::string_view getFilename() const;

        /**
         * Returns a string_view representing the extension inside the path (with the dot), extracted from the last dot
         * present after the last separator, to the end of the path.
         * @return
         */
        std::string_view getExtension() const;

        friend std::ostream& operator<<(std::ostream& os, const Path& bar) {
            return os << bar.path;
        }

    public:
        Path append(std::string_view toAppend) const;
        Path append(const char* toAppend) const;
        Path append(const Path& toAppend) const;
        Path operator/(std::string_view toAppend) const;
        Path operator/(const char* toAppend) const;
        Path operator/(const Path& toAppend) const;

    public:
        Path relative(std::string_view toAppend) const;
        Path relative(const char* toAppend) const;

    public:
        PathPartIterator begin() const;
        PathPartIterator end() const;

    public:
        /// Normalizes a path:
        /// - "." is removed
        /// - "" is removed. This means "/////a" will be "/a"
        /// - ".." is removed, and the precedent path part is removed
        /// Note: throws if normalizing the path goes "out-of-root". You are not allowed to have more ".." parts than total parts inside the path
        NormalizedPath normalize() const;

    private:
        std::string path;

        friend class PathPartIterator;
    };

    /// Path that is always normalized.
    class NormalizedPath: public Path {
    public:
        NormalizedPath();
        NormalizedPath(const Path& path);
        NormalizedPath(std::string_view path): NormalizedPath(Path(path)) {};
        NormalizedPath(const char* path): NormalizedPath(Path(path)) {};

        NormalizedPath(const NormalizedPath& toCopy) = default;
        NormalizedPath(NormalizedPath&& toMove) = default;

        NormalizedPath& operator=(const Path& toCopy);
        NormalizedPath& operator=(const NormalizedPath& toCopy) = default;
        NormalizedPath& operator=(NormalizedPath&& toMove) = default;
    };
}