//
// Created by jglrxavpok on 20/06/2021.
//

#pragma once

#include "engine/vulkan/includes.h"
#include "engine/vulkan/VulkanDriver.h"
#include "engine/render/FrameData.h"
#include "engine/render/resources/Texture.h"

namespace Carrot::Render {

    class FrameResource;

    class CompiledPass;
    class Graph;

    using CompiledPassCallback = std::function<void(const CompiledPass&, const FrameData&, vk::CommandBuffer&)>;

    class CompiledPass {
    public:
        explicit CompiledPass(
                Carrot::VulkanDriver& driver,
                std::vector<vk::UniqueFramebuffer>&& framebuffers,
                vk::UniqueRenderPass&& renderPass,
                const std::vector<vk::ClearValue>& clearValues,
                const CompiledPassCallback& renderingCode
                );

    public:
        void execute(const FrameData& data, vk::CommandBuffer& cmds) const;

        const vk::RenderPass& getRenderPass() const { return *renderPass; }

    private:
        Carrot::VulkanDriver& driver;
        std::vector<vk::UniqueFramebuffer> framebuffers;
        vk::UniqueRenderPass renderPass;

        std::vector<vk::ClearValue> clearValues;

        CompiledPassCallback renderingCode;
    };

    class PassBase {
    public:
        virtual ~PassBase() = default;

        virtual CompiledPassCallback generateCallback() = 0;

    public:
        void addInput(const FrameResource& resource);
        void addOutput(const FrameResource& resource, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue);

    public:
        std::unique_ptr<CompiledPass> compile(Carrot::VulkanDriver& driver, Graph& graph);

    protected:
        struct Input {
            const FrameResource* resource = nullptr;

            Input(const FrameResource* resource): resource(resource) {}
        };

        struct Output {
            const FrameResource* resource = nullptr;
            vk::AttachmentLoadOp loadOp;
            vk::ClearValue clearValue;

            Output(const FrameResource* resource, vk::AttachmentLoadOp loadOp, vk::ClearValue clearValue): resource(resource), loadOp(loadOp), clearValue(clearValue) {}
        };

        std::list<Input> inputs;
        std::list<Output> outputs;
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

    private:
        ExecutePassCallback<Data> executeCallback;
        Data data;
    };

}
