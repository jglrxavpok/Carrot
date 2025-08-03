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
#include <core/utils/UserNotifications.h>
#include <engine/edition/DragDropTypes.h>
#include <core/io/Logging.hpp>
#include <engine/physics/PhysicsSystem.h>

#include <engine/ecs/components/TransformComponent.h>
#include <engine/ecs/components/SpriteComponent.h>
#include <engine/ecs/components/Kinematics.h>
#include <engine/ecs/components/ForceSinPosition.h>
#include <engine/ecs/components/AnimatedModelComponent.h>
#include <engine/ecs/components/LightComponent.h>
#include <engine/ecs/components/RigidBodyComponent.h>

#include <engine/ecs/systems/SpriteRenderSystem.h>
#include <engine/ecs/systems/ModelRenderSystem.h>
#include <engine/ecs/systems/RigidBodySystem.h>
#include <engine/ecs/systems/SystemKinematics.h>
#include <engine/ecs/systems/SystemSinPosition.h>
#include <engine/ecs/systems/SystemHandleLights.h>
#include <engine/ecs/systems/SystemTransformSwapBuffers.h>
#include "ecs/systems/LightEditorRenderer.h"
#include "ecs/systems/CharacterPositionSetterSystem.h"
#include "ecs/systems/CollisionShapeRenderer.h"
#include "ecs/systems/CameraRenderer.h"

#include <core/scripting/csharp/CSProject.h>
#include <core/scripting/csharp/Engine.h>
#include <engine/scripting/CSharpBindings.h>

#include "layers/GizmosLayer.h"
#include <IconsFontAwesome5.h>
#include <commands/DeleteEntitiesCommand.h>
#include <commands/DuplicateEntitiesCommand.h>
#include <commands/ModifySystemsCommands.h>
#include <core/allocators/StackAllocator.h>
#include <engine/ecs/Prefab.h>
#include <engine/ecs/components/ErrorComponent.h>
#include <engine/ecs/components/PrefabInstanceComponent.h>
#include <core/io/FileSystemOS.h>
#include <engine/Engine.h>
#include <engine/edition/Widgets.h>

#include "../../asset_tools/sceneconverter/SceneConverter.h"

namespace fs = std::filesystem;

namespace Peeler {
    Application* Instance = nullptr;

    void Application::setupCamera(const Carrot::Render::Context& renderContext) {
        if(renderContext.pViewport == &gameViewport) {
            cameraController.applyTo(gameViewport.getSizef(), gameViewport.getCamera());

            currentScene.setupCamera(renderContext);

            if(!isPlaying) {
                // override any primary camera the game might have
                cameraController.applyTo(gameViewport.getSizef(), gameViewport.getCamera());
            }
        }
    }

    void Application::onFrame(const Carrot::Render::Context& renderContext) {
        ZoneScoped;

        if (pParticleEditor) {
            bool keepOpen = true;
            if (ImGui::Begin(ICON_FA_MAGIC "  Particle Editor", &keepOpen, ImGuiWindowFlags_MenuBar)) {
                pParticleEditor->onFrame(renderContext);
            }
            ImGui::End();

            if (!keepOpen) {
                // TODO: pParticleEditor->attemptClose();
            }
        }

        if(renderContext.pViewport == &engine.getMainViewport()) {
            ZoneScopedN("Main viewport");
            UIEditor(renderContext);
            Tools::ProjectMenuHolder::onFrame(renderContext);

            // TODO: do it more cleanly with shortcuts for multiple actions
            if(glfwGetKey(GetEngine().getMainWindow().getGLFWPointer(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && glfwGetKey(GetEngine().getMainWindow().getGLFWPointer(), GLFW_KEY_S) == GLFW_PRESS) {
                if(canSave()) {
                    this->triggerSave();
                }
            }
            if(glfwGetKey(GetEngine().getMainWindow().getGLFWPointer(), GLFW_KEY_F2) == GLFW_PRESS) {
                inspectorPanel.focusNameInput();
            }
        }
        if(renderContext.pViewport == &gameViewport) {
            ZoneScopedN("Game viewport");

            if(stopSimulationRequested) {
                performSimulationStop();
            } else if(startSimulationRequested) {
                performSimulationStart();
            }

            if(!isPlaying) {
                // override any primary camera the game might have
                cameraController.applyTo(glm::vec2{ gameViewport.getWidth(), gameViewport.getHeight() }, gameViewport.getCamera());

                float gridSize = 100.0f;
                float cellSize = 1.0f;
                float lineWidth = 0.005f;
                glm::vec4 gridColor = {0.5f, 0.5f, 0.5f, 1.0f};
                gridRenderer.render(renderContext, gridColor, lineWidth, cellSize, gridSize);

                gridColor = {0.1f, 0.1f, 0.9f, 1.0f};
                gridRenderer.render(renderContext, gridColor, 2.0f*lineWidth, gridSize/2.0f, gridSize);
            }
        }
    }

    void Application::UIEditor(const Carrot::Render::Context& renderContext) {
        ZoneScoped;
        if (projectLoadStateMachine && !loadStateMachineReady.isIdle()) {
            projectLoadStateMachine->switchTo();
        }
        if(tryToClose) {
            openUnsavedChangesPopup([&]() {
                requestShutdown();
            }, [&]() {
                tryToClose = false;
            });
            tryToClose = false;
        }

        shortcuts = {}; // reset shortcut state
        handleShortcuts(renderContext);

        if (shortcuts.undoRequested) {
            undoStack.undo();
        }
        if (shortcuts.redoRequested) {
            undoStack.redo();
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
            if(ImGui::BeginMenu(ICON_FA_FOLDER_OPEN "  Project")) {
                drawProjectMenu();
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu(ICON_FA_STREAM "  Scenes")) {
                drawScenesMenu();
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu(ICON_FA_TOOLS "  Settings")) {
                drawSettingsMenu();
                ImGui::EndMenu();
            }

            if(ImGui::BeginMenu(ICON_FA_HAMMER "  Tools")) {
                drawToolsMenu();
                ImGui::EndMenu();
            }

            static bool showDemo = false;

            if(ImGui::BeginMenu(ICON_FA_VIAL "  Tests")) {
                bool hasPhysicsDebug = GetPhysics().getDebugViewport() != nullptr;
                if(ImGui::MenuItem("Activate Physics debug", nullptr, hasPhysicsDebug)) {
                    if(!hasPhysicsDebug) {
                        GetPhysics().setViewport(&gameViewport);
                    } else {
                        GetPhysics().setViewport(nullptr);
                    }
                }

                if(ImGui::MenuItem("Show ImGui demo", nullptr, showDemo)) {
                    showDemo = !showDemo;
                }

                ImGui::EndMenu();
            }

            const bool csharpEnabled = getCurrentProjectFile() != EmptyProject; // must already have a project
            if(ImGui::BeginMenu(ICON_FA_CODE "  C#", csharpEnabled)) {
                if(ImGui::MenuItem("Generate C# .sln")) {
                    // TODO: check that solution does not exist yet

                    const std::string& projectName = getCurrentProjectName();
                    const Carrot::UUID projectUuid; // generate new UUID
                    const std::string projectGuidString = projectUuid.toString();

                    const std::filesystem::path projectBasePath = getCurrentProjectFile().parent_path() / "code";
                    const std::filesystem::path carrotDllFilepath = GetVFS().resolve(GetCSharpBindings().getEngineDllPath());
                    std::error_code ec; // ignored
                    std::filesystem::path relativeCarrotDllFilepath = std::filesystem::relative(carrotDllFilepath, projectBasePath, ec);
                    if(ec || relativeCarrotDllFilepath.empty()) {
                        relativeCarrotDllFilepath = carrotDllFilepath;
                    }

                    const std::string carrotDll = Carrot::toString(relativeCarrotDllFilepath.u8string());

                    // copy file and replace some tokens by their value
                    auto templateCopy = [&](const Carrot::IO::VFS::Path& path, const Carrot::IO::VFS::Path& destinationPath) {
                        const Carrot::IO::Resource r { path };
                        std::string templateContents = r.readText();

                        const std::array<std::pair<std::string, std::string>, 3> replacements = {
                                std::pair<std::string, std::string> {"%PROJECT-NAME%", projectName},
                                                                    {"%GUID%", projectGuidString},
                                                                    {"%CARROT-DLL%", carrotDll },
                        };

                        std::string finalContents = templateContents;
                        for(const auto& [k, v] : replacements) {
                            finalContents = Carrot::replace(finalContents, k, v);
                        }

                        const std::filesystem::path destinationFile = GetVFS().resolve(destinationPath);

                        const std::filesystem::path parentPath = destinationFile.parent_path();
                        if(!std::filesystem::exists(parentPath)) {
                            std::filesystem::create_directories(parentPath);
                        }

                        Carrot::IO::writeFile(Carrot::toString(destinationFile.u8string()), (void*)finalContents.c_str(), finalContents.size());
                    };

                    templateCopy("editor://resources/text/csharp/CSProjTemplate.csproj",
                                 Carrot::IO::VFS::Path { Carrot::sprintf("game://code/%s.csproj", projectName.c_str()) });
                    templateCopy("editor://resources/text/csharp/SolutionTemplate.sln",
                                 Carrot::IO::VFS::Path { Carrot::sprintf("game://code/%s.sln", projectName.c_str()) });
                    templateCopy("editor://resources/text/csharp/Properties/AssemblyInfo.cs", "game://code/Properties/AssemblyInfo.cs");
                    templateCopy("editor://resources/text/csharp/ExampleSystem.cs", "game://code/ExampleSystem.cs");
                }

                // TODO: should be done automatically
                if(ImGui::MenuItem("Build C#")) {
                    const std::string& projectName = getCurrentProjectName();
                    Carrot::IO::VFS::Path csproj { Carrot::sprintf("game://code/%s.csproj", projectName.c_str()) };
                    buildCSProject(csproj);
                }

                if(ImGui::MenuItem("Reload C#")) {
                    reloadGameAssembly();
                }

                ImGui::Separator();
                if(ImGui::MenuItem("Open .sln")) {
                    const std::string& projectName = getCurrentProjectName();
                    Carrot::IO::VFS::Path slnFile { Carrot::sprintf("game://code/%s.sln", projectName.c_str()) };
                    std::optional<std::filesystem::path> path = GetVFS().resolve(slnFile);
                    if(path.has_value()) {
                        if(!Carrot::IO::openFileInDefaultEditor(path.value())) {
                            // TODO: user error notification
                            Carrot::Log::error("Failed to open %s", Carrot::toString(path->u8string()).c_str());
                        }
                    } else {
                        // TODO: user error notification
                        Carrot::Log::error("No solution file found at %s", slnFile.toString().c_str());
                    }
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();

            drawCameraSettingsWindow();
            drawPhysicsSettingsWindow();
            drawNewSceneWindow();

            if(showDemo) {
                ImGui::ShowDemoWindow();
            }
        }

        //checkErrors();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if(ImGui::Begin("Game view", nullptr, ImGuiWindowFlags_NoBackground)) {
            UIGameView(renderContext);
            gameViewportFocused = ImGui::IsWindowFocused();
        } else {
            gameViewportFocused = false;
        }
        ImGui::End();
        ImGui::PopStyleVar(3);

        ImGui::End();

        if(ImGui::Begin(ICON_FA_STREAM "  Scene")) {
            UIWorldHierarchy(renderContext);
        }
        ImGui::End();

        if(ImGui::Begin(ICON_FA_FILE_ALT "  Resources")) {
            resourcePanel.draw(renderContext);
        }
        ImGui::End();

        if(ImGui::Begin(ICON_FA_ROUTE "  NavMesh utils")) {
            navMeshPanel.draw(renderContext);
        }
        ImGui::End();

        if(ImGui::Begin(InspectorPanel::ImGuiWindowID)) {
            inspectorPanel.draw(renderContext);
        }
        ImGui::End();

        if(ImGui::Begin("PlayBar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
            UIPlayBar(renderContext);
        }
        ImGui::End();

        if(ImGui::Begin(ICON_FA_GLOBE "  Scene properties")) {
            UISceneProperties(renderContext);
        }
        ImGui::End();

        undoStack.debugDraw();

        UIStatusBar(renderContext);

        // all UI has had an opportunity to use this
        if(entityIDPickedThisFrame != Carrot::UUID::null()) {
            bool additive = ImGui::GetIO().KeyCtrl;
            selectEntity(entityIDPickedThisFrame, additive);
        }

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
                        if (deferredLoad()) {
                            std::filesystem::path projectFolder = projectPath;
                            projectFolder /= projectName;
                            std::filesystem::path projectFile = projectFolder / Carrot::sprintf("%s.json", projectName.c_str());

                            settings.currentProject = projectFile;
                            settings.addToRecentProjects(projectFile);
                            hasUnsavedChanges = true;

                            wantsToLoadProject = false;
                            GetVFS().addRoot("game", projectFolder);
                            projectToLoad = std::filesystem::path{};
                        }
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
        if(ImGui::CollapsingHeader(ICON_FA_LIGHTBULB "  Lighting")) {
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
            option(Carrot::Skybox::Type::Meadow);
            option(Carrot::Skybox::Type::Clouds);

            ImGui::EndCombo();
        }

        ImGui::Separator();

        auto drawSystems = [&](
                const char* title,
                const std::vector<Carrot::ECS::System*>& systems,
                std::function<void(const std::string&)> add,
                std::function<void(Carrot::ECS::System*)> remove
            ) {
            Carrot::ECS::System* toRemove = nullptr;

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            if(ImGui::TreeNodeEx(title, nodeFlags, "%s", title)) {
                nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                for(auto* system : systems) {
                    std::string id = system->getName();
                    if(!system->shouldBeSerialized()) {
                        id += " (Editor)";
                    }
                    ImGui::TreeNodeEx(system, nodeFlags, "%s", id.c_str());
                    if(ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        for(const auto& e : system->getEntities()) {
                            ImGui::BulletText("%s", e.getName().data());
                        }
                        ImGui::EndTooltip();
                    }
                    if(ImGui::BeginPopupContextItem(id.c_str())) {
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
                            add(systemName);
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
            [&](const std::string& name) {
                if (!currentScene.world.getRenderSystem(name)) {
                    undoStack.push<AddSystemsCommand>(Carrot::Vector{name}, true);
                }
            },
            [&](Carrot::ECS::System* ptr) {
                undoStack.push<RemoveSystemCommand>(ptr->getName(), true);
            });
        drawSystems("Logic Systems", currentScene.world.getLogicSystems(),
            [&](const std::string& name) {
                if (!currentScene.world.getLogicSystem(name)) {
                    undoStack.push<AddSystemsCommand>(Carrot::Vector{name}, false);
                }
            },
            [&](Carrot::ECS::System* ptr) {
                undoStack.push<RemoveSystemCommand>(ptr->getName(), false);
            });
    }

    void Application::UIStatusBar(const Carrot::Render::Context& renderContext) {
        static bool showNotificationList = false;
        // https://github.com/ocornut/imgui/issues/3518#issuecomment-918186716
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        const float height = ImGui::GetTextLineHeightWithSpacing();
        if(ImGui::BeginViewportSideBar("##Status bar", nullptr, ImGuiDir_Down, height, windowFlags)) {
            if (ImGui::BeginMenuBar()) {
                static std::vector<Carrot::NotificationState> notifications{};
                notifications.clear(); // clear without removing memory to reuse allocations next frames
                Carrot::UserNotifications::getInstance().getNotifications(notifications);
                const std::string notificationButtonText = Carrot::sprintf("Background tasks (%llu)", notifications.size());
                const char* notificationsPopupID = "Background tasks";
                if(ImGui::MenuItem(notificationButtonText.c_str())) {
                    showNotificationList = !showNotificationList;
                }

                if(showNotificationList) {
                    if(ImGui::Begin(notificationsPopupID)) {
                        const double nowInSeconds = std::chrono::duration<float>(std::chrono::steady_clock::now().time_since_epoch()).count();
                        for(const auto& notif : notifications) {
                            ImGui::TextUnformatted(notif.title.c_str());
                            ImGui::TextUnformatted(notif.body.c_str());
                            // TODO: handle case if progress < 0
                            ImGui::ProgressBar(notif.progress);

                            const std::int32_t timeElapsedInSeconds = static_cast<std::int32_t>(nowInSeconds - notif.startTime);
                            ImGui::Text("Time elapsed: %d s", timeElapsedInSeconds);
                            ImGui::Separator();
                        }
                    }
                    ImGui::End();
                }
                ImGui::EndMenuBar();
            }
        }
        ImGui::End();
    }

    static ImColor getEntityNameColor(Carrot::ECS::Entity& entity) {
        const ImColor prefabColor { 0.0f, 0.8f, 1.0f, 1.0f };
        const ImColor errorColor { 0.8f, 0.2f, 0.0f, 1.0f };
        if(entity.getComponent<Carrot::ECS::PrefabInstanceComponent>().hasValue()) {
            return prefabColor;
        }
        if(entity.getComponent<Carrot::ECS::ErrorComponent>().hasValue()) {
            return errorColor;
        }
        return ImGui::GetStyle().Colors[ImGuiCol_Text];
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

        ImGuiTableFlags flags = 0;
        ImGui::BeginTable("EntityList##UIWorldHierarchy", 4, flags);
        ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Errors", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Warnings", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("Visibility", ImGuiTableColumnFlags_WidthFixed);

        const bool canModifyHierarchyViaShorcuts = ImGui::IsWindowFocused() || gameViewportFocused;
        bool requestRemoveEntities = canModifyHierarchyViaShorcuts && shortcuts.deleteRequested;
        bool requestDuplicateEntities = canModifyHierarchyViaShorcuts && shortcuts.duplicateRequested;

        std::function<void(Carrot::ECS::Entity&, bool)> showEntityTree = [&] (Carrot::ECS::Entity& entity, bool recursivelySelect) {
            if(!entity)
                return;
            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            if(selectedEntityIDs.contains(entity.getID())) {
                nodeFlags |= ImGuiTreeNodeFlags_Selected;
            }
            auto children = currentScene.world.getChildren(entity, Carrot::ShouldRecurse::NoRecursion);

            if(recursivelySelect) {
                selectEntity(entity.getID(), true);
            }

            auto addChildMenu = [&]() {
                std::string id = "##add child to entity ";
                id += std::to_string(entity.getID().hash());
                if(ImGui::BeginPopupContextItem(id.c_str())) {
                    bool isSingleEntity = selectedEntityIDs.size() <= 1;
                    if (!selectedEntityIDs.contains(entity.getID())) {
                        selectEntity(entity.getID(), false);
                        isSingleEntity = true;
                    }
                    if (isSingleEntity) {
                        if(ImGui::BeginMenu("Add child##add child to entity world hierarchy")) {
                            addEntityMenu(entity);
                            ImGui::EndMenu();
                        }
                    }

                    if(ImGui::MenuItem("Duplicate##duplicate entity world hierarchy", "CTRL + D" /*could BoolInputAction provide the shortcut text?*/)) {
                        requestDuplicateEntities = true;
                    }
                    if(ImGui::MenuItem("Remove##remove entity world hierarchy", "Delete")) {
                        requestRemoveEntities = true;
                    }
                    ImGui::Separator();

                    if (isSingleEntity) {
                        if(ImGui::MenuItem("Convert to prefab##convert to prefab entity world hierarchy")) {
                            convertEntityToPrefab(entity);
                        }
                        ImGui::Separator();
                    }

                    if(ImGui::MenuItem("Apply transform to colliders")) {
                        for(const auto& selectedID : selectedEntityIDs) {
                            Carrot::ECS::Entity selectedEntity = currentScene.world.wrap(selectedID);
                            if(selectedEntity) {
                                auto transformRef = selectedEntity.getComponent<Carrot::ECS::TransformComponent>();
                                auto componentRef = selectedEntity.getComponent<Carrot::ECS::RigidBodyComponent>();
                                if(transformRef.hasValue() && componentRef.hasValue()) {
                                    for(auto& pCollider : componentRef->rigidbody.getColliders()) {
                                        pCollider->getShape().changeShapeScale(transformRef->computeFinalScale());
                                    }
                                }
                            }
                        }
                    }

                    ImGui::EndPopup();
                }
            };

            auto dragAndDrop = [&]() {
                bool dragging = false;
                // TODO: makes drag and drop into editor complicated
                if (selectedEntityIDs.contains(entity.getID())) {
                    dragging = ImGui::BeginDragDropSource();
                    if(dragging) {
                        Carrot::Vector<Carrot::ECS::EntityID> draggedEntities;
                        draggedEntities.ensureReserve(selectedEntityIDs.size());
                        for (const auto& selectedEntity : selectedEntityIDs) {
                            draggedEntities.emplaceBack() = selectedEntity;
                            ImGui::Text("%s", currentScene.world.getName(selectedEntity).c_str());
                        }
                        ImGui::SetDragDropPayload(Carrot::Edition::DragDropTypes::EntityUUIDs, draggedEntities.cdata(), draggedEntities.bytes_size());
                        ImGui::EndDragDropSource();
                    }
                }

                if(ImGui::BeginDragDropTarget()) {
                    if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::EntityUUIDs)) {
                        verify(payload->DataSize % sizeof(Carrot::UUID) == 0, "Invalid payload for EntityUUIDs");
                        static_assert(sizeof(Carrot::ECS::EntityID) == sizeof(Carrot::UUID), "Assumes EntityID = UUID");
                        const u32 entityCount = payload->DataSize / sizeof(Carrot::UUID);
                        std::uint32_t* data = reinterpret_cast<std::uint32_t*>(payload->Data);

                        for (u32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
                            Carrot::UUID uuid{data[0], data[1], data[2], data[3]};

                            if(currentScene.world.exists(uuid)) {
                                changeEntityParent(uuid, entity/*new parent*/);
                            }

                            data += 4; // go to next UUID
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                return dragging;
            };

            auto drawErrorsColumn = [&]() {
                ImGui::TableSetColumnIndex(1);

                std::string id = "##errors";
                id += std::to_string(entity.getID().hash());

                drawEntityErrors(errorReport, entity, id.c_str());
            };

            auto drawWarningsColumn = [&]() {
                ImGui::TableSetColumnIndex(2);

                std::string id = "##warnings";
                id += std::to_string(entity.getID().hash());

                drawEntityWarnings(entity, id.c_str());
            };

            auto drawSettingsColumn = [&]() {
                ImGui::TableSetColumnIndex(3);

                std::string id = "##change entity visibility";
                id += std::to_string(entity.getID().hash());
                bool visible = (entity.getFlags() & Carrot::ECS::EntityFlags::Hidden) == 0;

                const char* icon = visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
                if(ImGui::SmallButton(Carrot::sprintf("%s##%s", icon, id.c_str()).c_str())) {
                    if(visible) {
                        entity.setFlags(Carrot::ECS::EntityFlags::Hidden);
                    } else {
                        entity.removeFlags(Carrot::ECS::EntityFlags::Hidden);
                    }
                }
            };

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);

            auto handleSelection = [&](bool isDragging) {
                // behavior based on behavior of file explorer on Linux Mint (probably close to Windows Explorer too)

                const bool isControlHeld = ImGui::GetIO().KeyCtrl;
                const bool isAltHeld = ImGui::GetIO().KeyAlt;

                if (isDragging) {
                    return;
                }

                if(!ImGui::IsItemHovered()) {
                    return;
                }

                if (isAltHeld && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
                    recursivelySelect = true;
                    return;
                }

                if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                    if (!isControlHeld) {
                        selectEntity(entity.getID(), false); // only select this entity, clear previous selection
                    } else {
                        if (selectedEntityIDs.contains(entity.getID())) {
                            selectedEntityIDs.erase(entity.getID());
                        } else {
                            selectEntity(entity.getID(), true);
                        }
                    }
                }
            };
            if(!children.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, getEntityNameColor(entity).Value);
                bool showChildren = ImGui::TreeNodeEx((void*)entity.getID().hash(), nodeFlags, "%s", currentScene.world.getName(entity).c_str());
                ImGui::PopStyleColor();

                const bool isDragging = dragAndDrop();
                handleSelection(isDragging);

                addChildMenu();
                drawErrorsColumn();
                drawWarningsColumn();
                drawSettingsColumn();

                if(showChildren) {
                    for(auto& c : children) {
                        showEntityTree(c, recursivelySelect);
                    }
                    ImGui::TreePop();
                }
            } else { // has no children
                nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                ImGui::PushStyleColor(ImGuiCol_Text, getEntityNameColor(entity).Value);
                ImGui::TreeNodeEx((void*)entity.getID().hash(), nodeFlags, "%s", currentScene.world.getName(entity).c_str());
                ImGui::PopStyleColor();
                const bool isDragging = dragAndDrop();
                handleSelection(isDragging);

                addChildMenu();

                drawErrorsColumn();
                drawWarningsColumn();
                drawSettingsColumn();
            }
        };
        for(auto& entityObj : currentScene.world.getAllEntities()) {
            if( ! currentScene.world.getParent(entityObj)) {
                showEntityTree(entityObj, false);
            }
        }
        ImGui::EndTable();

        if(auto* payload = ImGui::GetDragDropPayload()) {
            if(payload->IsDataType(Carrot::Edition::DragDropTypes::EntityUUIDs)) {
                ImGui::TextDisabled("Make it a root entity");

                if(ImGui::BeginDragDropTarget()) {
                    if(auto* payload = ImGui::AcceptDragDropPayload(Carrot::Edition::DragDropTypes::EntityUUIDs)) {
                        verify(payload->DataSize % sizeof(Carrot::UUID) == 0, "Invalid payload for EntityUUIDs");
                        static_assert(sizeof(Carrot::ECS::EntityID) == sizeof(Carrot::UUID), "Assumes EntityID = UUID");
                        const u32 entityCount = payload->DataSize / sizeof(Carrot::UUID);
                        std::uint32_t* data = reinterpret_cast<std::uint32_t*>(payload->Data);

                        for (u32 entityIndex = 0; entityIndex < entityCount; entityIndex++) {
                            Carrot::UUID uuid{data[0], data[1], data[2], data[3]};

                            if(currentScene.world.exists(uuid)) {
                                changeEntityParent(uuid, std::optional<Carrot::ECS::Entity>{} /*no parent*/);
                            }

                            data += 4; // next entity
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
        }

        if (requestRemoveEntities) {
            removeSelectedEntities();
        }
        if (requestDuplicateEntities) {
            duplicateSelectedEntities();
        }
    }

    void Application::UIGameView(const Carrot::Render::Context& renderContext) {
        ZoneScoped;
        ImGuizmo::BeginFrame();
        
        ImVec2 entireRegion = ImGui::GetContentRegionAvail();
        verify(gameViewport.getRenderGraph() != nullptr, "No render graph for game viewport?");
        gameTextureRef = GetVulkanDriver().getResourceRepository().getTextureRef(gameTexture, renderContext.swapchainIndex);

        float startX = ImGui::GetCursorScreenPos().x;
        float startY = ImGui::GetCursorScreenPos().y;

        std::int32_t windowX, windowY;
        engine.getMainWindow().getPosition(windowX, windowY);
        glm::vec2 offset = glm::mod(glm::vec2{ startX - windowX, startY - windowY }, glm::vec2 { ImGui::GetMainViewport()->Size.x, ImGui::GetMainViewport()->Size.y });
        gameViewport.setOffset(offset);

        ImGui::Image(gameTextureRef->getImguiID(), entireRegion);

        movingGameViewCamera = ImGui::IsItemClicked(ImGuiMouseButton_Right);

        bool canPickEntity = true;
        for (int j = sceneViewLayersStack.size()-1; j >= 0; j--) {
            auto* pLayer = sceneViewLayersStack[j].get();
            pLayer->draw(renderContext, startX, startY);

            canPickEntity &= pLayer->allowSceneEntityPicking();
            movingGameViewCamera &= pLayer->allowCameraMovement();

            if(!pLayer->showLayersBelow()) {
                break;
            }
        }
        flushLayers();

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

        entityIDPickedThisFrame = Carrot::UUID::null();
        if(canPickEntity && ImGui::IsItemClicked()) {
            verify(gameViewport.getRenderGraph() != nullptr, "No render graph for game viewport?");
            const auto gbufferPass = gameViewport.getRenderGraph()->getPassData<Carrot::Render::PassData::GBuffer>("gbuffer").value();
            const auto& entityIDTexture = GetVulkanDriver().getResourceRepository().getTexture(gbufferPass.entityID, renderContext.lastSwapchainIndex);
            glm::vec2 uv { ImGui::GetMousePos().x, ImGui::GetMousePos().y };
            uv -= glm::vec2 { startX, startY };
            uv /= glm::vec2 { ImGui::GetWindowWidth(), ImGui::GetWindowHeight() };
            if(uv.x < 0 || uv.x > 1 || uv.y < 0 || uv.y > 1)
                return;
            glm::vec<4, std::uint32_t> sample = entityIDTexture.sampleUVec4(uv.x, uv.y);
            Carrot::UUID uuid { sample[0], sample[1], sample[2], sample[3] };
            if(currentScene.world.exists(uuid)) {
                entityIDPickedThisFrame = uuid; // deferred because the inspector can have a widget to select an entity running
            }
        }

        bool requireResize = entireRegion.x != gameViewport.getWidth() || entireRegion.y != gameViewport.getHeight();
        if(requireResize && !ImGui::IsMouseDown(ImGuiMouseButton_Left) /*avoid too many resize events at once*/) {
            WaitDeviceIdle();
            GetRenderer().waitForRenderToComplete();
            gameViewport.resize(static_cast<std::uint32_t>(entireRegion.x), static_cast<std::uint32_t>(entireRegion.y));
        }
    }

    void Application::drawScenesMenu() {
        if(ImGui::MenuItem(ICON_FA_FILE "  New")) {
            showNewScenePopup = true;
        }

        if(ImGui::MenuItem(ICON_FA_FOLDER_OPEN "  Open")) {
            // TODO
        }

        if(ImGui::BeginMenu(ICON_FA_FOLDER_OPEN "  Open recent")) {
            for(const auto& known : knownScenes) {
                const std::string label = known.toString();
                const bool selected = known == scenePath;
                if(ImGui::MenuItem(label.c_str(), nullptr, selected) && !selected) {
                    openScene(known);
                }
            }
            ImGui::EndMenu();
        }

        if(ImGui::MenuItem(ICON_FA_SAVE "  Save")) {
            saveCurrentScene();
        }

        if(ImGui::MenuItem(ICON_FA_SAVE "  Save as")) {
            // TODO
        }
    }

    void Application::drawSettingsMenu() {
        if(ImGui::MenuItem(ICON_FA_CUBES "  Physics settings", nullptr, &showPhysicsSettings /* TODO: save to editor settings */)) {
            //showPhysicsSettings = !showPhysicsSettings;
        }
        ImGui::MenuItem(ICON_FA_CAMERA "  Editor camera settings", nullptr, &showCameraSettings /* TODO: save to editor settings */);
    }

    void Application::drawToolsMenu() {
        if (ImGui::MenuItem(ICON_FA_MAGIC "  Particle editor", nullptr, &showParticleEditor)) {
            if (!pParticleEditor) {
                pParticleEditor = Carrot::makeUnique<Peeler::ParticleEditor>(Carrot::Allocator::getDefault(), engine);
            }
        }
    }

    void Application::drawNewSceneWindow() {
        if(!showNewScenePopup) {
            return;
        }

        // TODO: unsaved changes window

        const char* popupID = "New Scene##popup title";
        ImGui::OpenPopup(popupID);
        bool isOpen = true;
        if(ImGui::BeginPopupModal(popupID, &isOpen)) {
            static std::string sceneName = "New scene";

            bool finished = false;
            finished |= ImGui::InputText("Name", sceneName, ImGuiInputTextFlags_EnterReturnsTrue);
            finished |= ImGui::Button(ICON_FA_PLUS "  Create");

            ImGui::SameLine();
            if(ImGui::Button(ICON_FA_BAN "  Cancel")) {
                showNewScenePopup = false;
                ImGui::CloseCurrentPopup();
            }

            if(finished && !sceneName.empty()) {
                newScene(Carrot::IO::VFS::Path { "game://scenes/" + sceneName + ".json" });
                showNewScenePopup = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void Application::drawCameraSettingsWindow() {
        if (showCameraSettings) {
            if (ImGui::Begin("Camera settings", &showCameraSettings)) {
                ImGui::SliderFloat("Speed", &cameraSpeedMultiplier, 0.01f, 1000.0f);
            }
            ImGui::End();
        }
    }

    void Application::drawPhysicsSettingsWindow() {
        if(showPhysicsSettings) {
            if(ImGui::Begin("Physics settings", &showPhysicsSettings)) {

                // draw collision matrix (ie which layers can collide with which other layers)
                auto& layersManager = GetPhysics().getCollisionLayers();
                const auto collisionLayers = layersManager.getLayers();
                const std::size_t layerCount = collisionLayers.size();

                // the first column will be the other layer name
                if(ImGui::BeginTable("Collision Matrix", layerCount+1)) {
                    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_DefaultHide);
                    for (std::size_t i = 0; i < layerCount; ++i) {
                        ImGui::TableSetupColumn(collisionLayers[i].name.c_str());
                    }

                    ImGui::TableHeadersRow();
                    for (std::size_t i = 0; i < layerCount; ++i) {
                        ImGui::PushID(i);
                        CLEANUP(ImGui::PopID());
                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(collisionLayers[i].name.c_str());

                        ImGui::SameLine();
                        if(ImGui::Button("X")) {
                            // TODO: remove
                        }

                        for (std::size_t j = 0; j < layerCount; ++j) {
                            ImGui::PushID(j);
                            CLEANUP(ImGui::PopID());

                            ImGui::TableNextColumn();
                            bool shouldCollide = layersManager.canCollide(collisionLayers[i].layerID, collisionLayers[j].layerID);

                            std::string id = Carrot::sprintf("##collision_%d_%d", i, j);
                            if(ImGui::Checkbox("##collision", &shouldCollide)) {
                                layersManager.setCanCollide(collisionLayers[i].layerID, collisionLayers[j].layerID, shouldCollide);
                            }
                        }
                    }
                    ImGui::EndTable();

                    bool justOpenedPopup = false;
                    static const char* newLayerModalID = "New collision layer";
                    if(ImGui::Button(newLayerModalID)) {
                        ImGui::OpenPopup(newLayerModalID);
                        justOpenedPopup = true;
                    }

                    bool open = true;
                    if(ImGui::BeginPopupModal(newLayerModalID, &open)) {
                        static std::string layerName = "New layer";
                        static bool layerIsStatic = false;

                        if(justOpenedPopup) {
                            layerName = "New layer";
                        }

                        bool confirm = false;
                        confirm |= ImGui::InputText("Layer name", layerName, ImGuiInputTextFlags_EnterReturnsTrue /* press enter to directly create layer without mouse */);

                        if(justOpenedPopup) {
                            ImGui::SetKeyboardFocusHere();
                        }

                        ImGui::Checkbox("Is static", &layerIsStatic);

                        confirm |= ImGui::Button(ICON_FA_PLUS "  Create");
                        ImGui::SameLine();
                        if(ImGui::Button(ICON_FA_BAN "  Cancel")) {
                            ImGui::CloseCurrentPopup();
                        }

                        if(confirm) {
                            layersManager.newLayer(layerName, layerIsStatic);
                            ImGui::CloseCurrentPopup();
                        }

                        ImGui::EndPopup();
                    }
                }

            }
            ImGui::End();
        }
    }

    void Application::buildCSProject(const Carrot::IO::VFS::Path& csproj) {
        if(!GetVFS().exists(csproj)) {
            Carrot::Log::error("C# project %s does not exist", csproj.toString().c_str());
            return;
        }

        Carrot::Scripting::CSProject project { csproj };

        // TODO: move to another thread?
        std::vector<fs::path> sourceFiles;
        sourceFiles.reserve(project.getSourceFiles().size());

        const Carrot::IO::VFS::Path root = "game://code";
        for(const auto& relativePath : project.getSourceFiles()) {
            sourceFiles.emplace_back(GetVFS().resolve(root / Carrot::toString(relativePath.u8string())));
        }

        std::vector<fs::path> references = {
                GetVFS().resolve("engine://scripting/Carrot.dll")
        };

        std::string dllName = getCurrentProjectName() + ".dll";
        const fs::path assemblyOutput = GetVFS().resolve(root / std::string_view{"bin"} / dllName);
        fs::path pdbOutput = assemblyOutput;
        pdbOutput.replace_extension(".pdb");

        if(!fs::exists(pdbOutput.parent_path())) {
            fs::create_directories(pdbOutput.parent_path());
        }

        // TODO: redirect output and display in console?
        bool r = GetCSharpScripting().compileFiles(assemblyOutput, sourceFiles, references, pdbOutput);
        if(r) {
            Carrot::Log::info("Successfully compiled %s", dllName.c_str());

            reloadGameAssembly();
        } else {
            Carrot::Log::error("Failed to compile %s, check output", dllName.c_str());
        }
    }

    void Application::reloadGameAssembly() {
        const Carrot::IO::VFS::Path root = "game://code";
        std::string dllName = getCurrentProjectName() + ".dll";

        const Carrot::IO::VFS::Path gameDll = root / std::string_view{"bin"} / dllName;

        if(!GetVFS().exists(gameDll)) {
            GetCSharpBindings().unloadGameAssembly();
        } else {
            GetCSharpBindings().loadGameAssembly(gameDll);

            inspectorPanel.registerCSharpEdition();
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
        savedScene.copyFrom(currentScene);

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

    bool Application::isCurrentlyPlaying() const {
        return isPlaying;
    }

    bool Application::isCurrentlyPaused() const {
        return isCurrentlyPlaying() && isPaused;
    }

    void Application::performSimulationStop() {
        verify(stopSimulationRequested, "Don't call performSimulationStop directly!");

        isPlaying = false;
        isPaused = false;
        requestedSingleStep = false;
        hasDoneSingleStep = false;
        currentScene.world.broadcastStopEvent();
        savedScene.load();
        currentScene.copyFrom(savedScene);
        savedScene.clear();
        GetPhysics().pause();
        GetEngine().ungrabCursor();
        GetAudioManager().stopAllSounds();
        stopSimulationRequested = false;
    }

    void Application::popLayer() {
        removeLayer(sceneViewLayersStack.back().get());
    }

    void Application::removeLayer(ISceneViewLayer* layer) {
        std::size_t j = 0;
        for (; j < sceneViewLayersStack.size(); ++j) {
            auto& ptr = sceneViewLayersStack[j];
            if(ptr.get() == layer) {
                toDeleteLayers.emplace_back(j);
                break;
            }
        }
    }

    void Application::flushLayers() {
        std::size_t removedCount = 0;
        for (int j = 0; j < toDeleteLayers.size(); ++j) {
            auto index = toDeleteLayers[j];
            index -= removedCount;
            if(index < sceneViewLayersStack.size()) {
                sceneViewLayersStack.erase(sceneViewLayersStack.begin() + index);
                removedCount++;
            }
        }
        toDeleteLayers.clear();
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
        currentScene.world.addRenderSystem<Peeler::ECS::CharacterPositionSetterSystem>();
        currentScene.world.addRenderSystem<Peeler::ECS::LightEditorRenderer>();
    }

    void Application::removeEditingSystems() {
        currentScene.world.removeRenderSystem<Peeler::ECS::LightEditorRenderer>();
        currentScene.world.addRenderSystem<Peeler::ECS::LightEditorRenderer>();
        currentScene.world.removeRenderSystem<Peeler::ECS::CollisionShapeRenderer>();
        currentScene.world.removeRenderSystem<Peeler::ECS::CharacterPositionSetterSystem>();
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

    Application::Application(Carrot::Engine& engine): Carrot::CarrotGame(engine), Tools::ProjectMenuHolder(),
        currentScene(engine.getSceneManager().getMainScene()),
        settings("peeler"),
        gameViewport(engine.createViewport(GetEngine().getMainWindow())),
        playButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/play_button.png"),
        playActiveButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/play_button_playing.png"),
        pauseButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/pause_button.png"),
        pauseActiveButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/pause_button_paused.png"),
        stepButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/step_button.png"),
        stopButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/stop_button.png"),
        resourcePanel(*this),
        inspectorPanel(*this),
        navMeshPanel(*this),
        undoStack(*this)
    {
        Instance = this;
        NFD_Init();

        GetVFS().addRoot("editor", std::filesystem::current_path());
        engine.setShutdownRequestHandler([this]() {
            stopSimulation();
        });
        GetEngine().setSkybox(Carrot::Skybox::Type::Forest);
        attachSettings(settings);

        pushLayer<GizmosLayer>();

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
        currentScene.bindToViewport(gameViewport);
        setupGameViewport();

        // register components & systems for serialisation
        {
            auto& systems = Carrot::ECS::getSystemLibrary();
            // CollisionShapeRenderer is not serializable
            // LightEditorRenderer is not serializable
        }

        settings.load();
        settings.useMostRecent();
        if(settings.currentProject) {
            scheduleLoad(settings.currentProject.value());
        }

        resourcePanel.updateCurrentFolder("game://");
    }

    void Application::checkForOutdatedFormat() {
        checkedForOutdatedFormat = true;
        rapidjson::Document description;
        try {
            description.Parse(Carrot::IO::readFileAsText(projectToLoad.string()).c_str());
        } catch(std::runtime_error& e) {
            Carrot::Log::error("Failed to load project %s: %s", Carrot::toString(projectToLoad.u8string().c_str()).c_str(), e.what());
            projectToLoad = EmptyProject;
        }

        if (description.HasMember("scenes")) {
            const auto scenesArray = description["scenes"].GetArray();
            outdatedScenePaths.ensureReserve(scenesArray.Size());
            for (const auto& sceneElement : scenesArray) {
                std::string_view path { sceneElement.GetString(), sceneElement.GetStringLength() };
                if (path.ends_with(".json")) {
                    outdatedScenePaths.pushBack(std::string{path});
                }
            }
        }
    }

    void Application::displayOutdatedFormatPopup() {
        if (!outdatedScenePaths.empty()) {
            auto result = Carrot::Widgets::drawMessageBox("Outdated scenes",
                "You have scenes in an outdated format. Do you want to convert them to the new format?\n"
                "Refusing will cancel the load of your project.", Carrot::Widgets::MessageBoxIcon::Warning, Carrot::Widgets::MessageBoxButtons::Yes | Carrot::Widgets::MessageBoxButtons::No);
            if (result.has_value()) {
                switch (result.value()) {
                    case Carrot::Widgets::MessageBoxButtons::Yes:
                        userConfirmedUpdateToNewFormat = true;
                        break;

                    case Carrot::Widgets::MessageBoxButtons::No: {
                        // cancel loading
                        projectToLoad = EmptyProject;
                        deferredLoad();
                        return;
                    }

                    default: TODO;
                }
            }
        } else {
            userConfirmedUpdateToNewFormat = true;
        }
    }

    void Application::tick(double frameTime) {
        if(wantsToLoadProject && projectToLoad != EmptyProject) {
            if (!checkedForOutdatedFormat) {
                checkForOutdatedFormat();
                checkedForOutdatedFormat = true;
            }

            if (projectToLoadHasOutdatedFormat) {
                displayOutdatedFormatPopup();
                if (!userConfirmedUpdateToNewFormat) {
                    return;
                }
            }

            if (deferredLoad()) {
                wantsToLoadProject = false;
                projectToLoad = std::filesystem::path{};
            }
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

        for (int j = sceneViewLayersStack.size()-1; j >= 0; j--) {
            auto* pLayer = sceneViewLayersStack[j].get();
            pLayer->tick(frameTime);

            if(!pLayer->showLayersBelow()) {
                break;
            }
        }
        flushLayers();

        if (pParticleEditor) {
            pParticleEditor->tick(frameTime);
        }

        if(movingGameViewCamera && !isPlaying) {
            cameraController.move(moveCamera.getValue().x * cameraSpeedMultiplier, moveCamera.getValue().y * cameraSpeedMultiplier, (moveCameraUp.getValue() - moveCameraDown.getValue()) * cameraSpeedMultiplier,
                turnCamera.getValue().x, turnCamera.getValue().y,
                frameTime*5);
        }
    }

    void Application::prePhysics() {
        currentScene.prePhysics();
    }

    void Application::postPhysics() {
        currentScene.postPhysics();
    }

    void Application::recordOpaqueGBufferPass(vk::RenderPass pass, const Carrot::Render::Context& renderContext,
                                              vk::CommandBuffer& commands) {
        // no op, everything is done inside gameViewport
    }

    void Application::recordTransparentGBufferPass(vk::RenderPass pass, const Carrot::Render::Context& renderContext,
                                                   vk::CommandBuffer& commands) {
        // no op, everything is done inside gameViewport
    }

    void Application::performLoad(std::filesystem::path fileToOpen) {
        projectToLoad = fileToOpen;
        wantsToLoadProject = true;
        checkedForOutdatedFormat = false;
        projectToLoadHasOutdatedFormat = false;
        userConfirmedUpdateToNewFormat = false;
        outdatedScenePaths.clear();
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

    void Application::addCurrentSceneToSceneList() {
        if(std::find(knownScenes.begin(), knownScenes.end(), scenePath) == knownScenes.end()) {
            knownScenes.pushBack(scenePath);
        }
    }

    void Application::newScene(const Carrot::IO::VFS::Path& path) {
        openUnsavedChangesPopup([this, path]() {
            scenePath = path;
            currentScene.clear();

            addEditingSystems();
            currentScene.world.freezeLogic();
            currentScene.load();

            hasUnsavedChanges = true;
            addCurrentSceneToSceneList();
        }, [](){});
    }

    void Application::openScene(const Carrot::IO::VFS::Path& path) {
        openUnsavedChangesPopup([this, path]() {
            scenePath = path;
            try {
                currentScene.clear();
                currentScene.deserialise(scenePath);
                undoStack.clear();
            } catch (std::exception& e) {
                Carrot::Log::error("Failed to open scene: %s", e.what());
                currentScene.clear();
            }

            addEditingSystems();
            currentScene.world.freezeLogic();
            currentScene.load();

            hasUnsavedChanges = true;
            addCurrentSceneToSceneList();
        }, [](){});
    }

    void Application::saveCurrentScene() {
        rapidjson::Value scene(rapidjson::kObjectType);
        rapidjson::Document sceneData;
        sceneData.SetObject();

        auto outputFolder = GetVFS().resolve(scenePath);
        currentScene.serialise(outputFolder);

        // force other scenes to reload the instances of this prefab
        GetAssetServer().removePrefab(scenePath);

        addCurrentSceneToSceneList();
    }

    bool Application::ErrorReport::hasErrors() const {
        return !errorMessagesPerEntity.empty();
    }

    static Carrot::StackAllocator entityListAllocator{ Carrot::Allocator::getDefault() }; // to avoid reallocating each frame, reuse memory

    void Application::checkErrors() {
        errorReport.errorMessagesPerEntity.clear();
        entityListAllocator.clear();

        struct EntityList: Carrot::Vector<Carrot::ECS::EntityID> {
            EntityList(): Vector(entityListAllocator) {}
        };

        struct DuplicateNameChecker {
            void addName(const Carrot::ECS::EntityID& entityID, const std::string& name) {
                name2entities[Carrot::toLowerCase(name)].pushBack(entityID);
            }

            void finalise(ErrorReport& report) {
                std::erase_if(name2entities, [](const auto& pair) {
                    return pair.second.size() == 1;
                });

                for (const auto& [_, entityIDs] : name2entities) {
                    for (const auto& entityID : entityIDs) {
                        report.errorMessagesPerEntity[entityID].pushBack("This entity has the same name as another at the same level of hierarchy.\nScene cannot be saved in this state!");
                    }
                }
            }

            std::unordered_map<std::string, EntityList> name2entities;
        };

        // check that there are no two entities with the same name at the same hierarchy level
        std::function<void(Carrot::ECS::Entity)> recursiveCheck = [&](Carrot::ECS::Entity entity) {
            auto directChildren = entity.getChildren(Carrot::ShouldRecurse::NoRecursion);
            DuplicateNameChecker checker;
            for (Carrot::ECS::Entity& child : directChildren) {
                checker.addName(child.getID(), child.getName());
                recursiveCheck(child);
            }
            checker.finalise(errorReport);
        };

        DuplicateNameChecker rootChecker;
        for (auto& entity : currentScene.world.getAllEntities()) {
            if (entity.getParent().has_value()) {
                continue;
            }

            rootChecker.addName(entity.getID(), entity.getName());
            recursiveCheck(entity);
        }
        rootChecker.finalise(errorReport);
    }

    bool Application::deferredLoad() {
        wantsToLoadProject = false;
        projectToLoadHasOutdatedFormat = false;
        checkedForOutdatedFormat = false;
        userConfirmedUpdateToNewFormat = false;
        GetVFS().removeRoot("game");
        if(projectToLoad == EmptyProject) {
            currentScene.clear();
            knownScenes.clear();
            scenePath = "game://scenes/main";
            addDefaultSystems(currentScene);
            hasUnsavedChanges = false;
            settings.currentProject.reset();
            deselectAllEntities();
            updateWindowTitle();
            currentScene.world.freezeLogic();
            return true;
        }
        GetVFS().addRoot("game", std::filesystem::absolute(projectToLoad).parent_path());
        rapidjson::Document description;
        try {
            description.Parse(Carrot::IO::readFileAsText(projectToLoad.string()).c_str());
        } catch(std::runtime_error& e) {
            Carrot::Log::error("Failed to load project %s: %s", Carrot::toString(projectToLoad.u8string().c_str()).c_str(), e.what());
            projectToLoad = EmptyProject;
            return deferredLoad();
        }

        // find outdated scenes
        Carrot::Vector<Carrot::IO::VFS::Path> scenePaths;
        bool needToSave = false;
        if (description.HasMember("scenes")) {
            const auto scenesArray = description["scenes"].GetArray();
            scenePaths.ensureReserve(scenesArray.Size());
            for (const auto& sceneElement : scenesArray) {
                std::string_view path { sceneElement.GetString(), sceneElement.GetStringLength() };
                const bool isOutdated = path.ends_with(".json");
                Carrot::IO::VFS::Path scenePath = path;
                scenePaths.emplaceBack(scenePath);

                // update outdated scenes
                if (isOutdated)
                {
                    const fs::path realScenePath = GetVFS().resolve(scenePath);
                    const fs::path sceneFolder = realScenePath.parent_path();
                    Carrot::Log::info("Converting scene in '%s'...", Carrot::toString(realScenePath.u8string()));
                    Carrot::SceneConverter::convert(realScenePath, sceneFolder);
                    Carrot::Log::info("Finished!");

                    fs::remove(realScenePath);
                    scenePath = Carrot::IO::VFS::Path{ scenePath.getRoot(), scenePath.getPath().withExtension("") };
                    needToSave = true;
                }
            }
        }

        auto& layersManager = GetPhysics().getCollisionLayers();
        layersManager.reset();
        if(description.HasMember("collision_layers")) {
            std::unordered_map<std::string, Carrot::Physics::CollisionLayerID> name2layer;
            name2layer[layersManager.getLayer(layersManager.getStaticLayer()).name] = layersManager.getStaticLayer();
            name2layer[layersManager.getLayer(layersManager.getMovingLayer()).name] = layersManager.getMovingLayer();
            for(const auto& [key, obj] : description["collision_layers"].GetObject()) {
                auto layerID = layersManager.newLayer(std::string{ key.GetString(), key.GetStringLength() }, obj["static"].GetBool());
                name2layer[key.GetString()] = layerID;
            }

            for(const auto& [key, obj] : description["collision_layers"].GetObject()) {
                if(obj.HasMember("does_not_collide_with")) {
                    for(const auto& otherLayerName : obj["does_not_collide_with"].GetArray()) {
                        auto it = name2layer.find(std::string{ otherLayerName.GetString(), otherLayerName.GetStringLength() });
                        if(it == name2layer.end()) {
                            Carrot::Log::warn("Unknown collision layer: %s, when parsing collides_with of %s", otherLayerName.GetString(), key.GetString());
                            continue;
                        }

                        layersManager.setCanCollide(name2layer[key.GetString()], it->second, false);
                    }
                }
            }
        }

        currentScene.clear(); // clear all references to C# code
        reloadGameAssembly();
        settings.currentProject = projectToLoad;
        settings.addToRecentProjects(projectToLoad);
        hasUnsavedChanges = false;

        knownScenes.clear();
        if(description.HasMember("scenes")) {
            knownScenes = scenePaths;
        } else {
            knownScenes.pushBack(scenePath);
        }
        {
            openScene(description["scene"].GetString());
        }

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

            auto& window = engine.getMainWindow();
            window.setPosition(x, y);
            window.setWindowSize(width, height);
            if(maximised) {
                window.maximise();
            }
        }

        selectedEntityIDs.clear();
        if(description.HasMember("selectedIDs")) {
            for(const auto& selectedIDValue : description["selectedIDs"].GetArray()) {
                auto wantedSelection = Carrot::UUID::fromString(selectedIDValue.GetString());
                if(currentScene.world.exists(wantedSelection)) {
                    selectedEntityIDs.insert(wantedSelection);
                }
            }
        }

        if (description.HasMember("open_tools")) {
            showParticleEditor = false;
            showPhysicsSettings = false;
            for (const auto& tools : description["open_tools"].GetArray()) {
                std::string_view openedTool { tools.GetString(), tools.GetStringLength() };
                if (openedTool == "particle_editor") {
                    showParticleEditor = true;
                } else if (openedTool == "physics_settings") {
                    showPhysicsSettings = true;
                }
            }
        }

        updateWindowTitle();

        if (needToSave) {
            triggerSave();
        }
        return true;
    }

    void Application::saveToFile(std::filesystem::path path) {
        rapidjson::Document document;
        document.SetObject();

        {
            if(!GetVFS().hasRoot("game")) {
                GetVFS().addRoot("game", path.parent_path());
            }

            if(scenePath.isGeneric()) {
                scenePath = Carrot::IO::VFS::Path("game", scenePath.getPath());
            }

            saveCurrentScene();
            document.AddMember("scene", scenePath.toString(), document.GetAllocator());

            if(!knownScenes.empty()) {
                rapidjson::Value knownScenesArray { rapidjson::kArrayType };
                for(const auto& knownScene : knownScenes) {
                    knownScenesArray.PushBack(rapidjson::Value { knownScene.toString().c_str(), document.GetAllocator() }, document.GetAllocator());
                }
                document.AddMember("scenes", knownScenesArray, document.GetAllocator());
            }
        }

        if(!selectedEntityIDs.empty()) {
            rapidjson::Value uuidArray{rapidjson::kArrayType};
            for(const auto& id : selectedEntityIDs) {
                uuidArray.PushBack(rapidjson::Value(id.toString().c_str(), document.GetAllocator()), document.GetAllocator());
            }
            document.AddMember("selectedIDs", uuidArray, document.GetAllocator());
        }

        {
            document.AddMember("camera", cameraController.serialise(document), document.GetAllocator());
        }

        {
            rapidjson::Value openedTools{ rapidjson::kArrayType };
            if (showParticleEditor) {
                openedTools.PushBack(rapidjson::Value{"particle_editor", document.GetAllocator()}, document.GetAllocator());
            }
            if (showPhysicsSettings) {
                openedTools.PushBack(rapidjson::Value{"physics_settings", document.GetAllocator()}, document.GetAllocator());
            }

            document.AddMember("open_tools", openedTools, document.GetAllocator());
        }

        {
            rapidjson::Value windowObj(rapidjson::kObjectType);

            const auto& window = engine.getMainWindow();

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

        {
            auto& layersManager = GetPhysics().getCollisionLayers();
            rapidjson::Value layersObj(rapidjson::kObjectType);

            for(const auto& layer : layersManager.getLayers()) {
                // remove default layers
                if(layer.layerID == layersManager.getStaticLayer()) {
                    continue;
                }
                if(layer.layerID == layersManager.getMovingLayer()) {
                    continue;
                }

                rapidjson::Value layerObj(rapidjson::kObjectType);

                layerObj.AddMember("static", layer.isStatic, document.GetAllocator());

                rapidjson::Value doesNotCollideArray{ rapidjson::kArrayType };
                for(const auto& otherLayer : layersManager.getLayers()) {
                    if(!layersManager.canCollide(layer.layerID, otherLayer.layerID)) {
                        doesNotCollideArray.PushBack(rapidjson::Value{otherLayer.name.c_str(), document.GetAllocator()}, document.GetAllocator());
                    }
                }

                if(!doesNotCollideArray.GetArray().Empty()) {
                    layerObj.AddMember("does_not_collide_with", doesNotCollideArray, document.GetAllocator());
                }

                layersObj.AddMember(rapidjson::Value(layer.name.c_str(), document.GetAllocator()), layerObj, document.GetAllocator());
            }

            document.AddMember("collision_layers", layersObj, document.GetAllocator());
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
        return !isPlaying && !errorReport.hasErrors();
    }

    void Application::onSwapchainSizeChange(Carrot::Window& window, int newWidth, int newHeight) {

    }

    void Application::onSwapchainImageCountChange(std::size_t newCount) {

    }

    void Application::addEntityMenu(std::optional<Carrot::ECS::Entity> parent) {
        if(ImGui::MenuItem("Add Cube##add entity menu")) {
            auto entity = addEntity(parent);
            entity.addComponent<Carrot::ECS::ModelComponent>();
            Carrot::ECS::ModelComponent& modelComp = entity.getComponent<Carrot::ECS::ModelComponent>();
            modelComp.asyncModel = Carrot::AsyncModelResource(GetAssetServer().loadModelTask("resources/models/simple_cube.obj"));
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

    void Application::changeEntityParent(const Carrot::ECS::EntityID& toChangeID, std::optional<Carrot::ECS::Entity> newParent) {
        auto entityToChange = currentScene.world.wrap(toChangeID);
        entityToChange.reparent(newParent);
    }

    void Application::duplicateSelectedEntities() {
        if (selectedEntityIDs.empty()) {
            return;
        }

        undoStack.push<DuplicateEntitiesCommand>(selectedEntityIDs);
    }

    void Application::selectEntity(const Carrot::ECS::EntityID& entity, bool additive) {
        inspectorType = InspectorType::Entities;
        if(!additive) {
            selectedEntityIDs.clear();
        }
        selectedEntityIDs.insert(entity);

        auto* shapeRenderer = currentScene.world.getRenderSystem<ECS::CollisionShapeRenderer>();
        if(shapeRenderer) {
            shapeRenderer->setSelected(selectedEntityIDs);
        }
    }

    void Application::deselectAllEntities() {
        selectedEntityIDs.clear();

        auto* shapeRenderer = currentScene.world.getRenderSystem<ECS::CollisionShapeRenderer>();
        if(shapeRenderer) {
            shapeRenderer->setSelected(selectedEntityIDs);
        }
    }

    void Application::removeSelectedEntities() {
        if (selectedEntityIDs.empty()) {
            return;
        }

        undoStack.push<DeleteEntitiesCommand>(selectedEntityIDs);
    }

    void Application::convertEntityToPrefab(Carrot::ECS::Entity& entity) {
        nfdchar_t* savePath;

        // prepare filters for the dialog
        nfdfilteritem_t filterItem[1] = {{"Carrot Prefab", "cprefab"}};

        // show the dialog
        std::string defaultName = Carrot::sprintf("%s", std::string(entity.getName()).c_str());
        nfdresult_t result = NFD_SaveDialog(&savePath, filterItem, 1, nullptr, defaultName.c_str());
        if (result == NFD_OKAY) {
            std::optional<Carrot::IO::VFS::Path> vfsPathOpt = GetVFS().represent(savePath);
            if(vfsPathOpt.has_value()) {
                auto pPrefab = Carrot::ECS::Prefab::makePrefab();
                const Carrot::IO::VFS::Path& vfsPath = vfsPathOpt.value();
                pPrefab->createFromEntity(entity);
                bool success = pPrefab->save(vfsPath);
                if(success) {
                    // replace entity with an instance of the prefab
                    Carrot::ECS::Entity instance = pPrefab->instantiate(entity.getWorld());
                    entity.remove();
                } else {
                    Carrot::Log::error("Failed to save prefab at %s :(", vfsPath.toString().c_str());
                }
            } else {
                Carrot::Log::error("File %s is not inside VFS, cannot save prefab to it.", savePath);
            }

            // remember to free the memory (since NFD_OKAY is returned)
            NFD_FreePath(savePath);
        } else if (result == NFD_CANCEL) {
            /* no op */ ;
        } else {
            std::string msg = "Error: ";
            msg += NFD_GetError();
            throw std::runtime_error(msg);
        }
    }

    bool Application::drawCantSavePopup() {
        if (isPlaying) {
            return Carrot::Widgets::drawMessageBox("Cannot save", "Game is running", Carrot::Widgets::MessageBoxIcon::Info, Carrot::Widgets::MessageBoxButtons::Ok) != Carrot::Widgets::MessageBoxButtons::Ok;
        } else if (errorReport.hasErrors()) {
            return Carrot::Widgets::drawMessageBox("Cannot save", "Scene has errors!", Carrot::Widgets::MessageBoxIcon::Error, Carrot::Widgets::MessageBoxButtons::Ok) != Carrot::Widgets::MessageBoxButtons::Ok;
        }
        return true;
    }

    void Application::requestOpenParticleEditor(const Carrot::IO::VFS::Path& particleFile) {
        std::optional<std::filesystem::path> path = GetVFS().safeResolve(particleFile);
        if (!path.has_value()) {
            return;
        }

        if (!pParticleEditor) {
            pParticleEditor = Carrot::makeUnique<Peeler::ParticleEditor>(Carrot::Allocator::getDefault(), engine);
        }

        pParticleEditor->scheduleLoad(path.value());
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
        engine.getMainWindow().setTitle(Carrot::sprintf("Peeler - %s%s", projectName.c_str(), hasUnsavedChanges ? "*" : ""));
    }

    static std::shared_ptr<Carrot::Render::LightHandle> lightHandle = nullptr;

    void Application::addDefaultSystems(Carrot::Scene& scene) {
        scene.world.addRenderSystem<Carrot::ECS::SpriteRenderSystem>();
        scene.world.addRenderSystem<Carrot::ECS::ModelRenderSystem>();
        scene.world.addRenderSystem<Carrot::ECS::SystemHandleLights>();

        scene.world.addLogicSystem<Carrot::ECS::SystemKinematics>();
        scene.world.addLogicSystem<Carrot::ECS::SystemTransformSwapBuffers>();
        //scene.world.addLogicSystem<Carrot::ECS::SystemSinPosition>();
        scene.world.addLogicSystem<Carrot::ECS::RigidBodySystem>();

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

    void Application::onCutShortcut(const Carrot::Render::Context& frame) {
        shortcuts.cutRequested = true;
    }

    void Application::onCopyShortcut(const Carrot::Render::Context& frame) {
        shortcuts.copyRequested = true;
    }

    void Application::onPasteShortcut(const Carrot::Render::Context& frame) {
        shortcuts.pasteRequested = true;
    }

    void Application::onDuplicateShortcut(const Carrot::Render::Context& frame) {
        shortcuts.duplicateRequested = true;
    }

    void Application::onDeleteShortcut(const Carrot::Render::Context& frame) {
        shortcuts.deleteRequested = true;
    }

    void Application::onUndoShortcut(const Carrot::Render::Context& frame) {
        shortcuts.undoRequested = true;
    }

    void Application::onRedoShortcut(const Carrot::Render::Context& frame) {
        shortcuts.redoRequested = true;
    }

}