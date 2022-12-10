//
// Created by jglrxavpok on 29/04/2021.
//

#pragma once

#include "engine/Engine.h"
#include "engine/render/resources/BufferView.h"
#include <string>
#include <utility>
#include "core/io/Resource.h"

namespace Carrot {
    struct ComputeBinding {
        vk::DescriptorType type = vk::DescriptorType::eStorageBufferDynamic;
        std::uint32_t setID = 0xFFFFFFFFu;
        std::uint32_t bindingID = 0xFFFFFFFFu;
        uint32_t count = 1;

        std::shared_ptr<vk::DescriptorBufferInfo> bufferInfo = nullptr;
    };

    struct SetAndBindingKey {
        std::uint32_t setID = 0xFFFFFFFFu;
        std::uint32_t bindingID = 0xFFFFFFFFu;

        bool operator==(const SetAndBindingKey& o) const {
            return setID == o.setID && bindingID == o.bindingID;
        }
    };
}

namespace std {
    template<>
    struct hash<Carrot::SetAndBindingKey> {
        std::size_t operator()(const Carrot::SetAndBindingKey& o) const {
            std::size_t result = o.setID;
            result *= 31;
            result += o.bindingID;
            return result;
        }
    };
}

namespace Carrot {
    using SpecializationConstant = std::uint32_t; // does not support anything else for now

    class ComputePipeline: public SwapchainAware {
    public:
        explicit ComputePipeline(Carrot::Engine& engine, const IO::Resource shaderResource, const std::unordered_map<std::uint32_t, SpecializationConstant>& specializationConstants, const std::map<std::uint32_t, std::vector<ComputeBinding>>& bindings);

        void dispatchInline(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, vk::CommandBuffer& cmds) const;
        //! Launches the compute pipeline with the given group size. A signal semaphore can optionally be added, and will be signaled once processing is finished
        void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, vk::Semaphore* signal = nullptr) const;

        void waitForCompletion();

        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        constexpr static uint64_t UncompleteValue = 0;
        constexpr static uint64_t CompleteValue = 1;

        Carrot::Engine& engine;
        vk::CommandBuffer commandBuffer{};
        vk::UniqueDescriptorPool descriptorPool{};
        std::map<std::uint32_t, vk::UniqueDescriptorSetLayout> descriptorLayouts{};
        std::map<std::uint32_t, vk::DescriptorSet> descriptorSets{};
        vk::UniquePipelineLayout computePipelineLayout{};
        vk::UniquePipeline computePipeline{};
        vk::UniqueFence finishedFence{};

        std::vector<vk::DescriptorSet> descriptorSetsFlat;
        std::vector<uint32_t> dynamicOffsets;

        mutable BufferView dispatchBuffer;
        mutable vk::DispatchIndirectCommand* dispatchSizes = nullptr;
    };

    class ComputePipelineBuilder {
    public:
        explicit ComputePipelineBuilder(Carrot::Engine& engine);

        ComputePipelineBuilder& shader(IO::Resource shader) { shaderResource = std::move(shader); return *this; };
        ComputePipelineBuilder& bufferBinding(vk::DescriptorType type, uint32_t setID, uint32_t bindingID, const vk::DescriptorBufferInfo& info, uint32_t count = 1);
        ComputePipelineBuilder& specializationUInt32(std::uint32_t constantID, std::uint32_t value);

        std::unique_ptr<ComputePipeline> build();

    private:
        Carrot::Engine& engine;
        IO::Resource shaderResource;
        std::unordered_map<SetAndBindingKey, ComputeBinding> bindings;
        std::unordered_map<std::uint32_t, SpecializationConstant> specializationConstants;
    };

}
