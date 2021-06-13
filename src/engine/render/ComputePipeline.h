//
// Created by jglrxavpok on 29/04/2021.
//

#pragma once

#include "engine/Engine.h"
#include "engine/render/resources/BufferView.h"
#include <string>
#include "engine/io/Resource.h"

namespace Carrot {
    struct ComputeBinding {
        vk::DescriptorType type = vk::DescriptorType::eStorageBufferDynamic;
        uint32_t setID = 0;
        uint32_t bindingID = 0;
        uint32_t count = 1;

        shared_ptr<vk::DescriptorBufferInfo> bufferInfo = nullptr;
    };

    class ComputePipeline: public SwapchainAware {
    private:
        constexpr static uint64_t UncompleteValue = 0;
        constexpr static uint64_t CompleteValue = 1;

        Carrot::Engine& engine;
        vk::CommandBuffer commandBuffer{};
        vk::UniqueDescriptorPool descriptorPool{};
        std::map<uint32_t, vk::UniqueDescriptorSetLayout> descriptorLayouts{};
        std::map<uint32_t, vk::DescriptorSet> descriptorSets{};
        vk::UniquePipelineLayout computePipelineLayout{};
        vk::UniquePipeline computePipeline{};
        vk::UniqueFence finishedFence{};
        mutable BufferView dispatchBuffer;
        mutable vk::DispatchIndirectCommand* dispatchSizes = nullptr;

    public:
        explicit ComputePipeline(Carrot::Engine& engine, const IO::Resource shaderResource, const std::map<uint32_t, vector<ComputeBinding>>& bindings);

        void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) const;

        void waitForCompletion();

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;
    };

    class ComputePipelineBuilder {
    private:
        Carrot::Engine& engine;
        IO::Resource shaderResource;
        vector<ComputeBinding> bindings;

    public:
        explicit ComputePipelineBuilder(Carrot::Engine& engine);

        ComputePipelineBuilder& shader(IO::Resource shader) { shaderResource = shader; return *this; };
        ComputePipelineBuilder& bufferBinding(vk::DescriptorType type, uint32_t setID, uint32_t bindingID, const vk::DescriptorBufferInfo info, uint32_t count = 1);

        std::unique_ptr<ComputePipeline> build();
    };

}
