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
            std::filesystem::path currentFolderInDisk = GetVFS().resolve(currentFolder);
            ImGuiTreeNodeFlags_ flags = ImGuiTreeNodeFlags_SpanAvailWidth;
            std::function<void(const std::filesystem::path&, const std::string&, bool)> showFileHierarchy = [&](const std::filesystem::path& folder, const std::string& toDisplay, bool isFolder) -> void {
                ImGui::PushID(folder.string().c_str());

                ImGuiTreeNodeFlags localFlags = flags;
                if(!isFolder) {
                    localFlags |= ImGuiTreeNodeFlags_Bullet;
                }

                if(folder == currentFolderInDisk) {
                    localFlags |= ImGuiTreeNodeFlags_Selected;
                }

                const bool opened = ImGui::TreeNodeEx(folder.string().c_str(), localFlags, "");

                const ImVec2 imageSize = ImVec2(ImGui::GetFontSize(), ImGui::GetFontSize());
                const float baseOffset = ImGui::GetFontSize() /* arrow */ + ImGui::GetStyle().FramePadding.x * 2;
                const float textOffset = baseOffset + (ImGui::GetStyle().FramePadding.x) + imageSize.x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + imageSize.x);
                ImGui::SameLine();
                if(isFolder) {
                    ImGui::Image(folderIcon.getImguiID(), imageSize);
                } else {
                    ImGui::Image(genericFileIcon.getImguiID(), imageSize);
                }
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
                ImGui::SameLine();
                ImGui::Text("%s", toDisplay.c_str());
                if(opened) {
                    if(isFolder) {
                        std::error_code ec; // TODO: check
                        std::filesystem::directory_options options = std::filesystem::directory_options::none;
                        for (const auto& child: std::filesystem::directory_iterator(folder, options, ec)) {
                            std::string childName = child.path().filename().string();
                            if (child.is_directory()) {
                                showFileHierarchy(child.path(), childName, true);
                            } else {
                                showFileHierarchy(child.path(), childName, false);
                            }
                        }
                    }
                    ImGui::TreePop();
                }
                ImGui::PopID();
            };

            for(const auto& root : GetVFS().getRoots()) {
                showFileHierarchy(GetVFS().resolve(Carrot::IO::VFS::Path(root, "")), root, true);
            }
        }
        ImGui::End();

        if(ImGui::Begin(fileViewID)) {
            auto parts = Carrot::splitString(currentFolder.getPath().asString(), Carrot::IO::Path::SeparatorStr);

            parts.insert(parts.begin(), "/");

            static ImGuiTextFilter searchFilter;

            const float height = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;
            if(ImGui::BeginChild("##file address", ImVec2(0, height))) {
                ImGui::PushID("##file address widget");
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2,2));

                Carrot::IO::VFS::Path reconstructedPath = Carrot::IO::VFS::Path(currentFolder.getRoot(), "");
                for (int j = 0; j < parts.size(); ++j) {
                    ImGui::PushID(j);
                    const auto& part = parts[j];
                    if(j != 0) {
                        ImGui::SameLine();
                        ImVec2 size = ImGui::CalcTextSize(">");
                        ImGui::SetNextItemWidth(size.x + ImGui::GetStyle().CellPadding.x *2);
                        if(ImGui::BeginCombo("##subdirectory selector", ">", ImGuiComboFlags_NoArrowButton)) {
                            for (const auto& file: std::filesystem::directory_iterator(GetVFS().resolve(reconstructedPath))) {
                                if(file.is_directory()) {
                                    std::string filename = file.path().filename().string();
                                    bool selected = filename == part;
                                    if(ImGui::Selectable(filename.c_str(), selected)) {
                                        updateCurrentFolder(reconstructedPath / filename);
                                    }
                                }
                            }
                            ImGui::EndCombo();
                        }
                        ImGui::SameLine();

                        reconstructedPath = reconstructedPath / part;
                    }
                    if(ImGui::Button(part.c_str())) {
                        updateCurrentFolder(reconstructedPath);
                    }

                    ImGui::PopID();
                }
                ImGui::PopStyleVar(1);


                const char* filterText = "Filter (inc,-exc)";
                const float width = 200.0f;
                const float wantedX = ImGui::GetWindowWidth() - width - ImGui::CalcTextSize(filterText).x;
                ImGui::SameLine(wantedX);

                searchFilter.Draw(filterText, width);

                ImGui::PopID();

            }
            ImGui::EndChild();

            ImGui::Separator();

            if(ImGui::BeginChild("##file view")) {
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
                        if(!searchFilter.PassFilter(filename.c_str())) {
                            continue;
                        }
                        ImGui::TableNextColumn();

                        ImGui::PushID(index);
                        {
                            ImGui::ImageButton(texture, ImVec2(thumbnailSize, thumbnailSize));
                            if(ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("%s", filename.c_str());
                            }

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
            ImGui::EndChild();
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