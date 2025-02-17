//
// Created by jglrxavpok on 30/04/2022.
//

#include "VirtualFileSystem.h"
#include "core/exceptions/Exceptions.h"
#include "core/utils/stringmanip.h"
#include "core/io/Logging.hpp"
#include <regex>

namespace Carrot::IO {

    static const std::regex RootRegex("[a-z0-9_]*");

    void VirtualFileSystem::addRoot(std::string_view identifier, const std::filesystem::path& root) {
        if(identifier.empty()) {
            throw std::invalid_argument(std::string(identifier));
        }
        if(!std::regex_match(identifier.data(), identifier.data()+identifier.size(), RootRegex)) {
            throw std::invalid_argument(std::string(identifier));
        }
        if(!root.is_absolute()) {
            throw std::invalid_argument(Carrot::sprintf("Root path must be absolute: %s", root.u8string().c_str()));
        }

        Carrot::Log::debug("Added root %s at %s", std::string(identifier).c_str(), root.string().c_str());

        bool newRoot = false;
        auto& rootPath = roots.getOrCompute(std::string(identifier), [&]() -> std::filesystem::path {
            newRoot = true;
            return {};
        });
        verify(newRoot, "Root must not already exist");

        rootPath = root;
    }

    bool VirtualFileSystem::hasRoot(std::string_view identifier) const {
        auto snapshot = roots.snapshot();
        for(const auto& [rootID, _] : snapshot) {
            if(rootID == identifier)
                return true;
        }
        return false;
    }

    bool VirtualFileSystem::removeRoot(std::string_view identifier) {
        return roots.remove(std::string(identifier));
    }

    std::filesystem::path VirtualFileSystem::resolve(const VirtualFileSystem::Path& path) const {
        auto it = safeResolve(path);
        verify(it.has_value(), Carrot::sprintf("Invalid root: %s", path.getRoot().c_str()));
        return it.value();
    }

    VirtualFileSystem::Path VirtualFileSystem::complete(const Path& path) const {
        if(path.isGeneric()) {
            auto normalizedVersion = path.getPath();
            // TODO: might need caching of some kind to avoid querying the OS each time
            for(const auto& [rootID, rootPath] : roots.snapshot()) {
                std::filesystem::path p = *rootPath;
                p.append(normalizedVersion.asString());
                if(std::filesystem::exists(p)) {
                    return Path{rootID, path.getPath()};
                }
            }
            return Path{};
        } else {
            return path;
        }
    }

    std::optional<VirtualFileSystem::Path> VirtualFileSystem::represent(const std::filesystem::path& path) const {
        for(const auto& [rootID, rootPath] : roots.snapshot()) {
            std::filesystem::path relativePath = std::filesystem::relative(path, *rootPath);
            if(!relativePath.empty()) {
                std::string asStr = Carrot::toString(relativePath.u8string());
                if(asStr.size() < 2 || asStr[0] != '.' || asStr[1] != '.') {
                    return VirtualFileSystem::Path(rootID, NormalizedPath(asStr.c_str()));
                }
            }
        }
        return std::optional<VirtualFileSystem::Path> {};
    }

    bool VirtualFileSystem::exists(const VirtualFileSystem::Path& path) const {
        auto opt = safeResolve(path);
        if(!opt.has_value())
            return false;
        return std::filesystem::exists(opt.value());
    }

    std::vector<std::string> VirtualFileSystem::getRoots() const {
        std::vector<std::string> rootIDs;
        auto snapshot = roots.snapshot();
        rootIDs.reserve(snapshot.size());
        for(const auto& [rootID, _] : snapshot) {
            rootIDs.push_back(rootID);
        }
        return rootIDs;
    }

    std::optional<std::filesystem::path> VirtualFileSystem::safeResolve(const VirtualFileSystem::Path& path) const {
        if(path.isGeneric()) {
            auto normalizedVersion = path.getPath().normalize();
            // TODO: might need caching of some kind to avoid querying the OS each time
            for(const auto& [rootID, rootPath] : roots.snapshot()) {
                std::filesystem::path p = *rootPath;
                p.append(normalizedVersion.asString());
                if(std::filesystem::exists(p)) {
                    return p;
                }
            }
            return std::filesystem::path{};
        } else {
            const std::filesystem::path* rootPath = roots.find(path.getRoot());
            if(rootPath != nullptr) {
                std::filesystem::path result = *rootPath;
                return result.append(path.getPath().asString());
            } else {
                return std::optional<std::filesystem::path>{};
            }
        }
    }

    // Path

    VirtualFileSystem::Path::Path(): Path("") {}

    VirtualFileSystem::Path::Path(std::string_view uri) {
        std::size_t separatorPosition = uri.find(':');
        if(separatorPosition == std::string::npos) {
            set("", uri);
        } else {
            std::string_view foundRoot = uri.substr(0, separatorPosition);
            if(uri.size() < separatorPosition+3) {
                throw std::invalid_argument(Carrot::sprintf("Not enough characters after ':' in \"%s\"", uri.data()));
            }
            std::string_view foundPath = uri.substr(separatorPosition+3);
            if(foundPath.find(':') != std::string::npos) {
                throw std::invalid_argument(Carrot::sprintf("Found ':' character inside path part of \"%s\"", uri.data()));
            }
            set(foundRoot, foundPath);
        }
    }

    VirtualFileSystem::Path::Path(std::string_view root, const NormalizedPath& path) {
        set(root, path);
    }

    bool VirtualFileSystem::Path::isGeneric() const {
        return root.empty();
    }

    bool VirtualFileSystem::Path::isEmpty() const {
        return path.isEmpty();
    }

    std::string_view VirtualFileSystem::Path::getExtension() const {
        return path.getExtension();
    }

    VirtualFileSystem::Path VirtualFileSystem::Path::withExtension(std::string_view extension) const {
        return Path { root, path.withExtension(extension) };
    }

    std::string VirtualFileSystem::Path::toString() const {
        if(root.empty()) {
            return path.asString();
        }
        if(isGeneric()) {
            return path.asString();
        }
        return Carrot::sprintf("%s://%s", root.c_str(), path.c_str());
    }

    void VirtualFileSystem::Path::set(std::string_view newRoot, const NormalizedPath& newPath) {
        if(newPath.isAbsolute()) {
            throw std::invalid_argument(Carrot::sprintf("Path must not be absolute: %s", newPath.c_str()));
        }
        if(!std::regex_match(newRoot.data(), newRoot.data() + newRoot.size(), RootRegex)) {
            std::string s = newRoot.data();
            throw std::invalid_argument(Carrot::sprintf("Invalid root: %s", s.c_str()));
        }
        root = newRoot;
        path = newPath;
    }

    VirtualFileSystem::Path VirtualFileSystem::Path::operator/(const NormalizedPath& subpath) const {
        return VFS::Path(root, path.append(subpath));
    }

    VirtualFileSystem::Path VirtualFileSystem::Path::operator/(std::string_view subpath) const {
        return operator/(BasicPath(subpath));
    }

    VirtualFileSystem::Path VirtualFileSystem::Path::operator/(const char* subpath) const {
        return operator/(BasicPath(subpath));
    }

    VirtualFileSystem::Path VirtualFileSystem::Path::relative(const BasicPath& other) const {
        return Path {
            root,
            path.relative(other.asString())
        };
    }


    bool VirtualFileSystem::isDirectory(const Path& path) const {
        auto resolved = resolve(path);
        return std::filesystem::is_directory(resolved);
    }

    VirtualFileSystem::DirectoryIteration VirtualFileSystem::iterateOverDirectory(const Path& path) const {
        std::vector<std::string> roots;
        if (path.isGeneric()) {
            roots = getRoots();
        } else {
            roots.emplace_back(path.getRoot());
        }
        return DirectoryIteration(this, std::move(roots), path.getPath());
    }

    // Iterators
    VirtualFileSystem::DirectoryIteration::DirectoryIteration(const VirtualFileSystem* pVFS, std::vector<std::string> roots, const BasicPath& relativePath): pVFS(pVFS), roots(std::move(roots)), relativePath(relativePath) {}

    VirtualFileSystem::DirectoryIteration::Iterator VirtualFileSystem::DirectoryIteration::begin() {
        Iterator it { pVFS, &roots, &relativePath };
        return it;
    }

    VirtualFileSystem::DirectoryIteration::Iterator VirtualFileSystem::DirectoryIteration::end() {
        Iterator it { pVFS, &roots, &relativePath };
        it.setEnd();
        return it;
    }

    VirtualFileSystem::DirectoryIteration::Iterator::Iterator(const VirtualFileSystem *pVFS, const std::vector<std::string> *roots, const BasicPath *relativePath): pVFS(pVFS), pRoots(roots), pRelativePath(relativePath) {
        step();
    }

    void VirtualFileSystem::DirectoryIteration::Iterator::step() {
        if (rootIndex < 0) {
            // at end, nothing to do
            return;
        }

        // current root not initialized
        if (currentPoint == std::filesystem::directory_iterator{}) {
            while ((currentPoint = std::filesystem::directory_iterator{ pVFS->resolve({(*pRoots)[rootIndex], *pRelativePath}) }) == std::filesystem::directory_iterator{}) {
                rootIndex++;
                if (rootIndex >= pRoots->size()) {
                    setEnd();
                    return;
                }
            }
        }

        ++currentPoint;

        // end of current root
        if (currentPoint == std::filesystem::directory_iterator{}) {
            rootIndex++;
        }

        if (rootIndex >= pRoots->size()) {
            setEnd();
        }
    }

    VirtualFileSystem::DirectoryIteration::Iterator& VirtualFileSystem::DirectoryIteration::Iterator::operator++() {
        step();
        return *this;
    }

    void VirtualFileSystem::DirectoryIteration::Iterator::setEnd() {
        rootIndex = -1;
    }

    bool VirtualFileSystem::DirectoryIteration::Iterator::operator==(const Iterator& other) const {
        if (pVFS != other.pVFS) {
            return false;
        }
        if (rootIndex != other.rootIndex) {
            return false;
        }
        if (rootIndex == -1) {
            // both end iterators
            return true;
        }

        return pRoots == other.pRoots &&
            pRelativePath == other.pRelativePath &&
            currentPoint == other.currentPoint;
    }

    bool VirtualFileSystem::DirectoryIteration::Iterator::operator!=(const Iterator& other) const {
        return !(*this == other);
    }

    VirtualFileSystem::Path VirtualFileSystem::DirectoryIteration::Iterator::operator*() const {
        verify(rootIndex >= 0, "Cannot derefence end iterator!");
        if (rootIndex < 0) {
            return {};
        }

        const std::string filename = Carrot::toString(currentPoint->path().filename().u8string());
        return { (*pRoots)[rootIndex], *pRelativePath / BasicPath{ filename } };
    }


}