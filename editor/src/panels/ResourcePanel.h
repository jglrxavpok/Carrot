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
        void updateCurrentFolder(const Carrot::IO::VFS::Path& vfsPath);

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

        Carrot::Render::Texture genericFileIcon;
        Carrot::Render::Texture folderIcon;
        Carrot::Render::Texture parentFolderIcon;
        Carrot::Render::Texture driveIcon;

        ImGuiID mainNode;

        friend class Application;
    };
}