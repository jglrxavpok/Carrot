//
// Created by jglrxavpok on 05/06/2022.
//

#pragma once

#include "EditorPanel.h"
#include "engine/render/resources/Texture.h"

namespace Peeler {
    class ResourcePanel: public EditorPanel {
    public:
        explicit ResourcePanel(Application& app);

        virtual void draw(const Carrot::Render::Context &renderContext) override final;

    private:
        /// Returns true iif the path corresponds to a folder and the user wants to open it (double click)
        bool drawFileTile(const Carrot::IO::VFS::Path& vfsPath, float tileSize);
        const Carrot::Render::Texture& getFileTexture(const Carrot::IO::VFS::Path& filePath) const;
        void updateCurrentFolder(const Carrot::IO::VFS::Path& vfsPath);

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
        std::vector<ResourceEntry> resourcesInCurrentFolder;

        std::unordered_map<Carrot::IO::FileFormat, Carrot::Render::Texture::Ref> filetypeIcons;
        Carrot::Render::Texture genericFileIcon;
        Carrot::Render::Texture folderIcon;

        ImGuiID mainNode;

        friend class Application;
    };
}