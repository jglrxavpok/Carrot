//
// Created by jglrxavpok on 01/05/2022.
//

#include "Path.h"
#include "core/Macros.h"
#include "core/utils/stringmanip.h"
#include <stdexcept>
#include <vector>
#include <deque>
#include <filesystem>

namespace Carrot::IO {
    PathPartIterator::PathPartIterator(const Path& path, std::size_t startIndex, std::size_t endIndex): path(path), startIndex(startIndex), endIndex(endIndex) {}

    bool PathPartIterator::operator==(const PathPartIterator& other) const {
        return &path == &other.path && startIndex == other.startIndex && endIndex == other.endIndex;
    }

    PathPartIterator PathPartIterator::operator++(int) {
        return operator++();
    }

    PathPartIterator PathPartIterator::operator++() {
        if(startIndex == std::string::npos) { // end iterator
            return *this;
        }
        if(endIndex == std::string::npos) { // last before end
            startIndex = endIndex;
            return *this;
        }
        startIndex = endIndex+1;
        endIndex = path.path.find('/', endIndex+1);
        return *this;
    }

    std::string_view PathPartIterator::operator*() const {
        if(startIndex == std::string::npos) {
            throw std::runtime_error("Dereferencing end iterator");
        }
        std::size_t end = endIndex;
        if(end == std::string::npos) {
            end = path.path.size();
        }
        std::string_view view = path.path;
        if(startIndex == endIndex && startIndex == 0) { // special case for starting '/' in absolute paths
            return view.substr(startIndex, startIndex+1);
        }
        return view.substr(startIndex, end-startIndex);
    }

    Path::Path(): Path("") {}
    Path::Path(std::string_view path): path(path) {
        std::replace(this->path.begin(), this->path.end(), '\\', Path::Separator);
    }

    bool Path::isEmpty() const {
        return path.empty();
    }

    bool Path::isAbsolute() const {
        return !path.empty() && path[0] == Path::Separator;
    }

    bool Path::isRelative() const {
        return !isAbsolute();
    }

    PathPartIterator Path::begin() const {
        if(isAbsolute()) {
            return PathPartIterator(*this, 0, 0); // root character is its own part
        }
        std::size_t nextIndex = path.find('/', 0);
        return PathPartIterator(*this, 0, nextIndex);
    }

    PathPartIterator Path::end() const {
        return PathPartIterator(*this, std::string::npos, std::string::npos);
    }

    NormalizedPath Path::normalize() const {
        std::vector<std::string_view> parts;

        auto it = begin();
        auto endIt = end();

        bool absolute = false;
        if(isAbsolute()) {
            absolute = true;
            it++;
        }
        while(it != endIt) {
            const auto& part = *it;
            if(part == "..") {
                if(parts.empty()) {
                    throw std::invalid_argument(Carrot::sprintf("Could not normalize \"%s\" as it goes outside of root", path.c_str()));
                }
                parts.pop_back();
            } else if(!part.empty() && part != ".") {
                parts.emplace_back(part);
            }

            it++;
        }
        NormalizedPath p {};
        if(absolute) {
            parts.insert(parts.begin(), ""); // joining will add a '/' at the beginning of the path
        }
        p.path = Carrot::joinStrings(parts, Path::SeparatorStr);
        return p;
    }

    Path Path::operator/(std::string_view toAppend) const {
        return append(toAppend);
    }

    Path Path::operator/(const char* toAppend) const {
        return append(toAppend);
    }

    Path Path::operator/(const Path& toAppend) const {
        return append(toAppend);
    }

    Path Path::append(std::string_view toAppend) const {
        if(isEmpty()) {
            return {toAppend};
        }

        if(toAppend.empty()) {
            return *this;
        }

        std::size_t thisSize = path.size();
        if(path[thisSize-1] == Path::Separator) {
            thisSize--; // don't include trailing separator
        }

        std::size_t otherOffset = toAppend[0] == Path::Separator ? 1 : 0;

        std::size_t newSize = thisSize + toAppend.size() + 1/* separator */;
        std::unique_ptr<char[]> resultPath = std::make_unique<char[]>(newSize+1);
        memcpy(resultPath.get(), path.data(), thisSize);
        resultPath.get()[thisSize] = Path::Separator;
        memcpy(resultPath.get() + thisSize + 1, toAppend.data() + otherOffset, toAppend.size()-otherOffset);
        resultPath.get()[newSize] = '\0';
        return Path{resultPath.get()};
    }

    Path Path::append(const char* toAppend) const {
        return append(std::string_view(toAppend));
    }

    Path Path::append(const Path& toAppend) const {
        return append(toAppend.path);
    }

    Path Path::relative(std::string_view relativePath) const {
        if(relativePath.starts_with(Path::Separator) || isEmpty()) {
            return Path { relativePath };
        }
        if(path.ends_with(Path::Separator)) {
            return append(relativePath);
        }

        std::filesystem::path fullPath = path;
        fullPath = fullPath.parent_path();
        fullPath /= relativePath;
        return Carrot::IO::Path { std::string_view(fullPath.string()) };
    }

    Path Path::relative(const char* relativePath) const {
        return relative(std::string_view(relativePath));
    }

    bool Path::operator==(const Path& other) const {
        return path == other.path;
    }

    std::string_view Path::getFilename() const {
        std::size_t lastSeparator = path.find_last_of('/');
        if(lastSeparator == std::string::npos) {
            return path;
        }
        std::string_view pathView = path;
        return pathView.substr(lastSeparator+1);
    }

    std::string_view Path::getExtension() const {
        if(path.empty())
            return {};
        std::size_t startSearchIndex = path.find_last_of('/');
        if(startSearchIndex == std::string::npos) {
            // no separators, start from start of string
            startSearchIndex = 0;
        } else {
            startSearchIndex++; // go after last separator
        }
        const std::size_t index = path.find_last_of('.');
        if(index == std::string::npos) {
            // no extension
            return {};
        }
        if(index < startSearchIndex) {
            // dot before last '/'
            return {};
        }
        if(index == startSearchIndex) {
            // hidden file
            return {};
        }
        std::string_view pathView = path;
        return pathView.substr(index);
    }

    Path Path::withExtension(std::string_view wantedExtension) const {
        if(path.empty()) {
            return wantedExtension;
        }

        std::string extensionStr;
        if(!wantedExtension.empty()) {
            if(wantedExtension[0] != '.') {
                extensionStr += '.';
            }
            extensionStr += wantedExtension;
        }

        std::size_t startSearchIndex = path.find_last_of('/');
        if(startSearchIndex == std::string::npos) {
            // no separators, start from start of string
            startSearchIndex = 0;
        } else {
            startSearchIndex++; // go after last separator
        }
        const std::size_t index = path.find_last_of('.');
        if(index == std::string::npos) {
            // no extension, add the new one
            return Path{ path + extensionStr };
        }
        if(index < startSearchIndex) {
            // dot before last '/'
            return Path{ path + extensionStr };
        }
        if(index == startSearchIndex) {
            // hidden file
            return Path{ path + extensionStr };
        }
        std::string_view pathView = path;

        std::string result { pathView.substr(0, index) };
        result += extensionStr;
        return Path { std::move(result) };
    }

    // -- NormalizedPath
    NormalizedPath::NormalizedPath(): Path::Path() {}

    NormalizedPath::NormalizedPath(const Path& path) : Path(path) {
        *this = path.normalize();
    }

    NormalizedPath& NormalizedPath::operator=(const Path& toCopy) {
        *this = toCopy.normalize();
        return *this;
    }
}