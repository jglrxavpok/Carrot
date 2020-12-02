//
// Created by jglrxavpok on 01/12/2020.
//

#include <constants.h>
#include "Pipeline.h"
#include "ShaderStages.h"
#include "Vertex.h"
#include "UniformBufferObject.h"
#include "Buffer.h"

Carrot::Pipeline::Pipeline(Carrot::Engine& engine, vk::UniqueRenderPass& renderPass, const string& shaderName): engine(engine), renderPass(renderPass) {
    auto& device = engine.getLogicalDevice();
    stages = make_unique<Carrot::ShaderStages>(engine,
                                             vector<string> {
        "resources/shaders/"+shaderName+".vertex.glsl.spv",
        "resources/shaders/"+shaderName+".fragment.glsl.spv"
                                             });

    descriptorSetLayout = stages->createDescriptorSetLayout();
    auto bindingDescription = Carrot::Vertex::getBindingDescription();
    auto attributeDescriptions = Carrot::Vertex::getAttributeDescriptions();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,

            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data(),
    };

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = false,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
            // TODO: change for shadow maps
            .depthClampEnable = false,

            .rasterizerDiscardEnable = false,

            .polygonMode = vk::PolygonMode::eFill,

            .cullMode = vk::CullModeFlagBits::eFront,
            .frontFace = vk::FrontFace::eClockwise,

            // TODO: change for shadow maps
            .depthBiasEnable = false,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,

            .lineWidth = 1.0f,
    };

    vk::PipelineMultisampleStateCreateInfo multisampling{
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = false,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = false,
            .alphaToOneEnable = false,
    };
    vk::PipelineDepthStencilStateCreateInfo depthStencil {
            .depthTestEnable = true,
            .depthWriteEnable = true,
            .depthCompareOp = vk::CompareOp::eLessOrEqual,
            .stencilTestEnable = false,
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            // TODO: blending
            .blendEnable = false,

            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable = false,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
    };

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .setLayoutCount = 1,
            .pSetLayouts = &(*descriptorSetLayout),
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr,
    };

    layout = device.createPipelineLayoutUnique(pipelineLayoutCreateInfo, engine.getAllocator());

    vk::DynamicState dynamicStates[] = {
            vk::DynamicState::eScissor,
            vk::DynamicState::eViewport,
    };
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo {
            .dynamicStateCount = 2,
            .pDynamicStates = dynamicStates,
    };

    vk::Viewport viewport {
        .x = 0.0f,
        .y = 0.0f,
        .width = WINDOW_WIDTH,
        .height = WINDOW_HEIGHT,

        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    vk::Rect2D scissor {
        .offset = {0,0},
        .extent = {
                .width = WINDOW_WIDTH,
                .height = WINDOW_HEIGHT,
        }
    };

    vk::PipelineViewportStateCreateInfo viewportState {
        .viewportCount = 1,
        .pViewports = &viewport,

        .scissorCount = 1,
        .pScissors = &scissor,
    };

    auto shaderStageCreation = stages->createPipelineShaderStages();
    vk::GraphicsPipelineCreateInfo pipelineInfo{
            .stageCount = static_cast<uint32_t>(shaderStageCreation.size()),
            .pStages = shaderStageCreation.data(),

            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicStateInfo,

            .layout = *layout,
            .renderPass = *renderPass,
            .subpass = 0,
    };

    vkPipeline = device.createGraphicsPipelineUnique(nullptr, pipelineInfo, engine.getAllocator()).value;

    recreateDescriptorPool(engine.getSwapchainImageCount());
}

void Carrot::Pipeline::bind(vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint) const {
    commands.bindPipeline(bindPoint, *vkPipeline);
}

const vk::PipelineLayout& Carrot::Pipeline::getPipelineLayout() const {
    return *layout;
}

const vk::DescriptorSetLayout& Carrot::Pipeline::getDescriptorSetLayout() const {
    return *descriptorSetLayout;
}

void Carrot::Pipeline::bindDescriptorSets(vk::CommandBuffer& commands,
                                          const vector<vk::DescriptorSet>& descriptors,
                                          const vector<uint32_t>& dynamicOffsets) const {
    commands.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *layout, 0, descriptors, dynamicOffsets);
}

void Carrot::Pipeline::recreateDescriptorPool(uint32_t imageCount) {
    vector<vk::DescriptorPoolSize> sizes{};

    vector<vk::DescriptorSetLayoutBinding> bindings{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addBindings(stage, bindings);
    }

    for(const auto& binding : bindings) {
        sizes.emplace_back(vk::DescriptorPoolSize {
            .type = binding.descriptorType,
            .descriptorCount = imageCount*binding.descriptorCount,
        });
    }

    vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = imageCount,
            .poolSizeCount = static_cast<uint32_t>(sizes.size()),
            .pPoolSizes = sizes.data(),
    };

    descriptorPool = engine.getLogicalDevice().createDescriptorPoolUnique(poolInfo, engine.getAllocator());

    descriptorSets = allocateDescriptorSets();
}

vector<vk::DescriptorSet> Carrot::Pipeline::allocateDescriptorSets() {
    vector<vk::DescriptorSetLayout> layouts{engine.getSwapchainImageCount(), *descriptorSetLayout};
    vk::DescriptorSetAllocateInfo allocateInfo {
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };
    vector<vk::DescriptorSet> sets = engine.getLogicalDevice().allocateDescriptorSets(allocateInfo);

    vector<vk::DescriptorSetLayoutBinding> bindings{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addBindings(stage, bindings);
    }

    // allocate default values
    vector<vk::WriteDescriptorSet> writes{bindings.size() * engine.getSwapchainImageCount()};
    vector<vk::DescriptorBufferInfo> buffers{bindings.size() * engine.getSwapchainImageCount()};
    vector<vk::DescriptorImageInfo> samplers{bindings.size() * engine.getSwapchainImageCount()};
    vector<vk::DescriptorImageInfo> images{bindings.size() * engine.getSwapchainImageCount()};

    uint32_t writeIndex = 0;
    for(const auto& binding : bindings) {
        for(size_t i = 0; i < engine.getSwapchainImageCount(); i++) {
            auto& write = writes[writeIndex];
            write.dstBinding = binding.binding;
            write.descriptorCount = binding.descriptorCount;
            write.descriptorType = binding.descriptorType;
            write.dstSet = sets[i];

            switch (binding.descriptorType) {
                case vk::DescriptorType::eUniformBufferDynamic: {
                    auto& buffer = buffers[writeIndex];
                    buffer.buffer = engine.getUniformBuffers()[i]->getVulkanBuffer();
                    buffer.offset = 0;
                    buffer.range = sizeof(UniformBufferObject); // TODO: customizable
                    write.pBufferInfo = buffers.data()+writeIndex;

                    writeIndex++;
                }
                break;

                case vk::DescriptorType::eSampler: {
                    auto& sampler = samplers[writeIndex];
                    sampler.sampler = *engine.getLinearSampler();
                    write.pImageInfo = samplers.data()+writeIndex;

                    writeIndex++;
                }
                break;

                // TODO: write default textures
/*                case vk::DescriptorType::eSampledImage: {
                    auto& image = images[writeIndex];
                    image.imageView = *engine.getDefaultImageView();
                    image.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                    write.pImageInfo = images.data()+writeIndex;

                    writeIndex++;
                }
                break;*/
            }
        }
    }

    engine.getLogicalDevice().updateDescriptorSets(writeIndex, writes.data(), 0, nullptr);
    return sets;
}

const vector<vk::DescriptorSet>& Carrot::Pipeline::getDescriptorSets() const {
    return descriptorSets;
}
