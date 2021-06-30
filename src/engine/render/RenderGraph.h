//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once
#include "engine/vulkan/VulkanDriver.h"
#include "RenderPass.h"
#include "FrameData.h"
#include <list>
#include "engine/utils/UUID.h"

namespace Carrot::Render {
    class FrameResource: public std::enable_shared_from_this<FrameResource> {
    public:
        FrameResource() {
            rootID = id;
            parentID = id;
        }

        FrameResource(const FrameResource& toCopy) {
            rootID = toCopy.rootID;
            parentID = toCopy.parentID;
            id = toCopy.id;
            width = toCopy.width;
            height = toCopy.height;
            depth = toCopy.depth;
            format = toCopy.format;
            isSwapchain = toCopy.isSwapchain;
        }

        explicit FrameResource(const FrameResource* parent) {
            width = parent->width;
            height = parent->height;
            depth = parent->depth;
            format = parent->format;
            isSwapchain = parent->isSwapchain;
            rootID = parent->rootID;
            parentID = parent->id;
        };

        std::uint32_t width = -1;
        std::uint32_t height = -1;
        std::uint32_t depth = 1;
        vk::Format format = vk::Format::eUndefined;
        bool isSwapchain = false;
        Carrot::UUID id = Carrot::randomUUID();
        Carrot::UUID rootID = Carrot::randomUUID();
        Carrot::UUID parentID = Carrot::randomUUID();
    };

    class Graph {
    public:
        explicit Graph();

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
        Pass<Data>& addPass(const std::string& name, const SetupPassCallback<Data>& setup, const ExecutePassCallback<Data>& execute) {
            passes[name] = std::move(std::make_unique<Render::Pass<Data>>(execute));
            currentPass = passes[name].get();
            auto* pass = static_cast<Render::Pass<Data>*>(currentPass);
            setup(*this, *pass, pass->data);

            currentPass = nullptr;
            return *pass;
        }

    public:
        std::unique_ptr<Graph> compile();

    public:
        FrameResource& getSwapchainImage() {
            return swapchainImage;
        }

    public:
        FrameResource& read(const FrameResource& toRead, vk::ImageLayout expectedLayout);
        FrameResource& write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}));
        FrameResource& createRenderTarget(vk::Format format, vk::Extent3D size, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}));
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
        std::unordered_map<std::string, std::unique_ptr<Render::PassBase>> passes;
        Render::PassBase* currentPass = nullptr;
    };
}
