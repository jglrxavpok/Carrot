//
// Created by jglrxavpok on 30/04/2022.
//

#pragma once

#include <string>
#include <filesystem>
#include <map>
#include "core/async/ParallelMap.hpp"
#include "core/io/Path.h"

namespace Carrot::IO {
    /// Access to the VFS is internally synchronized
    class VirtualFileSystem {
    public:
        using BasicPath = IO::Path;

        /// Path inside the VFS
        ///  Can be represented as <root>://<path>, or <path> directly
        ///  Examples:
        ///  - engine://resources/textures/default.png -> only search inside "engine" root
        ///  - game://resources/models/main_character.fbx -> only search inside "game" root
        ///  - resources/models/main_character.fbx -> search through all roots
        class Path {
        public:
            Path();
            Path(std::string_view uri);
            Path(std::string_view root, const NormalizedPath& path);

            Path(const char* uri): Path(std::string_view(uri)) {};

            Path(const Path& toCopy) = default;
            Path(Path&& toMove) = default;

            Path& operator=(const Path& toCopy) = default;
            Path& operator=(Path&& toMove) = default;

            bool operator==(const Path& other) const = default;

            Path operator/(const NormalizedPath& subpath) const;
            Path operator/(std::string_view subpath) const;

        public:
            Path relative(const Path& other) const;

        public:
            /**
             * True iif no root is associated to this path.
             * If true, will iterate through all known roots on access.
             */
            bool isGeneric() const;

            /**
             * True iif the relative path part is empty
             */
            bool isEmpty() const;

            std::string toString() const;

            const std::string& getRoot() const { return root; }
            const NormalizedPath& getPath() const { return path; }

            // For GTest
            friend std::ostream& operator<<(std::ostream& os, const Path& bar) {
                return os << bar.root << ":" << bar.path;
            }

        private:
            /// Sets this path contents, and performs validation
            void set(std::string_view root, const NormalizedPath& path);

            std::string root;
            NormalizedPath path;
        };

        /// Resolves the input VFS path to a physical absolute path. Throws if the root is not valid
        std::filesystem::path resolve(const Path& path) const;

        /// Resolves the input VFS path to a physical absolute path
        std::optional<std::filesystem::path> safeResolve(const Path& path) const;

        /// Attempts to represent the given path inside this VFS. Path must be absolute
        ///  If no root matched the given path, returns an empty optional.
        std::optional<Path> represent(const std::filesystem::path& path) const;

        bool exists(const Path& path) const;

        /**
         * Returns a copy of the current roots when called.
         */
        std::vector<std::string> getRoots() const;

    public: // root management
        /// Adds a new root to the VFS with the given identifier. The identifier must match [a-z0-9_].
        ///  Identifier must also not exist already
        void addRoot(std::string_view identifier, const std::filesystem::path& root);

        /// Attemps to remove a given root from the VFS. Returns true if a root with the given identifier was removed.
        bool removeRoot(std::string_view identifier);

    private:
        Async::ParallelMap<std::string, std::filesystem::path> roots;
    };

    using VFS = VirtualFileSystem;
}

namespace std {
    template<>
    struct hash<Carrot::IO::VFS::Path> {
        size_t operator()(const Carrot::IO::VFS::Path& o) const {
            return std::hash<std::string>{}(o.toString());
        }
    };
}
