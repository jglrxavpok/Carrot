//
// Created by jglrxavpok on 05/06/2022.
//

#pragma once

#include "EditorPanel.h"
#include "engine/render/resources/Texture.h"

namespace Peeler {
    struct ThumbnailLoader {
        void startLoading(const Carrot::IO::VFS::Path& filepath);

        std::atomic_flag isReady = false;
        Carrot::Render::Texture::Ref loadedTexture;
    };

    class ResourcePanel: public EditorPanel {
    public:
        explicit ResourcePanel(Application& app);

        virtual void draw(const Carrot::Render::Context &renderContext) override final;

    private:
        /// Returns true iif the path corresponds to a folder and the user wants to open it (double click)
        bool drawFileTile(const Carrot::IO::VFS::Path& vfsPath, float tileSize);
        const Carrot::Render::Texture& requestFileThumbnail(const Carrot::IO::VFS::Path& filePath);
        void updateCurrentFolder(const Carrot::IO::VFS::Path& vfsPath);

        Carrot::Render::Texture::Ref getLoadedFileThumbnail(const Carrot::IO::VFS::Path& filePath);

    private:
        void fillModelContextPopup(const Carrot::IO::VFS::Path& vfsPath);
        void fillPrefabContextPopup(const Carrot::IO::VFS::Path& vfsPath);

    private:
        enum class ResourceType {
            GenericFile,
            Folder,
            Drive,
            // TODO: images, shaders, scenes, etc.
        };

        struct ResourceEntry {
            ResourceType type = ResourceType::GenericFile;
            Carrot::IO::VFS::Path path;
        };
        Carrot::IO::VFS::Path currentFolder = "engine://";
        Carrot::Vector<ResourceEntry> resourcesInCurrentFolder;

        std::unordered_map<Carrot::IO::FileFormat, Carrot::Render::Texture::Ref> filetypeIcons;
        std::unordered_map<Carrot::IO::VFS::Path, Carrot::Render::Texture::Ref> thumbnails;
        std::unordered_map<Carrot::IO::VFS::Path, ThumbnailLoader> thumbnailLoaders;
        Carrot::Render::Texture genericFileIcon;
        Carrot::Render::Texture folderIcon;

        ImGuiID mainNode;

        friend class Application;
    };
}