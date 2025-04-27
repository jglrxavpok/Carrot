//
// Created by jglrxavpok on 05/06/2022.
//

#include "ResourcePanel.h"
#include "../Peeler.h"
#include "engine/ecs/components/ModelComponent.h"
#include "engine/ecs/components/TransformComponent.h"
#include <core/utils/ImGuiUtils.hpp>
#include <core/io/IO.h>
#include <core/io/Files.h>
#include <core/io/Logging.hpp>
#include <engine/edition/DragDropTypes.h>
#include <core/utils/ImGuiUtils.hpp>
#include <engine/Engine.h>
#include <engine/ecs/Prefab.h>

namespace Peeler {
    ResourcePanel::ResourcePanel(Application& app):
        EditorPanel(app),
        folderIcon(GetVulkanDriver(), "resources/textures/ui/folder.png"),
        genericFileIcon(GetVulkanDriver(), "resources/textures/ui/file.png")
    {
        using namespace Carrot::IO;
        using namespace Carrot::Render;
        filetypeIcons[FileFormat::LUA] = GetAssetServer().blockingLoadTexture("resources/textures/ui/filetypes/lua.png");
        filetypeIcons[FileFormat::OBJ] = GetAssetServer().blockingLoadTexture("resources/textures/ui/filetypes/obj.png");
        filetypeIcons[FileFormat::FBX] = GetAssetServer().blockingLoadTexture("resources/textures/ui/filetypes/fbx.png");
        filetypeIcons[FileFormat::TXT] = GetAssetServer().blockingLoadTexture("resources/textures/ui/filetypes/txt.png");
        filetypeIcons[FileFormat::JSON] = GetAssetServer().blockingLoadTexture("resources/textures/ui/filetypes/json.png");

        updateCurrentFolder(Carrot::IO::VFS::Path("engine://"));
    }

    const Carrot::Render::Texture& ResourcePanel::getFileTexture(const Carrot::IO::VFS::Path& filePath) const {
        auto physicalPathOpt = GetVFS().safeResolve(filePath);
        if(!physicalPathOpt.has_value()) {
            return genericFileIcon;
        }
        auto& physicalPath = physicalPathOpt.value();
        if(physicalPath.empty()) {
            return genericFileIcon;
        }

        if(std::filesystem::is_directory(physicalPath)) {
            return folderIcon;
        }

        auto fileformat = Carrot::IO::getFileFormat(filePath.getPath().c_str());
        auto it = filetypeIcons.find(fileformat);
        if(it != filetypeIcons.end()) {
            return *it->second;
        }
        return genericFileIcon;
    }

    void ResourcePanel::draw(const Carrot::Render::Context& renderContext) {
        static float leftPaneWidth = 0.4f * ImGui::GetContentRegionAvailWidth();
        static float rightPaneWidth = ImGui::GetContentRegionAvailWidth() - leftPaneWidth;

        const char* treeViewID = "Resource Panel Tree View##resource panel tree view";
        const char* fileViewID = "Resource Panel File View##resource panel file view";

        mainNode = ImGui::GetID("##Resource Panel");

        if(ImGui::GetContentRegionAvail().x <= 0.0f || ImGui::GetContentRegionAvail().y <= 0.0f) {
            return;
        }

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
            auto folder = GetVFS().safeResolve(currentFolder);
            if(folder.has_value()) {
                std::filesystem::path currentFolderInDisk = folder.value();
                ImGuiTreeNodeFlags_ flags = ImGuiTreeNodeFlags_SpanAvailWidth;
                std::function<void(const std::filesystem::path&, const std::string&, bool)> showFileHierarchy = [&](const std::filesystem::path& filepath, const std::string& toDisplay, bool isFolder) -> void {
                    ImGui::PushID(filepath.string().c_str());

                    ImGuiTreeNodeFlags localFlags = flags;
                    if(!isFolder) {
                        localFlags |= ImGuiTreeNodeFlags_Bullet;
                    } else {
                        localFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                    }

                    if(filepath == currentFolderInDisk) {
                        localFlags |= ImGuiTreeNodeFlags_Selected;
                    }

                    const std::string imguiID = filepath.string();
                    const bool wasOpen = ImGui::TreeNodeBehaviorIsOpen(ImGui::GetID(imguiID.c_str()), localFlags);
                    const bool opened = ImGui::TreeNodeEx(imguiID.c_str(), localFlags, "");

                    auto vfsPath = GetVFS().represent(filepath);

                    if(!vfsPath.has_value()) {
                        return;
                    }

                    if(ImGui::BeginDragDropSource()) {
                        std::string str = vfsPath->toString();
                        ImGui::TextUnformatted(str.c_str());
                        ImGui::SetDragDropPayload(Carrot::Edition::DragDropTypes::FilePath, str.c_str(), str.length()*sizeof(char));
                        ImGui::EndDragDropSource();
                    }

                    const bool shouldOpenFolder = (wasOpen == opened || ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) && ImGui::IsItemClicked();
                    if(shouldOpenFolder && isFolder) {
                        if(vfsPath) {
                            updateCurrentFolder(vfsPath.value());
                        }
                    }

                    const ImVec2 imageSize = ImVec2(ImGui::GetFontSize(), ImGui::GetFontSize());
                    const float baseOffset = ImGui::GetFontSize() /* arrow */ + ImGui::GetStyle().FramePadding.x * 2;
                    const float textOffset = baseOffset + (ImGui::GetStyle().FramePadding.x) + imageSize.x;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + imageSize.x);
                    ImGui::SameLine();
                    ImGui::Image(getFileTexture(vfsPath.value()).getImguiID(), imageSize);
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
                    ImGui::SameLine();
                    ImGui::Text("%s", toDisplay.c_str());
                    if(opened) {
                        if(isFolder) {
                            std::error_code ec; // TODO: check
                            std::filesystem::directory_options options = std::filesystem::directory_options::none;
                            for (const auto& child: std::filesystem::directory_iterator(filepath, options, ec)) {
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
                                    const std::string filename = Carrot::toString(file.path().filename().u8string());
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
                const float padding = ImGui::GetStyle().CellPadding.y * 2;
                const float thumbnailSize = 100.0f;
                const float cellSize = thumbnailSize + padding;
                const float panelWidth = ImGui::GetContentRegionAvail().x;
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
                        std::string filename = entry.path.toString();
                        if(!searchFilter.PassFilter(filename.c_str())) {
                            continue;
                        }
                        ImGui::TableNextColumn();

                        ImGui::PushID(index);
                        {
                            if(drawFileTile(entry.path, thumbnailSize)) {
                                updateToPath = entry.path;
                            }
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

    bool ResourcePanel::drawFileTile(const Carrot::IO::VFS::Path& vfsPath, float tileSize) {
        static const char* windowID = "##file tile";
        const std::string filepath = vfsPath.toString();
        bool result = false;

        const ImVec2 padding = ImVec2(2.0f, 2.0f);
        const ImRect bb(ImGui::GetCurrentWindow()->DC.CursorPos, ImGui::GetCurrentWindow()->DC.CursorPos + ImVec2(tileSize, tileSize) + padding + padding);
        bool hovered = ImGui::IsMouseHoveringRect(bb.Min, bb.Max);
        bool held = hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);

        std::uint8_t styleVars = 0;
        std::uint8_t styleColors = 0;
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f); styleVars++;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding); styleVars++;
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f); styleVars++;

        const float alphaMultiplier = 0.5f;
        if(held) {
            ImColor baseColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonActive];
            baseColor.Value.w *= alphaMultiplier;
            ImGui::PushStyleColor(ImGuiCol_Border, (ImU32)baseColor); styleColors++;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, (ImU32)ImColor(baseColor.Value.x, baseColor.Value.y, baseColor.Value.z, baseColor.Value.w*0.5f)); styleColors++;
        } else if(hovered) {
            ImColor baseColor = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];
            baseColor.Value.w *= alphaMultiplier;
            ImGui::PushStyleColor(ImGuiCol_Border, (ImU32)baseColor); styleColors++;
            ImGui::PushStyleColor(ImGuiCol_ChildBg, (ImU32)ImColor(baseColor.Value.x, baseColor.Value.y, baseColor.Value.z, baseColor.Value.w*0.25f)); styleColors++;
        } else {
            ImColor baseColor = ImGui::GetStyle().Colors[ImGuiCol_Button];
            baseColor.Value.w *= alphaMultiplier;
            ImGui::PushStyleColor(ImGuiCol_Border, (ImU32)baseColor); styleColors++;
        }

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if(ImGui::BeginChild(windowID, ImVec2(tileSize, tileSize), true, windowFlags)) {
            const int lineCount = 2; // how many lines for the filename?

            if (ImGui::BeginDragDropSource()) {
                ImGui::TextUnformatted(filepath.c_str());
                ImGui::SetDragDropPayload(Carrot::Edition::DragDropTypes::FilePath, filepath.c_str(),
                                          filepath.length() * sizeof(char8_t));
                ImGui::EndDragDropSource();
            }

            float thumbnailSize = tileSize - (ImGui::GetFontSize() * lineCount + ImGui::GetStyle().ItemSpacing.y * (lineCount-1) + ImGui::GetStyle().WindowPadding.y * 2);
            ImGui::SameLine((tileSize - thumbnailSize) / 2);
            ImGui::Image(getFileTexture(vfsPath).getImguiID(), ImVec2(thumbnailSize, thumbnailSize));

            // TODO: center text
            auto filename = std::string(vfsPath.getPath().getFilename());
            ImGui::TextWrapped("%s", filename.c_str());

            if(ImGui::BeginPopupContextWindow()) {
                const Carrot::IO::FileFormat fileFormat = Carrot::IO::getFileFormat(vfsPath.getPath().c_str());
                const bool isSceneFolder = Carrot::Scene::isValidSceneFolder(vfsPath);
                if(Carrot::IO::isModelFormat(fileFormat)) {
                    fillModelContextPopup(vfsPath);
                } else if (isSceneFolder) {
                    fillPrefabContextPopup(vfsPath);
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(styleColors);
        ImGui::PopStyleVar(styleVars);

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", filepath.c_str());

            // TODO: preview when possible

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                if (std::filesystem::is_directory(GetVFS().resolve(vfsPath))) {
                    result = true;
                } else {
                    app.inspectorType = Application::InspectorType::Assets;
                    app.selectedAssetPaths.clear();
                    app.selectedAssetPaths.emplaceBack(vfsPath);
                }
            } else if(ImGui::GetIO().KeyCtrl && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                app.inspectorType = Application::InspectorType::Assets;
                app.selectedAssetPaths.emplaceBack(vfsPath);
            }
        }

        return result;
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

    void ResourcePanel::fillModelContextPopup(const Carrot::IO::VFS::Path& vfsPath) {
        if(ImGui::MenuItem("Add to scene")) {
            Carrot::ECS::Entity newEntity = app.addEntity()
                    .addComponent<Carrot::ECS::ModelComponent>();
            newEntity.updateName(vfsPath.getPath().getFilename());
            newEntity.getComponent<Carrot::ECS::ModelComponent>()->setFile(vfsPath);
        }
    }

    void ResourcePanel::fillPrefabContextPopup(const Carrot::IO::VFS::Path& vfsPath) {
        if(ImGui::MenuItem("Add to scene as prefab")) {
            auto pPrefab = GetAssetServer().loadPrefab(vfsPath);
            if(pPrefab) {
                pPrefab->instantiate(app.currentScene.world);
            } else {
                Carrot::Log::error("No prefab at %s", vfsPath.toString().c_str());
            }
        }
    }
}
