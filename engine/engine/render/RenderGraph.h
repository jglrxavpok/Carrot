//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once
#include "engine/vulkan/VulkanDriver.h"
#include "RenderPass.h"
#include "RenderContext.h"
#include <list>
#include "core/utils/UUID.h"
#include "RenderPassData.h"

namespace Carrot::Render {
    class Graph: public SwapchainAware {
    public:
        explicit Graph(VulkanDriver& driver);

        void execute(const Render::Context& data, vk::CommandBuffer& cmds);

    public:
        [[nodiscard]] Render::Texture& createTexture(const FrameResource& resource, size_t frameIndex, const vk::Extent2D& viewportSize);
        [[nodiscard]] Render::Texture& getTexture(const Carrot::UUID& resourceID, size_t frameIndex) const;
        [[nodiscard]] Render::Texture& getTexture(const FrameResource& resource, size_t frameIndex) const;
        [[nodiscard]] Render::Texture& getOrCreateTexture(const FrameResource& resource, size_t frameIndex, const vk::Extent2D& viewportSize);

        VulkanDriver& getVulkanDriver() { return driver; }

    public:
        Render::CompiledPass* getPass(std::string_view passName) const;

        template<typename Type>
        std::optional<Type> getPassData(std::string_view passName) const {
            for(const auto& [name, passData] : passesData) {
                if(name == passName)
                    return std::any_cast<Type>(passData);
            }
            return {};
        }

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        Carrot::VulkanDriver& driver;
        std::list<std::pair<std::string, std::unique_ptr<Render::CompiledPass>>> passes;
        std::vector<Render::CompiledPass*> sortedPasses;
        std::list<std::pair<std::string, std::any>> passesData;

        friend class GraphBuilder;
    };

    class GraphBuilder {
        template<typename Data>
        using SetupPassCallback = std::function<void(GraphBuilder&, Pass<Data>&, Data&)>;

    public:
        explicit GraphBuilder(Carrot::VulkanDriver& driver);
        GraphBuilder(const GraphBuilder& toCopy) = default;
        GraphBuilder& operator=(const GraphBuilder& toCopy) = default;

    public:
        template<typename Data>
        Pass<Data>& addPass(const std::string& name, const SetupPassCallback<Data>& setup, const ExecutePassCallback<Data>& execute,
                            const PostCompileCallback<Data>& postCompile = [](CompiledPass&, Data&){}) {
            auto& pair = passes.emplace_back(name, std::move(std::make_shared<Render::Pass<Data>>(getVulkanDriver(), name, execute, postCompile)));
            currentPass = pair.second.get();
            auto* pass = static_cast<Render::Pass<Data>*>(currentPass);
            setup(*this, *pass, pass->data);

            passesData.emplace_back(name, pass->data);

            currentPass = nullptr;
            return *pass;
        }

        std::unique_ptr<Graph> compile();

    public:
        FrameResource& getSwapchainImage() {
            return swapchainImage;
        }

        // TODO: remove
        VulkanDriver& getVulkanDriver();

    public:
        FrameResource& read(const FrameResource& toRead, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);
        FrameResource& write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ImageAspectFlags aspect);
        FrameResource& write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);
        FrameResource& createRenderTarget(vk::Format format, TextureSize size, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal);
        void present(FrameResource& toPresent);

        template<typename Type>
        std::optional<Type> getPassData(std::string_view passName) const {
            for(const auto& [name, passData] : passesData) {
                if(name == passName)
                    return std::any_cast<Type>(passData);
            }
            return {};
        }

    private:
        FrameResource swapchainImage;
        std::list<FrameResource> resources;
        std::set<Carrot::UUID> toPresent;
        std::list<std::pair<std::string, std::shared_ptr<Render::PassBase>>> passes;
        std::list<std::pair<std::string, std::any>> passesData;
        Render::PassBase* currentPass = nullptr;
    };

}
