//
// Created by jglrxavpok on 29/04/2021.
//

#include "ComputePipeline.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/utils/Macros.h"

Carrot::ComputePipelineBuilder::ComputePipelineBuilder(Carrot::Engine& engine): engine(engine) {

}

Carrot::ComputePipelineBuilder& Carrot::ComputePipelineBuilder::bufferBinding(vk::DescriptorType type, std::uint32_t setID, std::uint32_t bindingID, const vk::DescriptorBufferInfo& info, std::uint32_t count) {
    ComputeBinding binding{};
    binding.type = type;
    binding.count = count;
    binding.setID = setID;
    binding.bindingID = bindingID;
    binding.bufferInfo = std::make_unique<vk::DescriptorBufferInfo>();
    *binding.bufferInfo = info;
    bindings[{setID, bindingID}] = std::move(binding);
    return *this;
}

Carrot::ComputePipelineBuilder& Carrot::ComputePipelineBuilder::specializationUInt32(std::uint32_t constantID, std::uint32_t value) {
    specializationConstants[constantID] = value;
    return *this;
}

std::unique_ptr<Carrot::ComputePipeline> Carrot::ComputePipelineBuilder::build() {
    std::map<uint32_t, std::vector<ComputeBinding>> bindingsPerSet{};
    for(const auto& [setAndBindingIndex, b] : bindings) {
        bindingsPerSet[setAndBindingIndex.setID].push_back(b);
    }
    return std::make_unique<ComputePipeline>(engine, shaderResource, specializationConstants, bindingsPerSet);
}

Carrot::ComputePipeline::ComputePipeline(Carrot::Engine& engine, const IO::Resource shaderResource,
                                         const std::unordered_map<std::uint32_t, SpecializationConstant>& specializationConstants,
                                         const std::map<uint32_t, std::vector<ComputeBinding>>& bindings):
        engine(engine), dispatchBuffer(engine.getResourceAllocator().allocateBuffer(sizeof(vk::DispatchIndirectCommand),
                                                                                    vk::BufferUsageFlagBits::eIndirectBuffer,
                                                                                    vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent)) {
    auto& computeCommandPool = engine.getComputeCommandPool();

    // command buffers which will be sent to the compute queue
    commandBuffer = engine.getLogicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = computeCommandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    })[0];

    // TODO: support other types than uint32
    auto specialization = vk::SpecializationInfo{};

    std::vector<std::uint8_t> specializationData;
    std::vector<vk::SpecializationMapEntry> entries;
    specializationData.resize(specializationConstants.size() * sizeof(SpecializationConstant));
    entries.reserve(specializationConstants.size());

    std::size_t index = 0;
    for(const auto& [constantID, constant] : specializationConstants) {
        SpecializationConstant* ptr = &reinterpret_cast<SpecializationConstant*>(specializationData.data())[index];
        *ptr = constant;

        entries.emplace_back(vk::SpecializationMapEntry {
           .constantID = constantID,
           .offset = static_cast<uint32_t>(index * sizeof(SpecializationConstant)),
           .size = sizeof(SpecializationConstant),
        });
        index++;
    }
    specialization.pData = specializationData.data();
    specialization.dataSize = specializationData.size();
    specialization.mapEntryCount = entries.size();
    specialization.pMapEntries = entries.data();

    std::uint32_t totalBindingCount = 0;
    std::uint32_t dynamicBindingCount = 0;
    std::uint32_t storageBufferCount = 0;
    std::uint32_t storageBufferDynamicCount = 0;

    // descriptor layouts used by pipeline
    descriptorLayouts.clear();
    for (const auto& [setID, bindingList] : bindings) {
        std::vector<vk::DescriptorSetLayoutBinding> layoutBindings{};
        for(const auto& binding : bindingList) {
            switch (binding.type) {
                case vk::DescriptorType::eStorageBufferDynamic:
                    dynamicBindingCount++;
                    storageBufferDynamicCount++;
                    break;

                case vk::DescriptorType::eStorageBuffer:
                    storageBufferCount++;
                    break;

                default:
                    throw std::runtime_error("Unsupported binding type: "+std::to_string(static_cast<uint32_t>(binding.type)));
            }

            layoutBindings.push_back(vk::DescriptorSetLayoutBinding {
                    .binding = binding.bindingID,
                    .descriptorType = binding.type,
                    .descriptorCount = binding.count,
                    .stageFlags = vk::ShaderStageFlagBits::eCompute,
            });

            totalBindingCount++;
        }


        descriptorLayouts[setID] = std::move(engine.getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
            .bindingCount = static_cast<uint32_t>(layoutBindings.size()),
            .pBindings = layoutBindings.data()
        }));
    }

    dynamicOffsets.resize(dynamicBindingCount, 0);

    std::vector<vk::DescriptorPoolSize> descriptorSizes{};
    if(storageBufferCount != 0) {
        descriptorSizes.emplace_back(vk::DescriptorPoolSize {
                .type = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = storageBufferCount,
        });
    }

    if(storageBufferDynamicCount != 0) {
        descriptorSizes.emplace_back(vk::DescriptorPoolSize {
                .type = vk::DescriptorType::eStorageBufferDynamic,
                .descriptorCount = storageBufferDynamicCount,
        });
    }

    descriptorSets.clear();
    descriptorPool = std::move(engine.getLogicalDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
            .maxSets = engine.getSwapchainImageCount(),
            .poolSizeCount = static_cast<uint32_t>(descriptorSizes.size()),
            .pPoolSizes = descriptorSizes.data(),
    }));

    descriptorSets.clear();
    descriptorSetsFlat.clear();
    for(const auto& [setID, layout] : descriptorLayouts) {
        descriptorSets[setID] = engine.getLogicalDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *descriptorPool,
                .descriptorSetCount = 1,
                .pSetLayouts = &(*layout)
        })[0];

        descriptorSetsFlat.push_back(descriptorSets[setID]);
    }

    std::vector<vk::WriteDescriptorSet> writes{totalBindingCount};
    std::vector<vk::DescriptorBufferInfo> bufferInfo{storageBufferCount+storageBufferDynamicCount};

    std::uint32_t bufferIndex = 0;
    std::uint32_t writeIndex = 0;
    for (const auto& [setID, bindingList] : bindings) {
        for(const auto& binding : bindingList) {
            switch (binding.type) {
                case vk::DescriptorType::eStorageBufferDynamic:
                case vk::DescriptorType::eStorageBuffer:
                case vk::DescriptorType::eUniformBufferDynamic:
                case vk::DescriptorType::eUniformBuffer: {
                    auto& buf = bufferInfo[bufferIndex++];
                    buf = *binding.bufferInfo;
                    // only count = 1 is supported yet
                    //  would need additional allocation
                    assert(binding.count == 1);
                    writes[writeIndex++] = vk::WriteDescriptorSet {
                            .dstSet = descriptorSets[setID],
                            .dstBinding = binding.bindingID,
                            .descriptorCount = binding.count,
                            .descriptorType = binding.type,
                            .pBufferInfo = &buf,
                    };
                } break;

                default:
                    throw std::runtime_error("Unsupported binding type: "+std::to_string(static_cast<uint32_t>(binding.type)));
            }
        }
    }
    engine.getLogicalDevice().updateDescriptorSets(writes, {});

    auto computeStage = ShaderModule(engine.getVulkanDriver(), shaderResource);

    // create the pipeline
    std::vector<vk::DescriptorSetLayout> setLayouts{};
    for(const auto& [setID, layout] : descriptorLayouts) {
        setLayouts.push_back(*layout);
    }

    computePipelineLayout = engine.getLogicalDevice().createPipelineLayoutUnique(vk::PipelineLayoutCreateInfo {
            .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
            .pSetLayouts = setLayouts.data(),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
    }, engine.getAllocator());

    computePipeline = engine.getLogicalDevice().createComputePipelineUnique(nullptr, vk::ComputePipelineCreateInfo {
            .stage = computeStage.createPipelineShaderStage(vk::ShaderStageFlagBits::eCompute, &specialization),
            .layout = *computePipelineLayout,
    }, engine.getAllocator()).value;

    finishedFence = engine.getLogicalDevice().createFenceUnique(vk::FenceCreateInfo {
        .flags = vk::FenceCreateFlagBits::eSignaled
    });

    dispatchSizes = dispatchBuffer.map<vk::DispatchIndirectCommand>();

    {
        commandBuffer.begin(vk::CommandBufferBeginInfo {});

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *computePipelineLayout, 0, descriptorSetsFlat, dynamicOffsets);
        commandBuffer.dispatchIndirect(dispatchBuffer.getVulkanBuffer(), dispatchBuffer.getStart());
        commandBuffer.end();
    }
}

void Carrot::ComputePipeline::waitForCompletion() {
    DISCARD(engine.getLogicalDevice().waitForFences(*finishedFence, true, UINT64_MAX));
}

void Carrot::ComputePipeline::onSwapchainImageCountChange(size_t newCount) {
    SwapchainAware::onSwapchainImageCountChange(newCount);
}

void Carrot::ComputePipeline::onSwapchainSizeChange(int newWidth, int newHeight) {}

void Carrot::ComputePipeline::dispatchInline(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, vk::CommandBuffer& commandBuffer) const {
    dispatchSizes->x = groupCountX;
    dispatchSizes->y = groupCountY;
    dispatchSizes->z = groupCountZ;

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, *computePipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, *computePipelineLayout, 0, descriptorSetsFlat, dynamicOffsets);
    commandBuffer.dispatchIndirect(dispatchBuffer.getVulkanBuffer(), dispatchBuffer.getStart());
}

void Carrot::ComputePipeline::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ, vk::Semaphore* signal) const {
    dispatchSizes->x = groupCountX;
    dispatchSizes->y = groupCountY;
    dispatchSizes->z = groupCountZ;

    engine.getLogicalDevice().resetFences(*finishedFence);
    GetVulkanDriver().submitCompute(vk::SubmitInfo {
            .waitSemaphoreCount = 0,
            .pWaitSemaphores = nullptr,
            .pWaitDstStageMask = nullptr,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = signal != nullptr ? 1u : 0u,
            .pSignalSemaphores = signal,
    }, *finishedFence);
}
