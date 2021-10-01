//
// Created by jglrxavpok on 29/09/2021.
//

#pragma once
#include <engine/CarrotGame.h>
#include <engine/edition/ProjectMenuHolder.h>
#include <engine/edition/EditorSettings.h>

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

        void performLoad(std::filesystem::path path) override;

        void saveToFile(std::filesystem::path path) override;

        bool showUnsavedChangesPopup() override;

    private:
        ImGuiID mainDockspace;
        Tools::EditorSettings settings;
    };

}
