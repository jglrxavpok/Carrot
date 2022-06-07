//
// Created by jglrxavpok on 05/06/2022.
//

#include "ResourcePanel.h"
#include "../Peeler.h"
#include <core/utils/ImGuiUtils.hpp>
#include <core/io/IO.h>
#include <core/io/Files.h>
#include <engine/edition/DragDropTypes.h>

namespace Peeler {
    ResourcePanel::ResourcePanel(Application& app):
        EditorPanel(app),
        folderIcon(GetVulkanDriver(), "resources/textures/ui/folder.png"),
        genericFileIcon(GetVulkanDriver(), "resources/textures/ui/file.png"),
        parentFolderIcon(GetVulkanDriver(), "resources/textures/ui/parent_folder.png"),
        driveIcon(GetVulkanDriver(), "resources/textures/ui/drive.png")
    {
        updateCurrentFolder(Carrot::IO::VFS::Path("engine://"));
    }


    void ResourcePanel::draw(const Carrot::Render::Context& renderContext) {
        static float leftPaneWidth = 0.4f * ImGui::GetContentRegionAvailWidth();
        static float rightPaneWidth = ImGui::GetContentRegionAvailWidth() - leftPaneWidth;

        const char* treeViewID = "Resource Panel Tree View##resource panel tree view";
        const char* fileViewID = "Resource Panel File View##resource panel file view";

        mainNode = ImGui::GetID("##Resource Panel");

        if(!ImGui::DockBuilderGetNode(mainNode)) {
            ImGui::DockBuilderRemoveNode(mainNode);
            ImGui::DockBuilderAddNode(mainNode);
            ImGui::DockBuilderSetNodeSize(mainNode, ImGui::GetContentRegionAvail());

            ImGuiID leftNodeID;
            ImGuiID rightNodeID;
            ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Left, 0.4f, &leftNodeID, &rightNodeID);

            auto* leftNode = ImGui::DockBuilderGetNode(leftNodeID);
            leftNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

            auto* rightNode = ImGui::DockBuilderGetNode(rightNodeID);
            rightNode->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

            ImGui::DockBuilderDockWindow(treeViewID, leftNodeID);
            ImGui::DockBuilderDockWindow(fileViewID, rightNodeID);
            ImGui::DockBuilderFinish(mainNode);
        }

        ImGui::DockSpace(mainNode);

        if(ImGui::Begin(treeViewID)) {
            ImGuiDockNodeFlags_ flags = ImGuiDockNodeFlags_None;
            std::function<void(const std::filesystem::path&, const std::string&)> showFileHierarchy = [&](const std::filesystem::path& folder, const std::string& toDisplay) -> void {
                ImGui::PushID(folder.string().c_str());
                if(ImGui::TreeNodeEx(folder.string().c_str(), flags, "%s", toDisplay.c_str())) {
                    std::error_code ec; // TODO: check
                    std::filesystem::directory_options options = std::filesystem::directory_options::none;
                    for(const auto& child : std::filesystem::directory_iterator(folder, options, ec)) {
                        std::string childName = child.path().filename().string();
                        if(child.is_directory()) {
                            showFileHierarchy(child.path(), childName);
                        } else {
                            ImGui::BulletText("%s", childName.c_str());
                        }
                    }

                    ImGui::TreePop();
                }
                ImGui::PopID();
            };

            for(const auto& root : GetVFS().getRoots()) {
                showFileHierarchy(GetVFS().resolve(Carrot::IO::VFS::Path(root, "")), root);
            }
        }
        ImGui::End();

        if(ImGui::Begin(fileViewID)) {
            ImGui::TextUnformatted(currentFolder.getPath().c_str());

            // Thanks Hazel
            // https://github.com/TheCherno/Hazel/blob/f627b9c90923382f735350cd3060892bbd4b1e75/Hazelnut/src/Panels/ContentBrowserPanel.cpp#L30
            static float padding = 8.0f;
            static float thumbnailSize = 64.0f;
            float cellSize = thumbnailSize + padding;
            float panelWidth = ImGui::GetContentRegionAvail().x;
            int columnCount = (int)(panelWidth / cellSize);
            if (columnCount < 1)
                columnCount = 1;

            ImGuiTableFlags tableFlags = ImGuiTableFlags_NoBordersInBody;
            if(ImGui::BeginTable("##resource panel file view table", columnCount, tableFlags)) {
                ImGui::TableNextRow();
                std::uint32_t columnIndex = 0;
                std::uint32_t index = 0;

                std::optional<Carrot::IO::VFS::Path> updateToPath;
                for(const auto& entry : resourcesInCurrentFolder) {
                    ImGui::TableNextColumn();

                    ImTextureID texture = nullptr;
                    switch(entry.type) {
                        case ResourceType::GenericFile:
                            texture = genericFileIcon.getImguiID();
                            break;

                        case ResourceType::Folder:
                            texture = folderIcon.getImguiID();
                            break;

                        case ResourceType::Drive:
                            texture = driveIcon.getImguiID();
                            break;

                        default:
                            TODO // missing a resource type
                    }
                    std::string filename = entry.path.toString();
                    ImGui::PushID(index);
                    {
                        ImGui::ImageButton(texture, ImVec2(thumbnailSize, thumbnailSize));

                        if(ImGui::BeginDragDropSource()) {
                            ImGui::TextUnformatted(filename.c_str());
                            ImGui::SetDragDropPayload(Carrot::Edition::DragDropTypes::FilePath, filename.c_str(), filename.length()*sizeof(char8_t));
                            ImGui::EndDragDropSource();
                        }

                        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                            if(std::filesystem::is_directory(GetVFS().resolve(entry.path))) {
                                updateToPath = entry.path;
                            }
                        }

                        std::string smallFilename = std::string(entry.path.getPath().getFilename());
                        ImGui::TextUnformatted(smallFilename.c_str());
                    }
                    ImGui::PopID();

                    if(++columnIndex == columnCount) {
                        columnIndex = 0;
                        ImGui::TableNextRow();
                    }
                    index++;
                }

                if(updateToPath) {
                    updateCurrentFolder(updateToPath.value());
                }

                ImGui::EndTable();
            }
        }
        ImGui::End();

    }

    void ResourcePanel::updateCurrentFolder(const Carrot::IO::VFS::Path& vfsPath) {
        currentFolder = std::move(vfsPath);
        resourcesInCurrentFolder.clear();
        if(GetVFS().exists(vfsPath)) {
            for(const auto& file : std::filesystem::directory_iterator(GetVFS().resolve(currentFolder))) {
                ResourceType type = file.is_directory() ? ResourceType::Folder : ResourceType::GenericFile;
                Carrot::IO::VFS::Path p = GetVFS().represent(file.path()).value();
                resourcesInCurrentFolder.emplace_back(type, p);
            }
        }
    }

}