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
#include "Scene.h"
#include "GridRenderer.h"

namespace Peeler {
    class Application: public Carrot::CarrotGame, public Tools::ProjectMenuHolder {
    public:
        explicit Application(Carrot::Engine& engine);

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

    private:
        void markDirty();
        void updateWindowTitle();

    private: // UI
        void UIEditor(const Carrot::Render::Context& renderContext);
        void UIGameView(const Carrot::Render::Context& renderContext);
        void UIPlayBar(const Carrot::Render::Context& renderContext);
        void UIWorldHierarchy(const Carrot::Render::Context& renderContext);
        void UIInspector(const Carrot::Render::Context& renderContext);
        void UIResourcesPanel(const Carrot::Render::Context& renderContext);
        void UISceneProperties(const Carrot::Render::Context& renderContext);

    private:
        void addEntityMenu(std::optional<Carrot::ECS::Entity> parent = {});
        Carrot::ECS::Entity addEntity(std::optional<Carrot::ECS::Entity> parent = {});
        void duplicateEntity(const Carrot::ECS::Entity& entity, std::optional<Carrot::ECS::Entity> parent = {});
        void removeEntity(const Carrot::ECS::Entity& entity);

        void addDefaultSystems(Scene& scene);
        void addEditingSystems();
        void removeEditingSystems();

        void selectEntity(const Carrot::ECS::EntityID& entity);
        void deselectAllEntities();

    private: // simulation
        void pauseSimulation();
        void resumeSimulation();

        // launch a request
        void startSimulation();
        void stopSimulation();

        // process the request, don't call mid-frame
        void performSimulationStart();
        void performSimulationStop();

        void updateSettingsBasedOnScene();

    private:
        ImGuiID mainDockspace;
        Tools::EditorSettings settings;
        Carrot::Render::Viewport& gameViewport;
        Carrot::Render::FrameResource gameTexture;

        Carrot::Render::Texture playButtonIcon;
        Carrot::Render::Texture playActiveButtonIcon;
        Carrot::Render::Texture pauseButtonIcon;
        Carrot::Render::Texture pauseActiveButtonIcon;
        Carrot::Render::Texture stepButtonIcon;
        Carrot::Render::Texture stopButtonIcon;
        Carrot::Render::Texture translateIcon;
        Carrot::Render::Texture rotateIcon;
        Carrot::Render::Texture scaleIcon;
        Carrot::Render::Texture genericFileIcon;
        Carrot::Render::Texture folderIcon;
        Carrot::Render::Texture parentFolderIcon;
        Carrot::Render::Texture driveIcon;

        // hold game texture available at least for the frame it is used. The texture needs to be available at least until the next frame for ImGui to be able to use it.
        Carrot::Render::Texture::Ref gameTextureRef;

        GridRenderer gridRenderer;

    private: // Scene manipulation

        std::optional<Carrot::ECS::EntityID> selectedID;
        bool movingGameViewCamera = false;

        bool hasUnsavedChanges = false;

        Carrot::IO::VFS::Path scenePath = "game://scenes/main.json";
        Scene currentScene;
        Scene savedScene;

    private: // inputs
        Carrot::IO::ActionSet editorActions { "Editor actions" };
        Carrot::IO::Vec2InputAction moveCamera { "Move camera (strafe & forward) " };
        Carrot::IO::FloatInputAction moveCameraUp { "Move camera (up) " };
        Carrot::IO::FloatInputAction moveCameraDown { "Move camera (down) " };
        Carrot::IO::Vec2InputAction turnCamera { "Turn camera " };

        Carrot::Edition::FreeCameraController cameraController;

    private: // resources
        void updateCurrentFolder(std::filesystem::path path);

        enum class ResourceType {
            GenericFile,
            Folder,
            Drive,
            // TODO: images, shaders, scenes, etc.
        };

        struct ResourceEntry {
            ResourceType type = ResourceType::GenericFile;
            std::filesystem::path path;
        };
        std::filesystem::path currentFolder = "";
        std::vector<ResourceEntry> resourcesInCurrentFolder;

        bool isLookingAtRoots = false;
        bool tryToClose = false;

    private:
        // don't actually start/stop the simulation mid-frame! We might have live objects that need to still be alive at the end of the frame
        bool stopSimulationRequested = false;
        bool startSimulationRequested = false;

    private: // simulation state
        bool isPlaying = false;
        bool isPaused = false;
        bool requestedSingleStep = false;
        bool hasDoneSingleStep = false;
    };

}
