//
// Created by jglrxavpok on 29/09/2021.
//

#include "Peeler.h"
#include <engine/render/resources/Texture.h>
#include <engine/render/TextureRepository.h>
#include <engine/ecs/components/Transform.h>
#include <engine/ecs/components/SpriteComponent.h>
#include <engine/ecs/systems/SpriteRenderSystem.h>

namespace Peeler {

    void Application::onFrame(Carrot::Render::Context renderContext) {
        if(&renderContext.viewport == &engine.getMainViewport()) {
            UIEditor(renderContext);
        }
        if(&renderContext.viewport == &gameViewport) {
            state.onFrame(renderContext);
            engine.performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
                gameRenderingGraph->execute(renderContext, cmds);
                auto& texture = gameRenderingGraph->getTexture(gameTexture, renderContext.swapchainIndex);
                texture.assumeLayout(vk::ImageLayout::eColorAttachmentOptimal);
                texture.transitionInline(cmds, vk::ImageLayout::eShaderReadOnlyOptimal);
            }, false);
        }
    }

    void Application::UIEditor(const Carrot::Render::Context& renderContext) {
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
    }

    void Application::UIInspector(const Carrot::Render::Context& renderContext) {
        if(selectedID.has_value()) {
            auto& entityID = selectedID.value();
            auto components = state.world.getAllComponents(entityID);
            for(auto& comp : components) {
                comp->drawInspector(renderContext);
            }
        }
    }

    void Application::UIWorldHierarchy(const Carrot::Render::Context& renderContext) {
        if(ImGui::IsItemClicked()) {
            selectedID.reset();
        }

        if (ImGui::BeginPopupContextWindow("##popup world editor")) {
            if(ImGui::MenuItem("Add entity")) {
                auto entity = state.world.newEntity("Entity")
                    .addComponent<Carrot::ECS::Transform>()
                    .addComponent<Carrot::ECS::SpriteComponent>();
                auto testSprite = std::make_shared<Carrot::Render::Sprite>(engine.getRenderer(), engine.getRenderer().getOrCreateTexture("../icon128.png"));
                entity.getComponent<Carrot::ECS::SpriteComponent>()->sprite = testSprite;
            }

            ImGui::EndPopup();
        }

        std::function<void(Carrot::ECS::Entity&)> showEntityTree = [&, id = std::size_t { 0 }] (Carrot::ECS::Entity& entity) mutable {
            if(!entity)
                return;
            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
            if(selectedID.has_value() && selectedID.value() == entity.getID()) {
                nodeFlags |= ImGuiTreeNodeFlags_Selected;
            }
            auto children = state.world.getChildren(entity);
            if(!children.empty()) {
                if(ImGui::TreeNodeEx((void*)entity.getID().hash(), nodeFlags, "%s", state.world.getName(entity).c_str())) {
                    if(ImGui::IsItemClicked()) {
                        selectedID.reset(); // TODO: multi-select
                        selectedID = entity.getID();
                    }

                    for(auto& c : children) {
                        showEntityTree(c);
                    }

                    ImGui::TreePop();
                }
            } else { // has no children
                nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                ImGui::TreeNodeEx((void*)entity.getID().hash(), nodeFlags, "- %s", state.world.getName(entity).c_str());

                if(ImGui::IsItemClicked()) {
                    selectedID.reset(); // TODO: multi-select
                    selectedID = entity.getID();
                }
            }
        };
        for(auto& entityObj : state.world.getAllEntities()) {
            if( ! state.world.getParent(entityObj)) {
                showEntityTree(entityObj);
            }
        }
    }

    void Application::UIGameView(const Carrot::Render::Context& renderContext) {
        ImVec2 entireRegion = ImGui::GetContentRegionAvail();
        auto& texture = gameRenderingGraph->getTexture(gameTexture, renderContext.swapchainIndex);
        ImGui::Image(texture.getImguiID(), entireRegion);

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
        // TODO
    }

    void Application::stopSimulation() {
        isPlaying = false;
        // TODO
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
        stopButtonIcon(engine.getVulkanDriver(), "resources/textures/ui/stop_button.png")


    {
        attachSettings(settings);

        state.world.freezeLogic();
        state.world.addRenderSystem<Carrot::ECS::SpriteRenderSystem>();

        Carrot::Render::GraphBuilder graphBuilder(engine.getVulkanDriver());

        auto& resolvePass = engine.fillInDefaultPipeline(graphBuilder, Carrot::Render::Eye::NoVR,
                                     [&](const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context& frame, vk::CommandBuffer& cmds) {
                                         state.world.recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                         // TODO: game->recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                     },
                                     [&](const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context& frame, vk::CommandBuffer& cmds) {
                                         state.world.recordTransparentGBufferPass(pass.getRenderPass(), frame, cmds);
                                         // TODO: game->recordOpaqueGBufferPass(pass.getRenderPass(), frame, cmds);
                                     });
        gameTexture = resolvePass.getData().resolved;

        engine.getVulkanDriver().getTextureRepository().getUsages(gameTexture.rootID) |= vk::ImageUsageFlagBits::eSampled;

        gameRenderingGraph = std::move(graphBuilder.compile());

        gameViewport.getCamera().setTargetAndPosition(glm::vec3(), glm::vec3(5,5,5));
    }

    void Application::tick(double frameTime) {
        state.tick(frameTime);
    }

    void Application::recordOpaqueGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                                              vk::CommandBuffer& commands) {
        // no op, everything is done inside gameViewport
    }

    void Application::recordTransparentGBufferPass(vk::RenderPass pass, Carrot::Render::Context renderContext,
                                                   vk::CommandBuffer& commands) {
        // no op, everything is done inside gameViewport
    }

    void Application::performLoad(std::filesystem::path path) {
        // TODO
    }

    void Application::saveToFile(std::filesystem::path path) {
        // TODO
    }

    bool Application::showUnsavedChangesPopup() {
        // TODO
        return false;
    }

    void Application::onSwapchainSizeChange(int newWidth, int newHeight) {
        gameRenderingGraph->onSwapchainSizeChange(newWidth, newHeight); // TODO: fix resize of game view
    }

    void Application::onSwapchainImageCountChange(std::size_t newCount) {
        gameRenderingGraph->onSwapchainImageCountChange(newCount);
    }

    void GameState::tick(double frameTime) {
        world.tick(frameTime);
    }

    void GameState::onFrame(const Carrot::Render::Context& renderContext) {
        world.onFrame(renderContext);
    }
}