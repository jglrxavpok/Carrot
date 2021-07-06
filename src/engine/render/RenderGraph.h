//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once
#include "engine/vulkan/VulkanDriver.h"
#include "RenderPass.h"
#include "FrameData.h"
#include <list>
#include "engine/utils/UUID.h"
#include "RenderPassData.h"

namespace Carrot::Render {
    class Graph {
    public:
        explicit Graph(std::unordered_map<Carrot::UUID, vk::ImageUsageFlags> textureUsages);

        void execute(const FrameData& data, vk::CommandBuffer& cmds);

    public:
        Render::CompiledPass& getPass(const std::string& name);

        Render::Texture& getOrCreateTexture(VulkanDriver& driver, const FrameResource& resource, size_t frameIndex);
        Render::Texture& getTexture(const Carrot::UUID& resourceID, size_t frameIndex) const;
        Render::Texture& getTexture(const FrameResource& resource, size_t frameIndex) const;

        std::unordered_map<Carrot::UUID, vk::ImageLayout>& getFinalLayouts() { return finalLayouts; }

    private:
        std::unordered_map<std::string, std::unique_ptr<Render::CompiledPass>> passes;
        std::vector<Render::CompiledPass*> sortedPasses;
        std::vector<std::unordered_map<Carrot::UUID, Carrot::Render::Texture::Ref>> textures;
        std::unordered_map<Carrot::UUID, vk::ImageLayout> finalLayouts;
        std::unordered_map<Carrot::UUID, vk::ImageUsageFlags> textureUsages;

        friend class GraphBuilder;
    };

    // TODO: recompile on swapchain change
    class GraphBuilder {
        template<typename Data>
        using SetupPassCallback = std::function<void(GraphBuilder&, Pass<Data>&, Data&)>;

    public:
        explicit GraphBuilder(Carrot::VulkanDriver& driver);

    public:
        template<typename Data>
        Pass<Data>& addPass(const std::string& name, const SetupPassCallback<Data>& setup, const ExecutePassCallback<Data>& execute,
                            const PostCompileCallback<Data>& postCompile = [](CompiledPass&, Data&){}) {
            auto& pair = passes.emplace_back(name, std::move(std::make_unique<Render::Pass<Data>>(execute, postCompile)));
            currentPass = pair.second.get();
            auto* pass = static_cast<Render::Pass<Data>*>(currentPass);
            setup(*this, *pass, pass->data);

            currentPass = nullptr;
            return *pass;
        }

        std::unique_ptr<Graph> compile();

    public:
        vk::ImageUsageFlags getFrameResourceUsages(const FrameResource& resource) const;

    public:
        FrameResource& getSwapchainImage() {
            return swapchainImage;
        }

    public:
        FrameResource& read(const FrameResource& toRead, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);
        FrameResource& write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ImageAspectFlags aspect);
        FrameResource& write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ImageLayout layout, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor);
        FrameResource& createRenderTarget(vk::Format format, vk::Extent3D size, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}), vk::ImageLayout layout = vk::ImageLayout::eColorAttachmentOptimal);
        void present(const FrameResource& toPresent);

    private:
        struct ResourceToCreate {
            vk::Format format;
            vk::Extent3D size;
        };

        Carrot::VulkanDriver& driver;
        FrameResource swapchainImage;
        std::list<FrameResource> resources;
        std::list<ResourceToCreate> resourcesToCreate;
        std::set<Carrot::UUID> toPresent;
        std::list<std::pair<std::string, std::unique_ptr<Render::PassBase>>> passes;
        std::unordered_map<Carrot::UUID, vk::ImageUsageFlags> textureUsages;
        Render::PassBase* currentPass = nullptr;
    };
}
