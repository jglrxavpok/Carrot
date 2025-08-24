//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once
#include "RenderPass.h"
#include "RenderContext.h"
#include <any>
#include <list>
#include "core/utils/UUID.h"
#include "RenderPassData.h"

namespace Carrot {
    class Window;
    class BufferAllocation;
}

namespace Carrot::Render {
    class Graph: public SwapchainAware {
    public:
        explicit Graph(VulkanDriver& driver);

        /**
         * Setups graph for this frame
         */
        void onFrame(const Render::Context& data);

        /**
         * Actually performs rendering (by recording commands)
         */
        void execute(const Render::Context& data, vk::CommandBuffer& cmds);

    public:
        [[nodiscard]] Render::Texture& createTexture(const FrameResource& resource, size_t frameIndex, const vk::Extent2D& viewportSize);
        [[nodiscard]] Render::Texture& getTexture(const Carrot::UUID& resourceID, size_t frameIndex) const;
        [[nodiscard]] Render::Texture& getTexture(const FrameResource& resource, size_t frameIndex) const;
        [[nodiscard]] Render::Texture& getSwapchainTexture(const FrameResource& resource, size_t frameIndex) const;
        [[nodiscard]] Render::Texture& getOrCreateTexture(const FrameResource& resource, size_t frameIndex, const vk::Extent2D& viewportSize);

        [[nodiscard]] BufferAllocation& createBuffer(const FrameResource& resource, size_t frameIndex);
        [[nodiscard]] BufferAllocation& getBuffer(const Carrot::UUID& resourceID, size_t frameIndex) const;
        [[nodiscard]] BufferAllocation& getBuffer(const FrameResource& resource, size_t frameIndex) const;
        [[nodiscard]] BufferAllocation& getOrCreateBuffer(const FrameResource& resource, size_t frameIndex);

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

        void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override;

    private:
        // Draws a source resource to the destination texture
        void drawViewer(const Render::Context& context, const Render::FrameResource& sourceResource, std::unique_ptr<Texture>& destinationTexture, vk::CommandBuffer cmds);

        void drawPassNodes(const Render::Context& context, Render::CompiledPass* pass, std::uint32_t passIndex);
        void debugDraw(const Render::Context& context);
        void drawResource(const Render::Context& context);

        Carrot::VulkanDriver& driver;
        std::list<std::pair<std::string, std::unique_ptr<Render::CompiledPass>>> passes;
        std::vector<Render::CompiledPass*> sortedPasses;
        std::list<std::pair<std::string, std::any>> passesData;

        // for imgui debug
        void* nodesContext = nullptr;
        const FrameResource* hoveredResourceForMain = nullptr;
        const FrameResource* clickedResourceForMain = nullptr;
        const FrameResource* hoveredResourceForRender = nullptr;
        const FrameResource* clickedResourceForRender = nullptr;

        std::unique_ptr<Carrot::Render::Texture> hoveredResourceViewer;
        std::unique_ptr<Carrot::Render::Texture> clickedResourceViewer;

        friend class GraphBuilder;
    };

    class GraphBuilder {
        template<typename Data>
        using SetupPassCallback = std::function<void(GraphBuilder&, Pass<Data>&, Data&)>;

    public:
        explicit GraphBuilder(Carrot::VulkanDriver& driver, Window& window);
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
        FrameResource& createRenderTarget(std::string name, vk::Format format, TextureSize size, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal);
        FrameResource& createTarget(bool isStorageImage, std::string name, vk::Format format, TextureSize size, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal);

        /// Creates a buffer for this pass. The buffer is guaranteed to always be filled with 0 on the first frame.
        /// @param name Name of the buffer (for debug)
        /// @param size Size in bytes of the buffer
        /// @param clearEachFrame Should clear the buffer to 0 each frame?
        /// @return resource handle to the buffer
        FrameResource& createBuffer(std::string name, vk::DeviceSize size, vk::BufferUsageFlags usages, bool clearEachFrame);
        FrameResource& createStorageTarget(std::string name, vk::Format format, TextureSize size, vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal);
        void present(FrameResource& toPresent);

        void reuseBufferAcrossFrames(const FrameResource& toReuse, std::size_t historyLength);

        template<typename Type>
        std::optional<Type> getPassData(std::string_view passName) const {
            for(const auto& [name, passData] : passesData) {
                if(name == passName)
                    return std::any_cast<Type>(passData);
            }
            return {};
        }

    private:
        Window& window;
        FrameResource swapchainImage;
        std::list<FrameResource> resources;
        std::set<Carrot::UUID> toPresent;
        std::list<std::pair<std::string, std::shared_ptr<Render::PassBase>>> passes;
        std::list<std::pair<std::string, std::any>> passesData;
        Render::PassBase* currentPass = nullptr;
    };

}
