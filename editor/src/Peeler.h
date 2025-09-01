//
// Created by jglrxavpok on 29/09/2021.
//

#pragma once
#include <engine/CarrotGame.h>
#include <engine/edition/ProjectMenuHolder.h>
#include <engine/edition/EditorSettings.h>
#include <engine/edition/FreeCameraController.h>
#include <engine/render/Viewport.h>
#include <engine/ecs/World.h>
#include <engine/io/actions/Action.hpp>
#include <engine/io/actions/ActionSet.h>
#include <optional>
#include <core/io/vfs/VirtualFileSystem.h>
#include <engine/scene/Scene.h>
#include "GridRenderer.h"
#include "panels/InspectorPanel.h"
#include "panels/NavMeshPanel.h"
#include "panels/ResourcePanel.h"
#include "layers/ISceneViewLayer.h"
#include <commands/UndoStack.h>
#include <particle_editor/ParticleEditor.h>

namespace Peeler {
    extern Application* Instance;

    class Application: public Carrot::CarrotGame, public Tools::ProjectMenuHolder {
        struct ErrorReport {
            std::unordered_map<Carrot::ECS::EntityID, Carrot::Vector<std::string>> errorMessagesPerEntity;

            bool hasErrors() const;
        };

    public:

        explicit Application(Carrot::Engine& engine);

        void setupCamera(const Carrot::Render::Context& renderContext) override;
        void onFrame(const Carrot::Render::Context& renderContext) override;

        void tick(double frameTime) override;
        void prePhysics() override;
        void postPhysics() override;

        void onSwapchainSizeChange(Carrot::Window& window, int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(std::size_t newCount) override;

        bool onCloseButtonPressed() override;

        void onCutShortcut(const Carrot::Render::Context& frame) override;
        void onCopyShortcut(const Carrot::Render::Context& frame) override;
        void onPasteShortcut(const Carrot::Render::Context& frame) override;
        void onDuplicateShortcut(const Carrot::Render::Context& frame) override;
        void onDeleteShortcut(const Carrot::Render::Context& frame) override;
        void onUndoShortcut(const Carrot::Render::Context& frame) override;
        void onRedoShortcut(const Carrot::Render::Context& frame) override;

        // scene management
        void addCurrentSceneToSceneList();
        void newScene(const Carrot::IO::VFS::Path& path);
        void openScene(const Carrot::IO::VFS::Path& path);
        void saveCurrentScene();
        void checkErrors();

    public: // project management
        void performLoad(std::filesystem::path path) override;

        void saveToFile(std::filesystem::path path) override;

        bool showUnsavedChangesPopup() override;
        bool canSave() const override;

        bool drawCantSavePopup() override;
        void exportGame(const std::filesystem::path& outputDirectory);

    public: // tools
        /// Request to open the given file inside the particle editor.
        /// Can fail if:
        /// - the editor is open with unsaved changes, and the user canels the load
        /// - the input file does not exist
        void requestOpenParticleEditor(const Carrot::IO::VFS::Path& particleFile);

    public:
        Carrot::ECS::Entity addEntity(std::optional<Carrot::ECS::Entity> parent = {});
        void changeEntityParent(const Carrot::ECS::EntityID& entityToChange, std::optional<Carrot::ECS::Entity> newParent);

    private:
        Cider::GrowingStack loadStateMachineStack { 1 * 1024 * 1024 };
        Carrot::Async::Counter loadStateMachineReady;
        std::unique_ptr<Cider::Fiber> projectLoadStateMachine;
        bool deferredLoad();
        void checkForOutdatedFormat();
        void displayOutdatedFormatPopup();

    public:
        void markDirty();

    private:
        void updateWindowTitle();

    private: // internal UI
        void UIEditor(const Carrot::Render::Context& renderContext);
        void UIGameView(const Carrot::Render::Context& renderContext);
        void UIPlayBar(const Carrot::Render::Context& renderContext);
        void UIWorldHierarchy(const Carrot::Render::Context& renderContext);
        void UISceneProperties(const Carrot::Render::Context& renderContext);
        void UIStatusBar(const Carrot::Render::Context& renderContext);
        void drawExportMenu();

        void drawEntityErrors(const ErrorReport& report, Carrot::ECS::Entity entity, const char* uniqueWidgetID);
        void drawEntityWarnings(Carrot::ECS::Entity entity, const char* uniqueWidgetID);
        void drawScenesMenu();
        void drawSettingsMenu();
        void drawToolsMenu();
        void drawNewSceneWindow();
        void drawCameraSettingsWindow();
        void drawPhysicsSettingsWindow();

    private: // C# project handling
        void buildCSProject(const Carrot::IO::VFS::Path& csproj);
        void reloadGameAssembly();

    public: // widgets

        /**
         * Returns true iif an entity was picked this frame. Stores the result in 'destination' if one was found
         *
         * nameDisplayOverride !=nullptr is used to tell the widget that it should display the contents of nameDisplayOverride instead of the entity name
         */
        bool drawPickEntityWidget(const char* label, Carrot::ECS::Entity& destination, const char* nameDisplayOverride = nullptr);

    private:
        void addEntityMenu(std::optional<Carrot::ECS::Entity> parent = {});

    public:
        void duplicateSelectedEntities();
        void removeSelectedEntities();
        void convertEntityToPrefab(Carrot::ECS::Entity& entity);

    private:
        void addDefaultSystems(Carrot::Scene& scene);
        void addEditingSystems();
        void removeEditingSystems();

    public:
        void selectEntity(const Carrot::ECS::EntityID& entity, bool additive);
        void deselectAllEntities();

    public:
        template<typename LayerType, typename... Args>
        void pushLayer(Args&&... args) {
            sceneViewLayersStack.emplace_back(std::make_unique<LayerType>(*this, std::forward<Args>(args)...));
        }

        template<typename LayerType>
        bool hasLayer() {
            for (auto& layer : sceneViewLayersStack) {
                if(typeid(*layer) == typeid(LayerType)) {
                    return true;
                }
            }
            return false;
        }

        template<typename LayerType>
        void removeLayer() {
            for (auto& layer : sceneViewLayersStack) {
                if(typeid(*layer) == typeid(LayerType)) {
                    removeLayer(layer.get());
                    break;
                }
            }
        }

        void popLayer();

        /**
         * Remove first occurrence of given layer inside layer stack
         * @param layer
         */
        void removeLayer(ISceneViewLayer* layer);

    private:
        void flushLayers();

    public: // simulation
        void pauseSimulation();
        void resumeSimulation();

        // launch a request
        void startSimulation();
        void stopSimulation();

        // true iif during in game mode
        bool isCurrentlyPlaying() const;

        // true iif in game mode AND paused
        bool isCurrentlyPaused() const;

    private:
        // process the request, don't call mid-frame
        void performSimulationStart();
        void performSimulationStop();

    private: // inside PeelerRendering.cpp
        void setupGameViewport();
        Carrot::Render::FrameResource addOutlinePass(Carrot::Render::GraphBuilder&, const Carrot::Render::FrameResource& finalRenderedImage);

    private:
        ImGuiID mainDockspace;

        bool wantsToLoadProject = false;
        bool checkedForOutdatedFormat = false;
        bool projectToLoadHasOutdatedFormat = false;
        bool userConfirmedUpdateToNewFormat = false;
        Carrot::Vector<std::string> outdatedScenePaths;
        std::filesystem::path projectToLoad;

        Tools::EditorSettings settings;

        bool showNewScenePopup = false;
        bool showExportPopup = false;

        bool showCameraSettings = false;
        float cameraSpeedMultiplier = 1.0f;

        bool showPhysicsSettings = false;
        bool showParticleEditor = false;

    public: // for layers
        Carrot::Render::Viewport& gameViewport;
        // hold game texture available at least for the frame it is used. The texture needs to be available at least until the next frame for ImGui to be able to use it.
        Carrot::Render::Texture::Ref gameTextureRef;

        enum class InspectorType {
            Entities,
            Assets,
            System,
        } inspectorType = InspectorType::Entities;

        std::unordered_set<Carrot::ECS::EntityID> selectedEntityIDs;
        Carrot::Vector<Carrot::IO::VFS::Path> selectedAssetPaths;
        // TODO: Carrot::Vector<Carrot::ECS::System*> selectedSystems;

        UndoStack undoStack;

    private:
        Carrot::Render::Texture playButtonIcon;
        Carrot::Render::Texture playActiveButtonIcon;
        Carrot::Render::Texture pauseButtonIcon;
        Carrot::Render::Texture pauseActiveButtonIcon;
        Carrot::Render::Texture stepButtonIcon;
        Carrot::Render::Texture stopButtonIcon;

        Carrot::Render::FrameResource gameTexture;

        GridRenderer gridRenderer;

        std::vector<std::unique_ptr<ISceneViewLayer>> sceneViewLayersStack;
        std::vector<std::size_t> toDeleteLayers; // store for a single tick to avoid memory corruption after removing a layer
        ResourcePanel resourcePanel;
        InspectorPanel inspectorPanel;
        NavMeshPanel navMeshPanel;

    private:
        Carrot::UniquePtr<Peeler::ParticleEditor> pParticleEditor;

    private: // Scene manipulation
        bool movingGameViewCamera = false;

        bool hasUnsavedChanges = false;

        Carrot::Vector<Carrot::IO::VFS::Path> knownScenes;
        Carrot::IO::VFS::Path scenePath = "game://scenes/main.json";
        Carrot::UUID entityIDPickedThisFrame;

    public:
        Carrot::Scene& currentScene;
        ErrorReport errorReport;

    private:
        Carrot::Scene savedScene;

    private: // inputs
        Carrot::IO::ActionSet editorActions { "editor_actions" };
        Carrot::IO::Vec2InputAction moveCamera { "strafe_forward_camera" };
        Carrot::IO::FloatInputAction moveCameraUp { "move_camera_up" };
        Carrot::IO::FloatInputAction moveCameraDown { "move_camera_down" };
        Carrot::IO::Vec2InputAction turnCamera { "turn_camera" };

        Carrot::Edition::FreeCameraController cameraController;

    private:
        // don't actually start/stop the simulation mid-frame! We might have live objects that need to still be alive at the end of the frame
        bool stopSimulationRequested = false;
        bool startSimulationRequested = false;

        bool tryToClose = false;
        bool gameViewportFocused = false;

    private: // simulation state
        bool isPlaying = false;
        bool isPaused = false;
        bool requestedSingleStep = false;
        bool hasDoneSingleStep = false;

    private: // shortcuts
        struct {
            bool cutRequested = false;
            bool copyRequested = false;
            bool pasteRequested = false;
            bool duplicateRequested = false;
            bool deleteRequested = false;
            bool undoRequested = false;
            bool redoRequested = false;
        } shortcuts;
    };

}
