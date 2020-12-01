//
// Created by jglrxavpok on 01/12/2020.
//

#include <constants.h>
#include "Pipeline.h"
#include "ShaderStages.h"
#include "Vertex.h"

Carrot::Pipeline::Pipeline(Carrot::Engine& engine, vk::UniqueRenderPass& renderPass, const string& shaderName): engine(engine), renderPass(renderPass) {
    auto& device = engine.getLogicalDevice();
    const auto stages = Carrot::ShaderStages(engine,
                                             { "resources/shaders/"+shaderName+".vertex.glsl.spv",
                                               "resources/shaders/"+shaderName+".fragment.glsl.spv"
                                             });

    descriptorSetLayout = stages.createDescriptorSetLayout();
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

    auto shaderStageCreation = stages.createPipelineShaderStages();
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
