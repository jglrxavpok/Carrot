//
// Created by jglrxavpok on 29/09/2021.
//

#include "Peeler.h"
#include <engine/render/resources/Texture.h>
#include <engine/render/TextureRepository.h>
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
#include <engine/physics/PhysicsSystem.h>

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
#include <engine/ecs/systems/SystemKinematics.h>
#include <engine/ecs/systems/SystemSinPosition.h>
#include <engine/ecs/systems/SystemHandleLights.h>
#include <engine/ecs/systems/SystemTransformSwapBuffers.h>
#include <engine/ecs/systems/LuaSystems.h>
#include "ecs/systems/LightEditorRenderer.h"
#include "ecs/systems/CollisionShapeRenderer.h"
#include "ecs/systems/CameraRenderer.h"

#include "game_specific/ecs/CharacterControllerComponent.h"
#include "game_specific/ecs/PageComponent.h"
#include "game_specific/ecs/CharacterControllerSystem.h"

namespace Peeler {

    void Application::setupCamera(Carrot::Render::Context renderContext) {
        if(&renderContext.viewport == &gameViewport) {
            auto viewportSize = engine.getVulkanDriver().getFinalRenderSize();
            cameraController.applyTo(glm::vec2{ gameViewport.getWidth(), gameViewport.getHeight() }, gameViewport.getCamera());

            currentScene.setupCamera(renderContext);

            if(!isPlaying) {
                // override any primary camera the game might have
                cameraController.applyTo(glm::vec2{ gameViewport.getWidth(), gameViewport.getHeight() }, gameViewport.getCamera());
            }
        }
    }

    void Application::onFrame(Carrot::Render::Context renderContext) {
        ZoneScoped;
        if(&renderContext.viewport == &engine.getMainViewport()) {
            ZoneScopedN("Main viewport");
            UIEditor(renderContext);
            Tools::ProjectMenuHolder::onFrame(renderContext);

            // TODO: do it more cleanly with shortcuts for multiple actions
            if(glfwGetKey(GetVulkanDriver().getWindow().getGLFWPointer(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && glfwGetKey(GetVulkanDriver().getWindow().getGLFWPointer(), GLFW_KEY_S) == GLFW_PRESS) {
                if(canSave()) {
                    this->triggerSave();
                }
            }
        }
        if(&renderContext.viewport == &gameViewport) {
            ZoneScopedN("Game viewport");

            if(stopSimulationRequested) {
                performSimulationStop();
            } else if(startSimulationRequested) {
                performSimulationStart();
            }

            auto viewportSize = engine.getVulkanDriver().getFinalRenderSize();
            cameraController.applyTo(glm::vec2{ gameViewport.getWidth(), gameViewport.getHeight() }, gameViewport.getCamera());

            currentScene.onFrame(renderContext);

            if(!isPlaying) {
                // override any primary camera the game might have
                cameraController.applyTo(glm::vec2{ gameViewport.getWidth(), gameViewport.getHeight() }, gameViewport.getCamera());
            }

            float gridSize = 100.0f;
            float cellSize = 1.0f;
            float lineWidth = 0.005f;
            glm::vec4 gridColor = {0.5f, 0.5f, 0.5f, 1.0f};
            gridRenderer.render(renderContext, gridColor, lineWidth, cellSize, gridSize);

            gridColor = {0.1f, 0.1f, 0.9f, 1.0f};
            gridRenderer.render(renderContext, gridColor, 2.0f*lineWidth, gridSize/2.0f, gridSize);
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

            static bool showDemo = false;

            if(ImGui::BeginMenu("Tests")) {
                if(ImGui::MenuItem("Activate Physics debug")) {
                    GetPhysics().setViewport(&gameViewport);
                }

                if(ImGui::MenuItem("Show ImGui demo", nullptr, showDemo)) {
                    showDemo = !showDemo;
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();

            if(showDemo) {
                ImGui::ShowDemoWindow();
            }
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
            resourcePanel.draw(renderContext);
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

        if(wantsToLoadProject && projectToLoad == EmptyProject) {
            static const char* newProjectModalID = "New project";
            ImGui::OpenPopup(newProjectModalID);

            if(ImGui::BeginPopupModal(newProjectModalID, nullptr)) {
                static std::string projectName;
                static std::string projectPath;
                ImGui::InputText("Name", projectName);

                ImGui::InputText("Parent Folder", projectPath);

                bool incomplete = projectName.empty() || projectPath.empty();
                ImGui::BeginDisabled(incomplete);
                {
                    if(ImGui::Button("Create")) {
                        GetTaskScheduler().schedule(Carrot::TaskDescription {
                            .name = "Load new project",
                            .task = Carrot::Async::AsTask<void>([=]() {
                                deferredLoad();
                                std::filesystem::path projectFolder = projectPath;
                                std::filesystem::path projectFile = projectFolder / Carrot::sprintf("%s.json", projectName.c_str());

                                settings.currentProject = projectFile;
                                settings.addToRecentProjects(projectFile);
                                hasUnsavedChanges = true;

                                wantsToLoadProject = false;
                                GetVFS().addRoot("game", projectFolder);
                                projectToLoad = std::filesystem::path{};
                            })
                        }, Carrot::TaskScheduler::MainLoop);
                    }
                }
                ImGui::EndDisabled();

                ImGui::SameLine();
                if(ImGui::Button("Cancel")) {
                    wantsToLoadProject = false;
                    projectToLoad.clear();
                }

                ImGui::EndPopup();
            }
        }
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
                    currentScene.skybox = skybox;
                    GetEngine().setSkybox(skybox);
                }
            };
            option(Carrot::Skybox::Type::None);
            option(Carrot::Skybox::Type::Forest);

            ImGui::EndCombo();
        }

        ImGui::Separator();

        auto drawSystems = [&](
                const char* title,
                const std::vector<Carrot::ECS::System*>& systems,
                std::function<void(std::unique_ptr<Carrot::ECS::System>&&)> add,
                std::function<void(Carrot::ECS::System*)> remove
            ) {
            Carrot::ECS::System* toRemove = nullptr;

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            if(ImGui::TreeNodeEx(title, nodeFlags, "%s", title)) {
                nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                for(auto* system : systems) {
                    const char* id = system->getName();
                    ImGui::TreeNodeEx(system, nodeFlags, "%s", id);
                    if(ImGui::BeginPopupContextItem(id)) {
                        if(ImGui::MenuItem("Remove##remove system")) {
                            toRemove = system;
                        }
                        ImGui::EndPopup();
                    }
                }

                if(ImGui::BeginMenu(Carrot::sprintf("Add##add system to %s", title).c_str())) {
                    auto& systemLib = Carrot::ECS::getSystemLibrary();
                    for(const auto& systemName : systemLib.getAllIDs()) {
                        if(ImGui::MenuItem(systemName.c_str())) {
                            add(systemLib.create(systemName, currentScene.world));
                        }
                    }

                    ImGui::EndMenu();
                }

                ImGui::TreePop();
            }
            if(toRemove != nullptr) {
                remove(toRemove);
            }
        };
        drawSystems("Render Systems", currentScene.world.getRenderSystems(),
            [&](std::unique_ptr<Carrot::ECS::System>&& ptr) {
                std::string nameToMatch = ptr->getName();
                for(const auto& s : currentScene.world.getRenderSystems()) {
                    if(nameToMatch == s->getName()) {
                        return;
                    }
                }
                currentScene.world.addRenderSystem(std::move(ptr));
            },
            [&](Carrot::ECS::System* ptr) { currentScene.world.removeRenderSystem(ptr); });
        drawSystems("Logic Systems", currentScene.world.getLogicSystems(),
            [&](std::unique_ptr<Carrot::ECS::System>&& ptr) {
                std::string nameToMatch = ptr->getName();
                for(const auto& s : currentScene.world.getLogicSystems()) {
                    if(nameToMatch == s->getName()) {
                        return;
                    }
                }
                currentScene.world.addLogicSystem(std::move(ptr));
            },
            [&](Carrot::ECS::System* ptr) { currentScene.world.removeLogicSystem(ptr); });
    }

    void Application::UIInspector(const Carrot::Render::Context& renderContext) {
        ZoneScoped;
        if(selectedIDs.size() == 1) {
            auto& entityID = selectedIDs[0];
            if(!currentScene.world.exists(entityID)) {
                ImGui::Text("Entity no longer exists!");
                return;
            }
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
        } else if(selectedIDs.size() > 1) {
            ImGui::Text("Inspector for multiple entities is not supported yet.");
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
            if(std::find(selectedIDs.begin(), selectedIDs.end(), entity.getID()) != selectedIDs.end()) {
                nodeFlags |= ImGuiTreeNodeFlags_Selected;
            }
            auto children = currentScene.world.getChildren(entity, Carrot::ShouldRecurse::NoRecursion);

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
                        bool additive = ImGui::GetIO().KeyCtrl;
                        selectEntity(entity.getID(), additive);
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
                    bool additive = ImGui::GetIO().KeyCtrl;
                    selectEntity(entity.getID(), additive);
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
        verify(gameViewport.getRenderGraph() != nullptr, "No render graph for game viewport?");
        gameTextureRef = GetVulkanDriver().getTextureRepository().getRef(gameTexture, renderContext.swapchainIndex);

        float startX = ImGui::GetCursorScreenPos().x;
        float startY = ImGui::GetCursorScreenPos().y;
        ImGui::Image(gameTextureRef->getImguiID(), entireRegion);
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
        if(selectedIDs.size() == 1) {
            auto transformRef = currentScene.world.getComponent<Carrot::ECS::TransformComponent>(selectedIDs[0]);
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

        if(!isPlaying) {
            if(movingGameViewCamera) {
                if(!engine.isGrabbingCursor()) {
                    engine.grabCursor();
                }
            } else if(!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                engine.ungrabCursor();
            } else if(engine.isGrabbingCursor()) {
                movingGameViewCamera = true;
            }
        }

        if(!usingGizmo && ImGui::IsItemClicked()) {
            verify(gameViewport.getRenderGraph() != nullptr, "No render graph for game viewport?");
            const auto gbufferPass = gameViewport.getRenderGraph()->getPassData<Carrot::Render::PassData::GBuffer>("gbuffer").value();
            const auto& entityIDTexture = engine.getVulkanDriver().getTextureRepository().get(gbufferPass.entityID, renderContext.lastSwapchainIndex);
            glm::vec2 uv { ImGui::GetMousePos().x, ImGui::GetMousePos().y };
            uv -= glm::vec2 { startX, startY };
            uv /= glm::vec2 { ImGui::GetWindowWidth(), ImGui::GetWindowHeight() };
            if(uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
                return;
            glm::vec<4, std::uint32_t> sample = entityIDTexture.sampleUVec4(uv.x, uv.y);
            Carrot::UUID uuid { sample[0], sample[1], sample[2], sample[3] };
            if(currentScene.world.exists(uuid)) {
                bool additive = ImGui::GetIO().KeyCtrl;
                selectEntity(uuid, additive);
            }
        }

        bool requireResize = entireRegion.x != gameViewport.getWidth() || entireRegion.y != gameViewport.getHeight();
        if(requireResize) {
            WaitDeviceIdle();
            gameViewport.resize(static_cast<std::uint32_t>(entireRegion.x), static_cast<std::uint32_t>(entireRegion.y));
        }
    }

    void Application::startSimulation() {
        startSimulationRequested = true;
    }

    void Application::performSimulationStart() {
        verify(startSimulationRequested, "Don't call performSimulationStart directly!");
        GetEngine().ungrabCursor();
        isPlaying = true;
        isPaused = false;
        requestedSingleStep = false;
        hasDoneSingleStep = false;
        savedScene = currentScene;

        savedScene.unload();

        currentScene.world.unfreezeLogic();
        currentScene.world.broadcastStartEvent();
        currentScene.load();
        removeEditingSystems();
        GetPhysics().resume();

        startSimulationRequested = false;
    }

    void Application::stopSimulation() {
        stopSimulationRequested = true;
    }

    void Application::performSimulationStop() {
        verify(stopSimulationRequested, "Don't call performSimulationStop directly!");

        isPlaying = false;
        isPaused = false;
        requestedSingleStep = false;
        hasDoneSingleStep = false;
        currentScene.world.broadcastStopEvent();
        savedScene.load();
        currentScene = savedScene;
        savedScene.clear();
        GetPhysics().pause();
        GetEngine().ungrabCursor();
        stopSimulationRequested = false;
    }

    void Application::pauseSimulation() {
        isPaused = true;

        GetPhysics().pause();
        GetEngine().ungrabCursor();
        currentScene.world.freezeLogic();
    }

    void Application::resumeSimulation() {
        currentScene.world.unfreezeLogic();
        GetPhysics().resume();
        isPaused = false;
    }

    void Application::addEditingSystems() {
        currentScene.world.addRenderSystem<Peeler::ECS::CameraRenderer>();
        currentScene.world.addRenderSystem<Peeler::ECS::CollisionShapeRenderer>();
        currentScene.world.addRenderSystem<Peeler::ECS::LightEditorRenderer>();
    }

    void Application::removeEditingSystems() {
        currentScene.world.removeRenderSystem<Peeler::ECS::LightEditorRenderer>();
        currentScene.world.addRenderSystem<Peeler::ECS::LightEditorRenderer>();
        currentScene.world.removeRenderSystem<Peeler::ECS::CollisionShapeRenderer>();
        currentScene.world.removeRenderSystem<Peeler::ECS::CameraRenderer>();
    }

    void Application::UIPlayBar(const Carrot::Render::Context& renderContext) {
        float smallestDimension = std::min(ImGui::GetContentRegionMax().x, ImGui::GetContentRegionMax().y);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0,0,0.1,0.6));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0,0,0.1,0.3));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0.0f, 0.0f));

        // If editing:
        // - Play: Start game
        // - Step: Start the game and steps once
        // - Pause: Don't do anything (disabled)
        // - Stop: Don't do anything (disabled)

        // If playing:
        // - Play: Resumes the game if paused. Changes color depending on state
        // - Step: Pause the game if not paused already, and steps once
        // - Pause: Pause the game. Changes color depending on state
        // - Stop: Stop the simulation and go back to editing.

        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const bool isEditing = !isPlaying;

        ImGui::SameLine((ImGui::GetContentRegionMax().x - smallestDimension * 4.0f + spacing * 3.0f) / 2.0f );
        if(ImGui::ImageButton((isPlaying && !isPaused ? playActiveButtonIcon : playButtonIcon).getImguiID(), ImVec2(smallestDimension, smallestDimension))) {
            if(!isPlaying) {
                startSimulation();
            } else if(isPaused) {
                resumeSimulation();
            }
        }

        ImGui::SameLine();
        if(ImGui::ImageButton(stepButtonIcon.getImguiID(), ImVec2(smallestDimension, smallestDimension))) {
            if(!isPlaying) {
                startSimulation();
            }
            pauseSimulation();
            requestedSingleStep = true;
        }

        ImGui::SameLine();
        ImGui::BeginDisabled(isEditing);
        if(ImGui::ImageButton((isPaused ? pauseActiveButtonIcon : pauseButtonIcon).getImguiID(), ImVec2(smallestDimension, smallestDimension))) {
            if(!isPaused) {
                pauseSimulation();
            }
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::BeginDisabled(isEditing);
        if(ImGui::ImageButton(stopButtonIcon.getImguiID(), ImVec2(smallestDimension, smallestDimension))) {
            stopSimulation();
        }
        ImGui::EndDisabled();

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar(3);
    }

    Application::Application(Carrot::Engine& engine): Carrot::CarrotGame(engine), Tools::ProjectMenuHolder(), settings("peeler"), gameViewport(engine.createViewport()),
        playButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/play_button.png"),
        playActiveButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/play_button_playing.png"),
        pauseButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/pause_button.png"),
        pauseActiveButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/pause_button_paused.png"),
        stepButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/step_button.png"),
        stopButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/stop_button.png"),
        translateIcon(engine.getVulkanDriver(), "resources/textures/ui/translate.png"),
        rotateIcon(engine.getVulkanDriver(), "resources/textures/ui/rotate.png"),
        scaleIcon(engine.getVulkanDriver(), "resources/textures/ui/scale.png"),
        resourcePanel(*this)
    {
        NFD_Init();

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

        addDefaultSystems(currentScene);
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
        gameTexture = resolvePass.getData().postProcessed;

        engine.getVulkanDriver().getTextureRepository().getUsages(gameTexture.rootID) |= vk::ImageUsageFlagBits::eSampled;
        const auto gbufferPass = graphBuilder.getPassData<Carrot::Render::PassData::GBuffer>("gbuffer").value();
        engine.getVulkanDriver().getTextureRepository().getUsages(gbufferPass.entityID.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;

        struct TransitionData {
            Carrot::Render::FrameResource textureToTransition;
        };
        graphBuilder.addPass<TransitionData>("transition for ImGui",
            [&](Carrot::Render::GraphBuilder& graph, Carrot::Render::Pass<TransitionData>& pass, TransitionData& data) {
                data.textureToTransition = gameTexture;
                pass.rasterized = false;
            },
            [this](const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context& renderContext, const TransitionData& data, vk::CommandBuffer& cmds) {
                auto& texture = pass.getGraph().getTexture(data.textureToTransition, renderContext.swapchainIndex);
                texture.assumeLayout(vk::ImageLayout::eColorAttachmentOptimal);
                texture.transitionInline(cmds, vk::ImageLayout::eShaderReadOnlyOptimal);
            }
        );

        gameViewport.setRenderGraph(std::move(graphBuilder.compile()));

        gameViewport.getCamera().setTargetAndPosition(glm::vec3(), glm::vec3(2,-5,5));

        // register components & systems for serialisation
        {
            auto& systems = Carrot::ECS::getSystemLibrary();
            // CollisionShapeRenderer is not serializable
            // LightEditorRenderer is not serializable

            systems.addUniquePtrBased<Game::ECS::CharacterControllerSystem>();

            auto& components = Carrot::ECS::getComponentLibrary();
            components.add<Game::ECS::CharacterControllerComponent>();
            components.add<Game::ECS::PageComponent>();
        }

        settings.load();
        settings.useMostRecent();
        if(settings.currentProject) {
            scheduleLoad(settings.currentProject.value());
        }

        resourcePanel.updateCurrentFolder("game://");
    }

    void Application::tick(double frameTime) {
        if(wantsToLoadProject && projectToLoad != EmptyProject) {
            deferredLoad();
            wantsToLoadProject = false;
            projectToLoad = std::filesystem::path{};
        }
        if(isPlaying && requestedSingleStep) {
            if(!hasDoneSingleStep) {
                resumeSimulation();
                hasDoneSingleStep = true;
            } else {
                pauseSimulation();
                hasDoneSingleStep = false;
                requestedSingleStep = false;
            }
        }
        currentScene.tick(frameTime);

        if(movingGameViewCamera && !isPlaying) {
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
        projectToLoad = fileToOpen;
        wantsToLoadProject = true;
    }

    void Application::deferredLoad() {
        wantsToLoadProject = false;
        GetVFS().removeRoot("game");
        if(projectToLoad == EmptyProject) {
            currentScene.clear();
            scenePath = "game://scenes/main.json";
            addDefaultSystems(currentScene);
            hasUnsavedChanges = false;
            settings.currentProject.reset();
            deselectAllEntities();
            updateWindowTitle();
            currentScene.world.freezeLogic();
            return;
        }
        GetVFS().addRoot("game", std::filesystem::absolute(projectToLoad).parent_path());
        rapidjson::Document description;
        description.Parse(Carrot::IO::readFileAsText(projectToLoad.string()).c_str());

        rapidjson::Document scene;
        {
            scenePath = description["scene"].GetString();
            Carrot::IO::Resource sceneData = scenePath;
            scene.Parse(sceneData.readText());
            currentScene.deserialise(scene);
        }
        addEditingSystems();
        currentScene.world.freezeLogic();
        currentScene.load();
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

        settings.currentProject = projectToLoad;
        settings.addToRecentProjects(projectToLoad);
        hasUnsavedChanges = false;

        selectedIDs.clear();
        if(description.HasMember("selectedIDs")) {
            for(const auto& selectedIDValue : description["selectedIDs"].GetArray()) {
                auto wantedSelection = Carrot::UUID::fromString(selectedIDValue.GetString());
                if(currentScene.world.exists(wantedSelection)) {
                    selectedIDs.push_back(wantedSelection);
                }
            }
        }
        updateWindowTitle();
    }

    static void writeJSON(const std::filesystem::path& targetFile, const rapidjson::Document& toWrite) {
        if(!std::filesystem::exists(targetFile.parent_path())) {
            std::filesystem::create_directories(targetFile.parent_path());
        }
        FILE* fp = fopen(targetFile.string().c_str(), "wb"); // non-Windows use "w"

        char writeBuffer[65536];
        rapidjson::FileWriteStream os(fp, writeBuffer, sizeof(writeBuffer));

        rapidjson::PrettyWriter<rapidjson::FileWriteStream> writer(os);

        toWrite.Accept(writer);
        fclose(fp);
    }

    void Application::saveToFile(std::filesystem::path path) {
        rapidjson::Document document;
        document.SetObject();

        {
            rapidjson::Value scene(rapidjson::kObjectType);
            rapidjson::Document sceneData;
            sceneData.SetObject();

            currentScene.serialise(sceneData);

            if(scenePath.isGeneric()) {
                scenePath = Carrot::IO::VFS::Path("game", scenePath.getPath());
            }

            writeJSON(GetVFS().resolve(scenePath), sceneData);

            document.AddMember("scene", scenePath.toString(), document.GetAllocator());
        }

        if(!selectedIDs.empty()) {
            rapidjson::Value uuidArray{rapidjson::kArrayType};
            for(const auto& id : selectedIDs) {
                uuidArray.PushBack(rapidjson::Value(id.toString().c_str(), document.GetAllocator()), document.GetAllocator());
            }
            document.AddMember("selectedIDs", uuidArray, document.GetAllocator());
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

        writeJSON(path, document);

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

    }

    void Application::onSwapchainImageCountChange(std::size_t newCount) {

    }

    void Application::addEntityMenu(std::optional<Carrot::ECS::Entity> parent) {
        if(ImGui::MenuItem("Add Cube##add entity menu")) {
            auto entity = addEntity(parent);
            entity.addComponent<Carrot::ECS::ModelComponent>();
            Carrot::ECS::ModelComponent& modelComp = entity.getComponent<Carrot::ECS::ModelComponent>();
            modelComp.asyncModel = Carrot::AsyncModelResource(GetRenderer().coloadModel("resources/models/simple_cube.obj"));
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

        selectEntity(entity.getID(), false);

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

        selectEntity(entity.getID(), false);
        markDirty();
    }

    void Application::selectEntity(const Carrot::ECS::EntityID& entity, bool additive) {
        if(additive) {
            selectedIDs.push_back(entity);
        } else {
            selectedIDs.clear();
            selectedIDs.push_back(entity);
        }

        auto* shapeRenderer = currentScene.world.getRenderSystem<ECS::CollisionShapeRenderer>();
        if(shapeRenderer) {
            shapeRenderer->setSelected(selectedIDs);
        }
    }

    void Application::deselectAllEntities() {
        selectedIDs.clear();

        auto* shapeRenderer = currentScene.world.getRenderSystem<ECS::CollisionShapeRenderer>();
        if(shapeRenderer) {
            shapeRenderer->setSelected(selectedIDs);
        }
    }

    void Application::removeEntity(const Carrot::ECS::Entity& entity) {
        auto it = std::find(selectedIDs.begin(), selectedIDs.end(), entity.getID());
        if(it != selectedIDs.end()) {
            selectedIDs.erase(it);
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

    void Application::addDefaultSystems(Carrot::Scene& scene) {
        scene.world.addRenderSystem<Carrot::ECS::SpriteRenderSystem>();
        scene.world.addRenderSystem<Carrot::ECS::ModelRenderSystem>();
        scene.world.addRenderSystem<Carrot::ECS::SystemHandleLights>();
        scene.world.addRenderSystem<Carrot::ECS::LuaRenderSystem>();

        scene.world.addLogicSystem<Carrot::ECS::SystemKinematics>();
        scene.world.addLogicSystem<Carrot::ECS::SystemTransformSwapBuffers>();
        //scene.world.addLogicSystem<Carrot::ECS::SystemSinPosition>();
        scene.world.addLogicSystem<Carrot::ECS::RigidBodySystem>();
        scene.world.addLogicSystem<Carrot::ECS::LuaUpdateSystem>();

        // editing only systems
        addEditingSystems();

        GetRenderer().getLighting().getAmbientLight() = glm::vec3 {0.1,0.1,0.1};
    }

    bool Application::onCloseButtonPressed() {
        if(hasUnsavedChanges) {
            tryToClose = true;
            return false;
        }
        return true;
    }

}