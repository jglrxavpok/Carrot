//
// Created by jglrxavpok on 29/09/2021.
//

#include "Peeler.h"
#include <engine/render/resources/Texture.h>
#include <engine/render/TextureRepository.h>
#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/SpriteComponent.h>
#include <engine/ecs/components/Kinematics.h>
#include <engine/ecs/components/ForceSinPosition.h>
#include <engine/ecs/components/AnimatedModelInstance.h>
#include <engine/ecs/components/LightComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>
#include <engine/ecs/systems/SpriteRenderSystem.h>
#include <engine/ecs/systems/ModelRenderSystem.h>
#include <engine/ecs/systems/RigidBodySystem.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <core/utils/ImGuiUtils.hpp>
#include <utility>
#include <rapidjson/filewritestream.h>
#include <rapidjson/prettywriter.h>
#include <core/io/IO.h>
#include <core/io/Files.h>
#include <core/utils/stringmanip.h>
#include <engine/edition/DragDropTypes.h>
#include <core/io/Logging.hpp>
#include <engine/ecs/systems/SystemKinematics.h>
#include <engine/ecs/systems/SystemSinPosition.h>
#include <engine/ecs/systems/SystemHandleLights.h>
#include "ecs/systems/LightEditorRenderer.h"
#include "ecs/systems/CollisionShapeRenderer.h"
#include <engine/physics/PhysicsSystem.h>

namespace Peeler {

    void Application::onFrame(Carrot::Render::Context renderContext) {
        ZoneScoped;
        if(&renderContext.viewport == &engine.getMainViewport()) {
            ZoneScopedN("Main viewport");
            UIEditor(renderContext);
            Tools::ProjectMenuHolder::onFrame(renderContext);
        }
        if(&renderContext.viewport == &gameViewport) {
            ZoneScopedN("Game viewport");
            auto viewportSize = engine.getVulkanDriver().getFinalRenderSize();
            cameraController.applyTo(glm::vec2{ viewportSize.width, viewportSize.height }, gameViewport.getCamera());

            currentScene.onFrame(renderContext);

            float gridSize = 100.0f;
            float cellSize = 1.0f;
            float lineWidth = 0.005f;
            glm::vec4 gridColor = {0.5f, 0.5f, 0.5f, 1.0f};
            gridRenderer.render(renderContext, gridColor, lineWidth, cellSize, gridSize);

            gridColor = {0.1f, 0.1f, 0.9f, 1.0f};
            gridRenderer.render(renderContext, gridColor, 2.0f*lineWidth, gridSize/2.0f, gridSize);

            engine.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
                gameRenderingGraph->execute(renderContext, cmds);
                auto& texture = gameRenderingGraph->getTexture(gameTexture, renderContext.swapchainIndex);
                texture.assumeLayout(vk::ImageLayout::eColorAttachmentOptimal);
                texture.transitionInline(cmds, vk::ImageLayout::eShaderReadOnlyOptimal);
            }, false);
        }
    }

    void Application::UIEditor(const Carrot::Render::Context& renderContext) {
        ZoneScoped;
        if(tryToClose) {
            openUnsavedChangesPopup([&]() {
                requestShutdown();
            }, [&]() {
                tryToClose = false;
            });
            tryToClose = false;
        }

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        windowFlags |= ImGuiWindowFlags_NoBackground;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Editor window", nullptr, windowFlags);

        ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_PassthruCentralNode;
        mainDockspace = ImGui::GetID("PeelerMainDockspace");
        ImGui::DockSpace(mainDockspace, ImVec2(0.0f, 0.0f), dockspaceFlags);

        ImGui::PopStyleVar(3);

        if (ImGui::BeginMenuBar()) {
            if(ImGui::BeginMenu("Project")) {
                drawProjectMenu();
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu("Tests")) {
                if(ImGui::MenuItem("Activate Physics debug")) {
                    GetPhysics().setViewport(&gameViewport);
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if(ImGui::Begin("Game view", nullptr, ImGuiWindowFlags_NoBackground)) {
            UIGameView(renderContext);
        }
        ImGui::End();
        ImGui::PopStyleVar(3);

        ImGui::End();

        if(ImGui::Begin("World")) {
            UIWorldHierarchy(renderContext);
        }
        ImGui::End();

        if(ImGui::Begin("Resources")) {
            UIResourcesPanel(renderContext);
        }
        ImGui::End();

        if(ImGui::Begin("Properties")) {
            UIInspector(renderContext);
        }
        ImGui::End();

        if(ImGui::Begin("PlayBar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            UIPlayBar(renderContext);
        }
        ImGui::End();

        if(ImGui::Begin("Scene properties")) {
            UISceneProperties(renderContext);
        }
        ImGui::End();
    }

    void Application::UISceneProperties(const Carrot::Render::Context& renderContext) {
        if(ImGui::CollapsingHeader("Lighting")) {
            if(ImGui::BeginMenu("Ambient color")) {
                auto& ambientLight = GetRenderer().getLighting().getAmbientLight();
                float color[3] = { ambientLight.r, ambientLight.g, ambientLight.b };
                if(ImGui::ColorPicker3("Ambient color picker", color)) {
                    ambientLight = { color[0], color[1], color[2] };
                    currentScene.lighting.ambient = ambientLight;
                    markDirty();
                }
                ImGui::EndMenu();
            }

            bool supportsRaytracing = false;
            if(!supportsRaytracing) {
                ImGui::BeginDisabled();
            }
            if(ImGui::Checkbox("Raytraced shadows", &currentScene.lighting.raytracedShadows)) {
                // TODO: tell shader
            }
            if(!supportsRaytracing) {
                ImGui::EndDisabled();
            }
        }

        const char* preview = Carrot::Skybox::getName(GetEngine().getSkybox());
        if(ImGui::BeginCombo("Skybox", preview)) {
            auto option = [&](Carrot::Skybox::Type skybox) {
                const char* name = Carrot::Skybox::getName(skybox);
                bool selected = skybox == GetEngine().getSkybox();
                if(ImGui::Selectable(name, selected)) {
                    GetEngine().setSkybox(skybox);
                }
            };
            option(Carrot::Skybox::Type::None);
            option(Carrot::Skybox::Type::Forest);

            ImGui::EndCombo();
        }
    }

    void Application::UIResourcesPanel(const Carrot::Render::Context& renderContext) {
        if(currentFolder.has_parent_path()) {
            bool isAtRoot = Carrot::IO::Files::isRootFolder(currentFolder);
            if(ImGui::ImageButton(parentFolderIcon.getImguiID(), ImVec2(16, 16))) {
                if(isAtRoot) {
                    auto roots = Carrot::IO::Files::enumerateRoots();
                    if(roots.size() == 1) {
                        updateCurrentFolder(roots[0]);
                    } else { // Windows can have multiple drives
                        isLookingAtRoots = true;
                        resourcesInCurrentFolder.clear();
                        for(const auto& root : roots) {
                            resourcesInCurrentFolder.emplace_back(ResourceType::Drive, root);
                        }
                    }
                } else {
                    updateCurrentFolder(currentFolder.parent_path());
                }
            }
        }
        ImGui::SameLine();
        ImGui::UnformattedWText(currentFolder.u8string());

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
        if(ImGui::BeginTable("##resources table", columnCount, tableFlags)) {
            ImGui::TableNextRow();
            std::uint32_t index = 0;

            std::optional<std::filesystem::path> updateToPath;
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
                std::u8string filename = entry.path.u8string();
                ImGui::PushID(index);
                {
                    ImGui::ImageButton(texture, ImVec2(thumbnailSize, thumbnailSize));

                    if(ImGui::BeginDragDropSource()) {
                        ImGui::UnformattedWText(filename);
                        ImGui::SetDragDropPayload(Carrot::Edition::DragDropTypes::FilePath, filename.c_str(), filename.length()*sizeof(char8_t));
                        ImGui::EndDragDropSource();
                    }

                    if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        if(std::filesystem::is_directory(entry.path)) {
                            updateToPath = entry.path;
                        }
                    }

                    std::u8string smallFilename;
                    if(!isLookingAtRoots) {
                        smallFilename = entry.path.filename().u8string();
                    } else {
                        smallFilename = entry.path.u8string();
                    }
                    ImGui::UnformattedWTextWrapped(smallFilename);
                }
                ImGui::PopID();

                if(++index == columnCount) {
                    index = 0;
                    ImGui::TableNextRow();
                }
            }

            if(updateToPath) {
                updateCurrentFolder(updateToPath.value());
            }

            ImGui::EndTable();
        }
    }

    void Application::UIInspector(const Carrot::Render::Context& renderContext) {
        ZoneScoped;
        if(selectedID.has_value()) {
            auto& entityID = selectedID.value();
            auto entity = currentScene.world.wrap(entityID);
            auto& str = entity.getName();
            ImGui::InputText("Entity name##entity name field inspector", str);

            auto components = currentScene.world.getAllComponents(entityID);

            std::unordered_set<Carrot::ComponentID> toRemove;
            for(auto& comp : components) {
                bool shouldKeep = true;
                bool modified = false;
                comp->drawInspector(renderContext, shouldKeep, modified);
                if(!shouldKeep) {
                    toRemove.insert(comp->getComponentTypeID());
                }

                if(modified) {
                    markDirty();
                }
            }

            for(const auto& id : toRemove) {
                markDirty();
                entity.removeComponent(id);
            }

            if(ImGui::Button("Add component##inspector add component")) {
                ImGui::OpenPopup("Add component##Add component popup");
            }

            if(ImGui::BeginPopup("Add component##Add component popup")) {
                const auto& lib = Carrot::ECS::getComponentLibrary();

                std::unordered_set<std::string> componentsEntityHas;
                for(const auto* comp : entity.getAllComponents()) {
                    componentsEntityHas.insert(comp->getName());
                }
                for(const auto& compID : lib.getAllIDs()) {
                    std::string id = compID;

                    if(componentsEntityHas.contains(id)) {
                        continue;
                    }

                    id += "##add component menu item inspector";
                    if(ImGui::MenuItem(id.c_str())) {
                        auto comp = lib.create(compID, entity);
                        entity.addComponent(std::move(comp));
                        markDirty();
                    }
                }
                ImGui::EndPopup();
            }
        }
    }

    void Application::UIWorldHierarchy(const Carrot::Render::Context& renderContext) {
        ZoneScoped;
        if(ImGui::IsItemClicked()) {
            deselectAllEntities();
        }

        if (ImGui::BeginPopupContextWindow("##popup world editor")) {
            if(ImGui::BeginMenu("Add entity")) {
                addEntityMenu();
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        std::function<void(Carrot::ECS::Entity&)> showEntityTree = [&] (Carrot::ECS::Entity& entity) {
            if(!entity)
                return;
            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            if(selectedID.has_value() && selectedID.value() == entity.getID()) {
                nodeFlags |= ImGuiTreeNodeFlags_Selected;
            }
            auto children = currentScene.world.getChildren(entity);

            auto addChildMenu = [&]() {
                std::string id = "##add child to entity ";
                id += std::to_string(entity.getID().hash());
                if(ImGui::BeginPopupContextItem(id.c_str())) {
                    if(ImGui::BeginMenu("Add child##add child to entity world hierarchy")) {
                        addEntityMenu(entity);
                        ImGui::EndMenu();
                    }
                    if(ImGui::MenuItem("Duplicate##duplicate entity world hierarchy")) {
                        duplicateEntity(entity);
                    }
                    if(ImGui::MenuItem("Remove##remove entity world hierarchy")) {
                        removeEntity(entity);
                    }
                    ImGui::EndPopup();
                }
            };

            if(!children.empty()) {
                if(ImGui::TreeNodeEx((void*)entity.getID().hash(), nodeFlags, "%s", currentScene.world.getName(entity).c_str())) {
                    addChildMenu();
                    if(ImGui::IsItemClicked()) {
                        deselectAllEntities(); // TODO: multi-select
                        selectEntity(entity.getID());
                    }

                    for(auto& c : children) {
                        showEntityTree(c);
                    }

                    ImGui::TreePop();
                }
            } else { // has no children
                nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                ImGui::TreeNodeEx((void*)entity.getID().hash(), nodeFlags, "%s", currentScene.world.getName(entity).c_str());

                addChildMenu();
                if(ImGui::IsItemClicked()) {
                    deselectAllEntities(); // TODO: multi-select
                    selectEntity(entity.getID());
                }
            }
        };
        for(auto& entityObj : currentScene.world.getAllEntities()) {
            if( ! currentScene.world.getParent(entityObj)) {
                showEntityTree(entityObj);
            }
        }
    }

    void Application::UIGameView(const Carrot::Render::Context& renderContext) {
        ZoneScoped;
        ImGuizmo::BeginFrame();
        
        ImVec2 entireRegion = ImGui::GetContentRegionAvail();
        auto& texture = gameRenderingGraph->getTexture(gameTexture, renderContext.swapchainIndex);

        float startX = ImGui::GetCursorScreenPos().x;
        float startY = ImGui::GetCursorScreenPos().y;
        ImGui::Image(texture.getImguiID(), entireRegion);
        movingGameViewCamera = ImGui::IsItemClicked(ImGuiMouseButton_Right);

        auto& camera = gameViewport.getCamera();
        glm::mat4 identityMatrix = glm::identity<glm::mat4>();
        glm::mat4 cameraView = camera.computeViewMatrix();
        glm::mat4 cameraProjection = camera.getProjectionMatrix();
        cameraProjection[1][1] *= -1;

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(startX, startY, ImGui::GetWindowContentRegionMax().x, ImGui::GetWindowContentRegionMax().y);

        float* cameraViewImGuizmo = glm::value_ptr(cameraView);
        float* cameraProjectionImGuizmo = glm::value_ptr(cameraProjection);

        bool usingGizmo = false;
        if(selectedID.has_value()) {
            auto transformRef = currentScene.world.getComponent<Carrot::ECS::TransformComponent>(selectedID.value());
            if(transformRef) {
                glm::mat4 transformMatrix = transformRef->toTransformMatrix();

                static ImGuizmo::OPERATION gizmoOperation = ImGuizmo::OPERATION::TRANSLATE;

                ImVec2 selectionWindowPos = ImVec2(startX, startY);

                // some padding
                selectionWindowPos.x += 10.0f;
                selectionWindowPos.y += 10.0f;

                ImGui::SetNextWindowPos(selectionWindowPos, ImGuiCond_Always);
                ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking
                        | ImGuiWindowFlags_NoTitleBar
                        | ImGuiWindowFlags_NoCollapse
                        | ImGuiWindowFlags_NoScrollbar
                        | ImGuiWindowFlags_AlwaysAutoResize
                ;
                if(ImGui::Begin("##gizmo operation select window", nullptr, windowFlags)) {
                    auto setupSelectedBg = [](bool selected) {
                        if(selected) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0.5,0.6));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0.5,0.8));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0.5,1.0));
                        } else {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0.3,0.0));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0.3,0.3));
                            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0.3,0.5));
                        }
                    };

                    const float iconSize = 32.0f;

                    setupSelectedBg(gizmoOperation == ImGuizmo::OPERATION::TRANSLATE);
                    if(ImGui::ImageButton(translateIcon.getImguiID(), ImVec2(iconSize, iconSize))) {
                        gizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine();
                    setupSelectedBg(gizmoOperation == ImGuizmo::OPERATION::ROTATE);
                    if(ImGui::ImageButton(rotateIcon.getImguiID(), ImVec2(iconSize, iconSize))) {
                        gizmoOperation = ImGuizmo::OPERATION::ROTATE;
                    }
                    ImGui::PopStyleColor(3);

                    ImGui::SameLine();
                    setupSelectedBg(gizmoOperation == ImGuizmo::OPERATION::SCALE);
                    if(ImGui::ImageButton(scaleIcon.getImguiID(), ImVec2(iconSize, iconSize))) {
                        gizmoOperation = ImGuizmo::OPERATION::SCALE;
                    }
                    ImGui::PopStyleColor(3);
                }
                ImGui::End();

                ImGuizmo::MODE gizmoMode = ImGuizmo::MODE::LOCAL;

                bool used = ImGuizmo::Manipulate(
                        cameraViewImGuizmo,
                        cameraProjectionImGuizmo,
                        gizmoOperation,
                        gizmoMode,
                        glm::value_ptr(transformMatrix)
                );

                if(used) {
                    glm::mat4 parentMatrix = glm::identity<glm::mat4>();
                    auto parentEntity = transformRef->getEntity().getParent();
                    if(parentEntity) {
                        auto parentTransform = parentEntity->getComponent<Carrot::ECS::TransformComponent>();
                        if(parentTransform) {
                            parentMatrix = parentTransform->toTransformMatrix();
                        }
                    }

                    glm::mat4 localTransform = glm::inverse(parentMatrix) * transformMatrix;
                    float translation[3] = {0.0f};
                    float scale[3] = {0.0f};
                    float rotation[3] = {0.0f};
                    ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localTransform), translation, rotation, scale);

                    transformRef->localTransform.position = glm::vec3(translation[0], translation[1], translation[2]);
                    transformRef->localTransform.rotation = glm::quat(glm::radians(glm::vec3(rotation[0], rotation[1], rotation[2])));
                    transformRef->localTransform.scale = glm::vec3(scale[0], scale[1], scale[2]);

                    markDirty();
                }

                usingGizmo = ImGuizmo::IsUsing() || ImGuizmo::IsOver();
            }
        }

        movingGameViewCamera &= !usingGizmo;

        if(movingGameViewCamera) {
            if(!engine.isGrabbingCursor()) {
                engine.grabCursor();
            }
        } else if(!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            engine.ungrabCursor();
        } else if(engine.isGrabbingCursor()) {
            movingGameViewCamera = true;
        }

        if(!usingGizmo && ImGui::IsItemClicked()) {
            deselectAllEntities(); // TODO: multi-select
            const auto gbufferPass = gameRenderingGraph->getPassData<Carrot::Render::PassData::GBuffer>("gbuffer").value();
            const auto& entityIDTexture = engine.getVulkanDriver().getTextureRepository().get(gbufferPass.entityID, renderContext.lastSwapchainIndex);
            glm::vec2 uv { ImGui::GetMousePos().x, ImGui::GetMousePos().y };
            uv -= glm::vec2 { startX, startY };
            uv /= glm::vec2 { ImGui::GetWindowWidth(), ImGui::GetWindowHeight() };
            if(uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
                return;
            glm::vec<4, std::uint32_t> sample = entityIDTexture.sampleUVec4(uv.x, uv.y);
            Carrot::UUID uuid { sample[0], sample[1], sample[2], sample[3] };
            if(currentScene.world.exists(uuid)) {
                selectEntity(uuid);
            }
        }

        /* TODO: fix resize of game view
        bool requireResize = entireRegion.x != previousGameViewWidth || entireRegion.y != previousGameViewHeight;
        if(requireResize) {
            gameRenderingGraph->onSwapchainSizeChange(static_cast<std::uint32_t>(entireRegion.x), static_cast<std::uint32_t>(entireRegion.y));

            previousGameViewWidth = entireRegion.x;
            previousGameViewHeight = entireRegion.y;

        }*/
    }

    void Application::startSimulation() {
        isPlaying = true;
        savedScene = currentScene;

        savedScene.unload();

        currentScene.world.unfreezeLogic();
        currentScene.world.removeRenderSystem<Peeler::ECS::LightEditorRenderer>();
        currentScene.world.removeRenderSystem<Peeler::ECS::CollisionShapeRenderer>();
        updateSettingsBasedOnScene();
        GetPhysics().resume();
    }

    void Application::stopSimulation() {
        isPlaying = false;
        savedScene.load();
        currentScene = savedScene;
        savedScene.clear();
        currentScene.world.addRenderSystem<Peeler::ECS::CollisionShapeRenderer>();
        currentScene.world.addRenderSystem<Peeler::ECS::LightEditorRenderer>();
        updateSettingsBasedOnScene();
        GetPhysics().pause();
    }

    void Application::updateSettingsBasedOnScene() {
        GetRenderer().getLighting().getAmbientLight() = currentScene.lighting.ambient;
        // TODO: raytraced shadows
    }

    void Application::UIPlayBar(const Carrot::Render::Context& renderContext) {
        float smallestDimension = std::min(ImGui::GetContentRegionMax().x, ImGui::GetContentRegionMax().y);
        auto& textureToDisplay = isPlaying ? stopButtonIcon : playButtonIcon;

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0.1,0.6));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0.1,0.3));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 0.0f));

        ImGui::SameLine((ImGui::GetContentRegionMax().x - smallestDimension) / 2.0f );
        if(ImGui::ImageButton(textureToDisplay.getImguiID(), ImVec2(smallestDimension, smallestDimension))) {
            if(isPlaying) {
                stopSimulation();
            } else {
                startSimulation();
            }
        }

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    Application::Application(Carrot::Engine& engine): Carrot::CarrotGame(engine), Tools::ProjectMenuHolder(), settings("peeler"), gameViewport(engine.createViewport()),
        playButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/play_button.png"),
        stopButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/stop_button.png"),
        translateIcon(engine.getVulkanDriver(), "resources/textures/ui/translate.png"),
        rotateIcon(engine.getVulkanDriver(), "resources/textures/ui/rotate.png"),
        scaleIcon(engine.getVulkanDriver(), "resources/textures/ui/scale.png"),
        folderIcon(engine.getVulkanDriver(), "resources/textures/ui/folder.png"),
        genericFileIcon(engine.getVulkanDriver(), "resources/textures/ui/file.png"),
        parentFolderIcon(engine.getVulkanDriver(), "resources/textures/ui/parent_folder.png"),
        driveIcon(engine.getVulkanDriver(), "resources/textures/ui/drive.png")


    {
        GetEngine().setSkybox(Carrot::Skybox::Type::Forest);
        attachSettings(settings);

        {
            moveCamera.suggestBinding(Carrot::IO::GLFWGamepadVec2Binding(0, Carrot::IO::GameInputVectorType::LeftStick));
            moveCamera.suggestBinding(Carrot::IO::GLFWKeysVec2Binding(Carrot::IO::GameInputVectorType::WASD));

            turnCamera.suggestBinding(Carrot::IO::GLFWGamepadVec2Binding(0, Carrot::IO::GameInputVectorType::RightStick));
            turnCamera.suggestBinding(Carrot::IO::GLFWKeysVec2Binding(Carrot::IO::GameInputVectorType::ArrowKeys));
            turnCamera.suggestBinding(Carrot::IO::GLFWGrabbedMouseDeltaBinding);

            moveCameraUp.suggestBinding(Carrot::IO::GLFWGamepadAxisBinding(0, GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER));
            moveCameraUp.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_SPACE));

            moveCameraDown.suggestBinding(Carrot::IO::GLFWGamepadAxisBinding(0, GLFW_GAMEPAD_AXIS_LEFT_TRIGGER));
            moveCameraDown.suggestBinding(Carrot::IO::GLFWKeyBinding(GLFW_KEY_LEFT_SHIFT));

            editorActions.add(moveCamera);
            editorActions.add(moveCameraDown);
            editorActions.add(moveCameraUp);
            editorActions.add(turnCamera);
            editorActions.activate();
        }

        addDefaultSystems(currentScene, true);
        currentScene.world.freezeLogic();

        Carrot::Render::GraphBuilder graphBuilder(engine.getVulkanDriver());

        auto& resolvePass = engine.fillInDefaultPipeline(graphBuilder, Carrot::Render::Eye::NoVR,
                                     [&](const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context& frame, vk::CommandBuffer& cmds) {
                                         currentScene.world.recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                         GetRenderer().recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                     },
                                     [&](const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context& frame, vk::CommandBuffer& cmds) {
                                         currentScene.world.recordTransparentGBufferPass(pass.getRenderPass(), frame, cmds);
                                         GetRenderer().recordTransparentGBufferPass(pass.getRenderPass(), frame, cmds);
                                     });
        gameTexture = resolvePass.getData().resolved;

        engine.getVulkanDriver().getTextureRepository().getUsages(gameTexture.rootID) |= vk::ImageUsageFlagBits::eSampled;
        const auto gbufferPass = graphBuilder.getPassData<Carrot::Render::PassData::GBuffer>("gbuffer").value();
        engine.getVulkanDriver().getTextureRepository().getUsages(gbufferPass.entityID.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;

        gameRenderingGraph = std::move(graphBuilder.compile());

        gameViewport.getCamera().setTargetAndPosition(glm::vec3(), glm::vec3(2,-5,5));

        // register components for serialisation
        {
            auto& lib = Carrot::ECS::getComponentLibrary();
            lib.addUniquePtrBased<Carrot::ECS::TransformComponent>();
            lib.addUniquePtrBased<Carrot::ECS::Kinematics>();
            lib.addUniquePtrBased<Carrot::ECS::SpriteComponent>();
            lib.addUniquePtrBased<Carrot::ECS::ModelComponent>();
            //lib.addUniquePtrBased<Carrot::ECS::AnimatedModelInstance>();
            lib.addUniquePtrBased<Carrot::ECS::ForceSinPosition>();
            lib.addUniquePtrBased<Carrot::ECS::LightComponent>();
            lib.addUniquePtrBased<Carrot::ECS::RigidBodyComponent>();
        }

        settings.load();
        settings.useMostRecent();
        if(settings.currentProject) {
            scheduleLoad(settings.currentProject.value());
        }

        updateCurrentFolder(std::filesystem::current_path() / "resources");
    }

    void Application::tick(double frameTime) {
        currentScene.tick(frameTime);

        if(movingGameViewCamera) {
            cameraController.move(moveCamera.getValue().x, moveCamera.getValue().y, moveCameraUp.getValue() - moveCameraDown.getValue(),
                turnCamera.getValue().x, turnCamera.getValue().y,
                frameTime);
        }
    }

    void Application::recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                                              vk::CommandBuffer& commands) {
        // no op, everything is done inside gameViewport
    }

    void Application::recordTransparentGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                                                   vk::CommandBuffer& commands) {
        // no op, everything is done inside gameViewport
    }

    void Application::performLoad(std::filesystem::path fileToOpen) {
        if(fileToOpen == EmptyProject) {
            currentScene.clear();
            addDefaultSystems(currentScene, true);
            hasUnsavedChanges = false;
            settings.currentProject.reset();
            deselectAllEntities();
            updateWindowTitle();
            return;
        }
        rapidjson::Document description;
        description.Parse(Carrot::IO::readFileAsText(fileToOpen.string()).c_str());

        currentScene.deserialise(description["scene"].GetObject());
        addDefaultSystems(currentScene, true); // TODO: load systems from scene
        currentScene.world.freezeLogic();
        updateSettingsBasedOnScene();
        if(description.HasMember("camera")) {
            cameraController.deserialise(description["camera"].GetObject());
        }

        if(description.HasMember("window")) {
            auto windowObj = description["window"].GetObject();
            std::int32_t x = windowObj["x"].GetInt();
            std::int32_t y = windowObj["y"].GetInt();
            std::int32_t width = windowObj["width"].GetInt();
            std::int32_t height = windowObj["height"].GetInt();

            bool maximised = windowObj["maximised"].GetBool();

            auto& window = engine.getVulkanDriver().getWindow();
            window.setPosition(x, y);
            window.setWindowSize(width, height);
            if(maximised) {
                window.maximise();
            }
        }

        settings.currentProject = fileToOpen;
        settings.addToRecentProjects(fileToOpen);
        hasUnsavedChanges = false;

        if(description.HasMember("selected")) {
            auto wantedSelection = Carrot::UUID::fromString(description["selected"].GetString());
            if(currentScene.world.exists(wantedSelection)) {
                selectEntity(wantedSelection);
            } else {
                deselectAllEntities();
            }
        } else {
            deselectAllEntities();
        }
        updateWindowTitle();
    }

    void Application::saveToFile(std::filesystem::path path) {
        FILE* fp = fopen(path.string().c_str(), "wb"); // non-Windows use "w"

        char writeBuffer[65536];
        rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

        rapidjson::Document document;
        document.SetObject();

        {
            rapidjson::Value scene(rapidjson::kObjectType);
            document.AddMember("scene", currentScene.serialise(document), document.GetAllocator());
        }

        if(selectedID) {
            rapidjson::Value uuid(selectedID.value().toString(), document.GetAllocator());
            document.AddMember("selectedID", uuid, document.GetAllocator());
        }

        {
            document.AddMember("camera", cameraController.serialise(document), document.GetAllocator());
        }

        {
            rapidjson::Value windowObj(rapidjson::kObjectType);

            const auto& window = engine.getVulkanDriver().getWindow();

            std::int32_t x, y;
            std::int32_t w, h;
            window.getPosition(x, y);
            window.getWindowSize(w, h);

            bool maximised = window.isMaximised();
            windowObj.AddMember("x", x, document.GetAllocator());
            windowObj.AddMember("y", y, document.GetAllocator());
            windowObj.AddMember("width", w, document.GetAllocator());
            windowObj.AddMember("height", h, document.GetAllocator());
            windowObj.AddMember("maximised", maximised, document.GetAllocator());

            document.AddMember("window", windowObj, document.GetAllocator());
        }

        document.Accept(writer);
        fclose(fp);

        hasUnsavedChanges = false;
        settings.currentProject = path;
        settings.addToRecentProjects(path);
        updateWindowTitle();
    }

    bool Application::showUnsavedChangesPopup() {
        return hasUnsavedChanges;
    }

    bool Application::canSave() const {
        return !isPlaying;
    }

    void Application::onSwapchainSizeChange(int newWidth, int newHeight) {
        gameRenderingGraph->onSwapchainSizeChange(newWidth, newHeight); // TODO: fix resize of game view
    }

    void Application::onSwapchainImageCountChange(std::size_t newCount) {
        gameRenderingGraph->onSwapchainImageCountChange(newCount);
    }

    void Application::addEntityMenu(std::optional<Carrot::ECS::Entity> parent) {
        if(ImGui::MenuItem("Add Cube##add entity menu")) {
            auto entity = addEntity(parent);
            entity.addComponent<Carrot::ECS::ModelComponent>();
            Carrot::ECS::ModelComponent& modelComp = entity.getComponent<Carrot::ECS::ModelComponent>();
            modelComp.model = GetRenderer().getOrCreateModel("resources/models/simple_cube.obj");
        }

        if(ImGui::MenuItem("Add Light##add entity menu")) {
            auto entity = addEntity(parent);
            entity.addComponent<Carrot::ECS::LightComponent>();
        }

        ImGui::Separator();

        if(ImGui::MenuItem("Add empty##add entity menu")) {
            DISCARD(addEntity(parent));
        }
    }

    Carrot::ECS::Entity Application::addEntity(std::optional<Carrot::ECS::Entity> parent) {
        auto entity = currentScene.world.newEntity("Entity")
                .addComponent<Carrot::ECS::TransformComponent>();

        if(parent) {
            entity.setParent(parent);
        }

        selectEntity(entity.getID());

        markDirty();
        return entity;
    }

    void Application::duplicateEntity(const Carrot::ECS::Entity& entity, std::optional<Carrot::ECS::Entity> parent) {
        auto clone = currentScene.world.newEntity(std::string(entity.getName())+" (Copy)");
        for(const auto* comp : entity.getAllComponents()) {
            clone.addComponent(std::move(comp->duplicate(clone)));
        }

        for(const auto& child : currentScene.world.getChildren(entity)) {
            duplicateEntity(child, clone);
        }

        if(parent) {
            clone.setParent(*parent);
        } else {
            clone.setParent(entity.getParent());
        }

        selectEntity(entity.getID());
        markDirty();
    }

    void Application::selectEntity(const Carrot::ECS::EntityID& entity) {
        selectedID = entity;

        auto* shapeRenderer = currentScene.world.getRenderSystem<ECS::CollisionShapeRenderer>();
        if(shapeRenderer) {
            shapeRenderer->setSelected(selectedID);
        }
    }

    void Application::deselectAllEntities() {
        selectedID.reset();

        auto* shapeRenderer = currentScene.world.getRenderSystem<ECS::CollisionShapeRenderer>();
        if(shapeRenderer) {
            shapeRenderer->setSelected(selectedID);
        }
    }

    void Application::removeEntity(const Carrot::ECS::Entity& entity) {
        if(selectedID.has_value() && entity.getID() == selectedID.value()) {
            deselectAllEntities();
        }
        currentScene.world.removeEntity(entity);
        markDirty();
    }

    void Application::drawCantSavePopup() {
        ImGui::Text("Cannot save while game is running.");
        ImGui::Text("Please stop the game before saving.");
    }

    void Application::markDirty() {
        hasUnsavedChanges = true;
        updateWindowTitle();
    }

    void Application::updateWindowTitle() {
        std::string projectName = "Untitled";
        if(settings.currentProject) {
            projectName = settings.currentProject.value().stem().string();
        }
        engine.getVulkanDriver().getWindow().setTitle(Carrot::sprintf("Peeler - %s%s", projectName.c_str(), hasUnsavedChanges ? "*" : ""));
    }

    static std::shared_ptr<Carrot::Render::LightHandle> lightHandle = nullptr;

    void Application::addDefaultSystems(Scene& scene, bool editingScene) {
        scene.world.addRenderSystem<Carrot::ECS::SpriteRenderSystem>();
        scene.world.addRenderSystem<Carrot::ECS::ModelRenderSystem>();
        scene.world.addRenderSystem<Carrot::ECS::SystemHandleLights>();
        scene.world.addLogicSystem<Carrot::ECS::SystemKinematics>();
        scene.world.addLogicSystem<Carrot::ECS::SystemSinPosition>();
        scene.world.addLogicSystem<Carrot::ECS::RigidBodySystem>();

        if(editingScene) {
            scene.world.addRenderSystem<Peeler::ECS::LightEditorRenderer>();
            scene.world.addRenderSystem<Peeler::ECS::CollisionShapeRenderer>();
        }


        GetRenderer().getLighting().getAmbientLight() = glm::vec3 {0.1,0.1,0.1};
    }

    void Application::updateCurrentFolder(std::filesystem::path path) {
        currentFolder = std::move(path);
        resourcesInCurrentFolder.clear();
        for(const auto& file : std::filesystem::directory_iterator(currentFolder)) {
            ResourceType type = file.is_directory() ? ResourceType::Folder : ResourceType::GenericFile;
            resourcesInCurrentFolder.emplace_back(type, file.path());
        }

        isLookingAtRoots = false;
    }

    bool Application::onCloseButtonPressed() {
        if(hasUnsavedChanges) {
            tryToClose = true;
            return false;
        }
        return true;
    }

}