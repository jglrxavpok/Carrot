//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once

#include "engine/vulkan/includes.h"
#include "engine/vulkan/VulkanDriver.h"
#include "engine/render/FrameData.h"
#include "engine/render/resources/Texture.h"
#include "engine/utils/UUID.h"

namespace Carrot::Render {

    class FrameResource;

    class CompiledPass;
    class Graph;

    using CompiledPassCallback = std::function<void(const CompiledPass&, const FrameData&, vk::CommandBuffer&)>;

    struct ImageTransition {
        Carrot::UUID resourceID;
        vk::ImageLayout from;
        vk::ImageLayout to;
        vk::ImageAspectFlags aspect;

        explicit ImageTransition(Carrot::UUID resourceID, vk::ImageLayout from, vk::ImageLayout to, vk::ImageAspectFlags aspect): resourceID(resourceID), from(from), to(to), aspect(aspect) {}
    };

    class CompiledPass {
    public:
        explicit CompiledPass(
                Carrot::VulkanDriver& driver,
                Graph& graph,
                std::vector<vk::UniqueFramebuffer>&& framebuffers,
                vk::UniqueRenderPass&& renderPass,
                const std::vector<vk::ClearValue>& clearValues,
                const CompiledPassCallback& renderingCode,
                std::vector<ImageTransition>&& prePassTransitions
                );

    public:
        void execute(const FrameData& data, vk::CommandBuffer& cmds) const;

        const vk::RenderPass& getRenderPass() const { return *renderPass; }

        Graph& getGraph() { return graph; }

        const Graph& getGraph() const { return graph; }

    private:
        Carrot::VulkanDriver& driver;
        Graph& graph;
        std::vector<vk::UniqueFramebuffer> framebuffers;
        vk::UniqueRenderPass renderPass;
        std::vector<vk::ClearValue> clearValues;
        std::vector<ImageTransition> prePassTransitions;
        CompiledPassCallback renderingCode;
    };

    class PassBase {
    public:
        virtual ~PassBase() = default;

        virtual CompiledPassCallback generateCallback() = 0;

    public:
        void addInput(const FrameResource& resource, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect);
        void addOutput(const FrameResource& resource, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue, vk::ImageAspectFlags aspect, vk::ImageLayout layout);
        void present(const FrameResource& toPresent);

    public:
        std::unique_ptr<CompiledPass> compile(Carrot::VulkanDriver& driver, Graph& graph);

    protected:
        struct Input {
            const FrameResource* resource = nullptr;
            vk::ImageLayout expectedLayout = vk::ImageLayout::eUndefined;
            vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;

            Input(const FrameResource* resource, vk::ImageLayout expectedLayout, vk::ImageAspectFlags aspect): resource(resource), expectedLayout(expectedLayout), aspect(aspect) {}
        };

        struct Output {
            const FrameResource* resource = nullptr;
            vk::AttachmentLoadOp loadOp;
            vk::ClearValue clearValue;
            vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor;

            Output(const FrameResource* resource, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue, vk::ImageAspectFlags aspect): resource(resource), loadOp(loadOp), clearValue(clearValue), aspect(aspect) {}
        };

        std::list<Input> inputs;
        std::list<Output> outputs;
        std::unordered_map<Carrot::UUID, vk::ImageLayout> finalLayouts;

        friend class GraphBuilder;
    };

    template<typename Data>
    using ExecutePassCallback = std::function<void(const CompiledPass&, const FrameData&, const Data&, vk::CommandBuffer&)>;

    template<typename Data>
    class Pass: public PassBase {
    public:
        explicit Pass(const ExecutePassCallback<Data>& callback): executeCallback(callback) {};

        CompiledPassCallback generateCallback() override {
            return [executeCallback = executeCallback, data = data](const CompiledPass& pass, const FrameData& frameData, vk::CommandBuffer& cmds)
            {
                executeCallback(pass, frameData, data, cmds);
            };
        }

        const Data& getData() const { return data; }

    private:
        ExecutePassCallback<Data> executeCallback;
        Data data;

        friend class GraphBuilder;
    };

}
