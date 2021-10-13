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
#include "Scene.h"

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

    private:
        void addEntity(std::optional<Carrot::ECS::Entity> parent = {});
        void duplicateEntity(const Carrot::ECS::Entity& entity);
        void removeEntity(const Carrot::ECS::Entity& entity);

        void addDefaultSystems(Scene& scene);

    private: // simulation
        void startSimulation();
        void stopSimulation();

    private:
        ImGuiID mainDockspace;
        Tools::EditorSettings settings;
        Carrot::Render::Viewport& gameViewport;
        Carrot::Render::FrameResource gameTexture;
        std::unique_ptr<Carrot::Render::Graph> gameRenderingGraph;

        Carrot::Render::Texture playButtonIcon;
        Carrot::Render::Texture stopButtonIcon;
        Carrot::Render::Texture translateIcon;
        Carrot::Render::Texture rotateIcon;
        Carrot::Render::Texture scaleIcon;
        Carrot::Render::Texture genericFileIcon;
        Carrot::Render::Texture folderIcon;
        Carrot::Render::Texture parentFolderIcon;
        Carrot::Render::Texture driveIcon;

        std::optional<Carrot::ECS::EntityID> selectedID;
        bool movingGameViewCamera = false;

        bool hasUnsavedChanges = false;

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

    private: // simulation state
        bool isPlaying = false;
    };

}
