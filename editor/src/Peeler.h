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
#include "panels/ResourcePanel.h"
#include "layers/ISceneViewLayer.h"

namespace Peeler {
    extern Application* Instance;

    class Application: public Carrot::CarrotGame, public Tools::ProjectMenuHolder {
    public:

        explicit Application(Carrot::Engine& engine);

        void setupCamera(Carrot::Render::Context renderContext) override;
        void onFrame(Carrot::Render::Context renderContext) override;

        void tick(double frameTime) override;

        void recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                                     vk::CommandBuffer& commands) override;

        void recordTransparentGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                                          vk::CommandBuffer& commands) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(std::size_t newCount) override;

        bool onCloseButtonPressed() override;

    public: // project management
        void performLoad(std::filesystem::path path) override;

        void saveToFile(std::filesystem::path path) override;

        bool showUnsavedChangesPopup() override;
        bool canSave() const override;

        void drawCantSavePopup() override;

    public:
        Carrot::ECS::Entity addEntity(std::optional<Carrot::ECS::Entity> parent = {});
        void changeEntityParent(const Carrot::ECS::EntityID& entityToChange, std::optional<Carrot::ECS::Entity> newParent);

    private:
        void deferredLoad();

    public:
        void markDirty();

    private:
        void updateWindowTitle();

    private: // internal UI
        void UIEditor(const Carrot::Render::Context& renderContext);
        void UIGameView(const Carrot::Render::Context& renderContext);
        void UIPlayBar(const Carrot::Render::Context& renderContext);
        void UIWorldHierarchy(const Carrot::Render::Context& renderContext);
        void UIInspector(const Carrot::Render::Context& renderContext);
        void UISceneProperties(const Carrot::Render::Context& renderContext);

    private: // C# project handling
        void buildCSProject(const Carrot::IO::VFS::Path& csproj);
        void reloadGameAssembly();

    public: // widgets

        /**
         * Returns true iif an entity was picked this frame. Stores the result in 'destination' if one was found
         */
        bool drawPickEntityWidget(const char* label, Carrot::ECS::Entity& destination);

    private:
        void addEntityMenu(std::optional<Carrot::ECS::Entity> parent = {});

    public:
        void duplicateEntity(const Carrot::ECS::Entity& entity, std::optional<Carrot::ECS::Entity> parent = {});
        void removeEntity(const Carrot::ECS::Entity& entity);

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

    private:
        ImGuiID mainDockspace;

        bool wantsToLoadProject = false;
        std::filesystem::path projectToLoad;

        Tools::EditorSettings settings;

    public: // for layers
        Carrot::Render::Viewport& gameViewport;
        // hold game texture available at least for the frame it is used. The texture needs to be available at least until the next frame for ImGui to be able to use it.
        Carrot::Render::Texture::Ref gameTextureRef;
        std::vector<Carrot::ECS::EntityID> selectedIDs;

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

    private: // Scene manipulation
        bool movingGameViewCamera = false;

        bool hasUnsavedChanges = false;

        Carrot::IO::VFS::Path scenePath = "game://scenes/main.json";
        Carrot::UUID entityIDPickedThisFrame;

    public:
        Carrot::Scene currentScene;

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

    private: // simulation state
        bool isPlaying = false;
        bool isPaused = false;
        bool requestedSingleStep = false;
        bool hasDoneSingleStep = false;
    };

}
