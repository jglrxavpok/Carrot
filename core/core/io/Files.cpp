//
// Created by jglrxavpok on 13/10/2021.
//

#include "Files.h"
#include "core/Macros.h"
#include "core/utils/stringmanip.h"
#include <nfd.h>

#ifdef _WIN32
    #include <windows.h>
#endif

namespace Carrot::IO::Files {
    bool isRootFolder(const std::filesystem::path& path) {
        return path.parent_path() == path;
    }

    std::vector<std::filesystem::path> enumerateRoots() {
#ifdef _WIN32
        WCHAR buffer[256];
        DWORD length = GetLogicalDriveStringsW(sizeof(buffer), buffer);
        verify(length < sizeof(buffer), "Buffer is too small");
        std::wstring currentDrive;
        std::vector<std::filesystem::path> result;
        for (int i = 0; i < length; ++i) {
            wchar_t c = buffer[i];
            if(c == '\0') {
                result.emplace_back(currentDrive);
                currentDrive.clear();
            } else {
                currentDrive += c;
            }
        }
        return result;
#else
        return std::vector<std::filesystem::path>{"/"};
#endif
    }

    bool showSaveDialog(std::filesystem::path& _savePath, std::span<NativeFileDialogFilter> filters, std::optional<std::string> defaultName, std::optional<std::filesystem::path> defaultPath) {
        nfdchar_t *savePath;

        // prepare filters for the dialog
        std::vector<nfdfilteritem_t> filterItems;
        filterItems.reserve(filters.size());
        for(const auto& filter : filters) {
            filterItems.emplace_back(nfdfilteritem_t { filter.name.c_str(), filter.filters.c_str() });
        }

        // show the dialog
        nfdresult_t result = NFD_SaveDialog(&savePath,
                                            filterItems.data(), filterItems.size(), // filters
                                            defaultPath.has_value() ? Carrot::toString(defaultPath->u8string()).c_str() : nullptr, // default path
                                            defaultName.has_value() ? defaultName->c_str() : nullptr // default name
                                            );
        if (result == NFD_OKAY) {
            _savePath = savePath;
            // remember to free the memory (since NFD_OKAY is returned)
            NFD_FreePath(savePath);
            return true;
        } else if (result == NFD_CANCEL) {
            return false;
        } else {
            std::string msg = "Error: ";
            msg += NFD_GetError();
            throw std::runtime_error(msg);
        }
    }
}