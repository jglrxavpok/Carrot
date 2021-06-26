//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once
#include "engine/vulkan/VulkanDriver.h"
#include "RenderPass.h"
#include "FrameData.h"
#include <list>

namespace Carrot::Render {
    class FrameResource {
    public:
        FrameResource(): parent(nullptr) {};
        explicit FrameResource(const FrameResource* parent): parent(parent) {
            if(parent != nullptr) {
                width = parent->width;
                height = parent->height;
                depth = parent->depth;
                format = parent->format;
            }
        };
        // TODO

        const FrameResource* parent = nullptr;
        std::uint32_t width = -1;
        std::uint32_t height = -1;
        std::uint32_t depth = 1;
        vk::Format format = vk::Format::eUndefined;
    };

    class Graph {
    public:
        explicit Graph();

        void execute(const FrameData& data, vk::CommandBuffer& cmds);

    public:
        Render::CompiledPass& getPass(const std::string& name);

        Render::Texture& getOrCreateTexture(VulkanDriver& driver, const FrameResource& resource, size_t frameIndex);

    private:
        std::unordered_map<std::string, std::unique_ptr<Render::CompiledPass>> passes;
        std::vector<Render::CompiledPass*> sortedPasses;
        std::vector<std::vector<Carrot::Render::Texture::Ref>> textures;

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
            Data data{};
            passes[name] = std::move(std::make_unique<Render::Pass<Data>>(execute));
            currentPass = passes[name].get();
            auto* pass = static_cast<Render::Pass<Data>*>(currentPass);
            setup(*this, *pass, data);

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
        FrameResource& read(const FrameResource& toRead);
        FrameResource& write(const FrameResource& toWrite, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f}));

    private:
        Carrot::VulkanDriver& driver;
        FrameResource swapchainImage;
        std::list<FrameResource> resources;
        std::unordered_map<std::string, std::unique_ptr<Render::PassBase>> passes;
        Render::PassBase* currentPass = nullptr;
    };
}
