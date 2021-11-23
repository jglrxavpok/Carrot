//
// Created by jglrxavpok on 01/12/2020.
//

#include <engine/constants.h>
#include "Pipeline.h"
#include "engine/render/CameraBufferObject.h"
#include "engine/render/resources/Buffer.h"
#include <rapidjson/document.h>
#include "engine/io/IO.h"
#include "engine/render/DrawData.h"
#include "Vertex.h"
#include "engine/render/DebugBufferObject.h"
#include "engine/render/MaterialSystem.h"
#include "Mesh.h"
#include "Mesh.ipp"
#include <engine/io/Logging.hpp>

Carrot::Pipeline::Pipeline(Carrot::VulkanDriver& driver, const Carrot::IO::Resource pipelineDescription):
    Carrot::Pipeline::Pipeline(driver, PipelineDescription(pipelineDescription)) {

}

Carrot::Pipeline::Pipeline(Carrot::VulkanDriver& driver, const PipelineDescription description): driver(driver), description(description) {
    auto& device = driver.getLogicalDevice();
    stages = make_unique<Carrot::ShaderStages>(driver,
                                                std::vector<Carrot::IO::Resource> {
                                                    description.vertexShader,
                                                    description.fragmentShader
                                                },
                                                std::vector<vk::ShaderStageFlagBits> {
                                                    vk::ShaderStageFlagBits::eVertex,
                                                    vk::ShaderStageFlagBits::eFragment
                                                }
    );

    descriptorSetLayout0 = stages->createDescriptorSetLayout0(description.constants);

    pipelineTemplate.vertexBindingDescriptions = Carrot::getBindingDescriptions(description.vertexFormat);
    pipelineTemplate.vertexAttributes = Carrot::getAttributeDescriptions(description.vertexFormat);

    pipelineTemplate.vertexInput = {
            .vertexBindingDescriptionCount = static_cast<std::uint32_t>(pipelineTemplate.vertexBindingDescriptions.size()),
            .pVertexBindingDescriptions = pipelineTemplate.vertexBindingDescriptions.data(),

            .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(pipelineTemplate.vertexAttributes.size()),
            .pVertexAttributeDescriptions = pipelineTemplate.vertexAttributes.data(),
    };

    pipelineTemplate.inputAssembly = {
            .topology = description.topology,
            .primitiveRestartEnable = false,
    };

    pipelineTemplate.rasterizer = {
            // TODO: change for shadow maps
            .depthClampEnable = false,

            .rasterizerDiscardEnable = false,

            .polygonMode = vk::PolygonMode::eFill,

            .cullMode = description.cull ? vk::CullModeFlagBits::eFront : vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eClockwise,

            // TODO: change for shadow maps
            .depthBiasEnable = false,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,

            .lineWidth = 1.0f,
    };

    pipelineTemplate.multisampling = {
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = false,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = false,
            .alphaToOneEnable = false,
    };
    pipelineTemplate.depthStencil = {
            .depthTestEnable = description.depthTest,//description.type != PipelineType::GResolve,
            .depthWriteEnable = description.depthWrite,//description.type != PipelineType::GResolve && description.type != PipelineType::Skybox,
            .depthCompareOp = vk::CompareOp::eLessOrEqual,
            .stencilTestEnable = false,
    };

    if(description.alphaBlending) {
        pipelineTemplate.colorBlendAttachments = {
                vk::PipelineColorBlendAttachmentState {
                        .blendEnable = true,
                        .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
                        .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
                        .colorBlendOp = vk::BlendOp::eAdd,
                        .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                        .dstAlphaBlendFactor = vk::BlendFactor::eOne,
                        .alphaBlendOp = vk::BlendOp::eAdd,

                        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
                }
        };

        if(description.type == PipelineType::Particles || description.type == PipelineType::GBuffer) {
            // R32G32B32 position
            pipelineTemplate.colorBlendAttachments.push_back(vk::PipelineColorBlendAttachmentState {
                    .blendEnable = false,

                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
            });
            // R32G32B32 normals
            pipelineTemplate.colorBlendAttachments.push_back(vk::PipelineColorBlendAttachmentState {
                    .blendEnable = false,

                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
            });

            // R32 not blendable
            pipelineTemplate.colorBlendAttachments.push_back(vk::PipelineColorBlendAttachmentState {
                    .blendEnable = false,

                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
            });

            // R32G32B32A32
            pipelineTemplate.colorBlendAttachments.push_back(vk::PipelineColorBlendAttachmentState {
                    .blendEnable = false,

                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
            });
        }
    } else {
        pipelineTemplate.colorBlendAttachments = {
                static_cast<std::size_t>(description.type == PipelineType::Particles || description.type == PipelineType::GBuffer ? 5 : 1),
                vk::PipelineColorBlendAttachmentState {
                        .blendEnable = false,

                        .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
                }
        };
    }

    pipelineTemplate.colorBlending = vk::PipelineColorBlendStateCreateInfo {
            .logicOpEnable = false,
            .attachmentCount = static_cast<uint32_t>(pipelineTemplate.colorBlendAttachments.size()),
            .pAttachments = pipelineTemplate.colorBlendAttachments.data(),
    };

    std::vector<vk::PushConstantRange> pushConstants{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addPushConstants(stage, pushConstantMap);
    }
    for(const auto& [_, range] : pushConstantMap) {
        pushConstants.push_back(range);
    }

    std::vector<vk::DescriptorSetLayout> layouts{};

    if(!description.tmptmptmpUseMaterialSystem)
        layouts.push_back(*descriptorSetLayout0);
    if(description.vertexFormat == VertexFormat::SkinnedVertex) {
        vk::DescriptorSetLayoutBinding binding {
                .binding = 0,
                .descriptorType = vk::DescriptorType::eStorageBuffer,
                .descriptorCount = 1,
                .stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eRaygenKHR,
        };
        descriptorSetLayout1 = driver.getLogicalDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo{
                .bindingCount = 1,
                .pBindings = &binding,
        }, driver.getAllocationCallbacks());

        layouts.push_back(*descriptorSetLayout1);
    }
    if(description.tmptmptmpUseMaterialSystem || description.vertexFormat == VertexFormat::Vertex || description.type == PipelineType::GResolve) { // FIXME: remove condition
        layouts.push_back(GetRenderer().getMaterialSystem().getDescriptorSetLayout());
    }

    if(description.reserveSet2ForCamera) {
        while(layouts.size() < 2) {
            layouts.push_back(driver.getEmptyDescriptorSetLayout());
        }
        layouts.push_back(driver.getRenderer().getCameraDescriptorSetLayout());
    }

    if(description.type == PipelineType::GResolve) { // FIXME: remove condition
        layouts.push_back(GetRenderer().getLighting().getDescriptorSetLayout());
    }

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = static_cast<std::uint32_t>(pushConstants.size()),
            .pPushConstantRanges = pushConstants.data(),
    };

    layout = device.createPipelineLayoutUnique(pipelineLayoutCreateInfo, driver.getAllocationCallbacks());

    pipelineTemplate.dynamicStateInfo = {
            .dynamicStateCount = 2,
            .pDynamicStates = pipelineTemplate.dynamicStates,
    };

    pipelineTemplate.viewport = vk::Viewport {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(driver.getFinalRenderSize().width),
        .height = static_cast<float>(driver.getFinalRenderSize().height),

        .minDepth = -1.0f,
        .maxDepth = 1.0f,
    };

    pipelineTemplate.scissor = vk::Rect2D {
        .offset = {0,0},
        .extent = {
                .width = driver.getFinalRenderSize().width,
                .height = driver.getFinalRenderSize().height,
        }
    };

    pipelineTemplate.viewportState = vk::PipelineViewportStateCreateInfo {
        .viewportCount = 1,
        .pViewports = &pipelineTemplate.viewport,

        .scissorCount = 1,
        .pScissors = &pipelineTemplate.scissor,
    };

    std::vector<Specialization> specializations{};

    for(const auto& [constantName, constantValue] : description.constants) {
        if("MAX_MATERIALS" == constantName) {
            maxMaterialID = constantValue;
        }
        if("MAX_TEXTURES" == constantName) {
            maxTextureID = constantValue;
        }
        Carrot::Log::info("Specializing constant %s to value %lu", constantName.c_str(), constantValue);
        specializations.push_back(Specialization {
                .offset = static_cast<std::uint32_t>(specializations.size()*sizeof(uint32_t)),
                .size = sizeof(std::uint32_t),
                .constantID = static_cast<std::uint32_t>(specializations.size()), // TODO: Map name to index
        });
        pipelineTemplate.specializationData.push_back(static_cast<std::uint32_t>(constantValue));
    }

    uint32_t totalDataSize = 0;
    std::vector<vk::SpecializationMapEntry> entries{};
    for(const auto& spec : specializations) {
        entries.emplace_back(spec.convertToEntry());
        totalDataSize += spec.size;
    }
    pipelineTemplate.specializationEntries = entries;
    pipelineTemplate.specialization = {
            .mapEntryCount = static_cast<std::uint32_t>(pipelineTemplate.specializationEntries.size()),
            .pMapEntries = pipelineTemplate.specializationEntries.data(),
            .dataSize = totalDataSize,
            .pData = pipelineTemplate.specializationData.data(),
    };

    pipelineTemplate.shaderStageCreation = stages->createPipelineShaderStages(&pipelineTemplate.specialization);
    pipelineTemplate.pipelineInfo = vk::GraphicsPipelineCreateInfo {
            .stageCount = static_cast<std::uint32_t>(pipelineTemplate.shaderStageCreation.size()),
            .pStages = pipelineTemplate.shaderStageCreation.data(),

            .pVertexInputState = &pipelineTemplate.vertexInput,
            .pInputAssemblyState = &pipelineTemplate.inputAssembly,
            .pViewportState = &pipelineTemplate.viewportState,
            .pRasterizationState = &pipelineTemplate.rasterizer,
            .pMultisampleState = &pipelineTemplate.multisampling,
            .pDepthStencilState = &pipelineTemplate.depthStencil,
            .pColorBlendState = &pipelineTemplate.colorBlending,
            .pDynamicState = &pipelineTemplate.dynamicStateInfo,

            .layout = *layout,
            .renderPass = {}, // will be filled in getOrCreatePipelineForRenderPass
            .subpass = static_cast<std::uint32_t>(description.subpassIndex),
    };

    recreateDescriptorPool(driver.getSwapchainImageCount());
}

const vk::PushConstantRange& Carrot::Pipeline::getPushConstant(std::string_view name) const {
    return pushConstantMap[std::string(name)];
}

vk::Pipeline& Carrot::Pipeline::getOrCreatePipelineForRenderPass(vk::RenderPass pass) const {
    auto it = vkPipelines.find(pass);
    if(it == vkPipelines.end()) {
        vk::GraphicsPipelineCreateInfo info = pipelineTemplate.pipelineInfo;
        info.renderPass = pass;
        vkPipelines[pass] = std::move(driver.getLogicalDevice().createGraphicsPipelineUnique(nullptr, info, driver.getAllocationCallbacks()));
    }
    return *vkPipelines[pass];
}

void Carrot::Pipeline::bind(vk::RenderPass pass, Carrot::Render::Context renderContext, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint) const {
    auto& pipeline = getOrCreatePipelineForRenderPass(pass);
    commands.bindPipeline(bindPoint, pipeline);
    if(description.reserveSet2ForCamera && description.vertexFormat != VertexFormat::SkinnedVertex) {
        //bindDescriptorSets(commands, {driver.getEmptyDescriptorSet()}, {}, 1);
        renderContext.renderer.bindCameraSet(bindPoint, getPipelineLayout(), renderContext, commands);
    }
    if(description.type == PipelineType::Blit || description.type == PipelineType::Skybox) {
        if(descriptorPool) {
            bindDescriptorSets(commands, {descriptorSets0[renderContext.swapchainIndex]}, {});
        }
    } else if(description.type == PipelineType::GBuffer) {
        if(description.vertexFormat == VertexFormat::Vertex || description.vertexFormat == VertexFormat::SkinnedVertex) { // FIXME hack to make sprites work
            if(descriptorPool) {
                bindDescriptorSets(commands, {descriptorSets0[renderContext.swapchainIndex]}, {});
            }
            renderContext.renderer.getMaterialSystem().bind(renderContext, commands, 1, *layout, bindPoint);
        } else if(!description.tmptmptmpUseMaterialSystem) {
            if(descriptorPool) {
                bindDescriptorSets(commands, {descriptorSets0[renderContext.swapchainIndex]}, {0});
            }
        }
    } else if(!description.tmptmptmpUseMaterialSystem) {
        if(descriptorPool) {
            bindDescriptorSets(commands, {descriptorSets0[renderContext.swapchainIndex]}, {0});
        }
        renderContext.renderer.getMaterialSystem().bind(renderContext, commands, 1, *layout, bindPoint);
    }

    if(description.tmptmptmpUseMaterialSystem) {
        renderContext.renderer.getMaterialSystem().bind(renderContext, commands, 0, *layout, bindPoint);
    }

    if(description.type == PipelineType::GResolve) {
        renderContext.renderer.getLighting().bind(renderContext, commands, 3, *layout, bindPoint); // TODO: don't hardcode
    }
}

const vk::PipelineLayout& Carrot::Pipeline::getPipelineLayout() const {
    return *layout;
}

const vk::DescriptorSetLayout& Carrot::Pipeline::getDescriptorSetLayout() const {
    return *descriptorSetLayout0;
}

void Carrot::Pipeline::bindDescriptorSets(vk::CommandBuffer& commands,
                                          const std::vector<vk::DescriptorSet>& descriptors,
                                          const std::vector<std::uint32_t>& dynamicOffsets,
                                          std::uint32_t firstSet) const {
    commands.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *layout, firstSet, descriptors, dynamicOffsets);
}

void Carrot::Pipeline::recreateDescriptorPool(uint32_t imageCount) {
    std::vector<vk::DescriptorPoolSize> sizes{};

    std::vector<NamedBinding> bindings{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addBindingsSet0(stage, bindings, description.constants);
    }

    for(const auto& binding : bindings) {
        sizes.emplace_back(vk::DescriptorPoolSize {
            .type = binding.vkBinding.descriptorType,
            .descriptorCount = imageCount*binding.vkBinding.descriptorCount,
        });
    }

    if(sizes.empty())
        return;

    vk::DescriptorPoolCreateInfo poolInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = imageCount,
            .poolSizeCount = static_cast<uint32_t>(sizes.size()),
            .pPoolSizes = sizes.data(),
    };

    descriptorPool = driver.getLogicalDevice().createDescriptorPoolUnique(poolInfo, driver.getAllocationCallbacks());

    allocateDescriptorSets();
}

void Carrot::Pipeline::allocateDescriptorSets() {
    if(descriptorPool) {
        driver.getLogicalDevice().resetDescriptorPool(*descriptorPool);
    }
    descriptorSets0 = allocateDescriptorSets0();
}

std::vector<vk::DescriptorSet> Carrot::Pipeline::allocateDescriptorSets0() {
    if(!descriptorPool) {
        return std::vector<vk::DescriptorSet>{driver.getSwapchainImageCount(), vk::DescriptorSet{}};
    }
    std::vector<vk::DescriptorSetLayout> layouts{driver.getSwapchainImageCount(), *descriptorSetLayout0};
    vk::DescriptorSetAllocateInfo allocateInfo {
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };
    std::vector<vk::DescriptorSet> sets = driver.getLogicalDevice().allocateDescriptorSets(allocateInfo);

    std::vector<NamedBinding> bindings{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addBindingsSet0(stage, bindings, description.constants);
    }

    // allocate default values
    std::vector<vk::WriteDescriptorSet> writes{bindings.size() * driver.getSwapchainImageCount()};
    std::vector<vk::DescriptorBufferInfo> buffers{bindings.size() * driver.getSwapchainImageCount()};
    std::vector<vk::DescriptorImageInfo> samplers{bindings.size() * driver.getSwapchainImageCount()};
    std::vector<vk::DescriptorImageInfo> images{bindings.size() * driver.getSwapchainImageCount()};

    std::uint32_t writeIndex = 0;
    for(const auto& binding : bindings) {
        for(std::size_t i = 0; i < driver.getSwapchainImageCount(); i++) {
            auto& write = writes[writeIndex];
            write.dstBinding = binding.vkBinding.binding;
            write.descriptorCount = binding.vkBinding.descriptorCount;
            write.descriptorType = binding.vkBinding.descriptorType;
            write.dstSet = sets[i];

            switch (binding.vkBinding.descriptorType) {
                case vk::DescriptorType::eUniformBufferDynamic: {
                    auto& buffer = buffers[writeIndex];
                    if(binding.name == "Debug") {
                        buffer.buffer = driver.getDebugUniformBuffers()[i]->getVulkanBuffer();
                        buffer.range = sizeof(DebugBufferObject);
                    } else if(binding.name == "Particles") {
                        // TODO
                        buffer.buffer = driver.getDebugUniformBuffers()[i]->getVulkanBuffer();
                        buffer.range = sizeof(DebugBufferObject);
                    } else {
                        // TODO
                        buffer.buffer = driver.getDebugUniformBuffers()[i]->getVulkanBuffer();
                        buffer.range = sizeof(DebugBufferObject);
                    }
                    buffer.offset = 0;
                    write.pBufferInfo = buffers.data()+writeIndex;

                    writeIndex++;
                }
                break;

                case vk::DescriptorType::eSampler: {
                    auto& sampler = samplers[writeIndex];
                    sampler.sampler = driver.getLinearSampler();
                    write.pImageInfo = samplers.data()+writeIndex;

                    writeIndex++;
                }
                break;
            }
        }
    }

    driver.getLogicalDevice().updateDescriptorSets(writeIndex, writes.data(), 0, nullptr);
    return sets;
}

const std::vector<vk::DescriptorSet>& Carrot::Pipeline::getDescriptorSets0() const {
    return descriptorSets0;
}

Carrot::VertexFormat Carrot::Pipeline::getVertexFormat() const {
    return description.vertexFormat;
}

Carrot::PipelineType Carrot::Pipeline::getPipelineType(const std::string& name) {
    if(name == "gResolve") {
        return PipelineType::GResolve;
    } else if(name == "gbuffer") {
        return PipelineType::GBuffer;
    } else if(name == "blit") {
        return PipelineType::Blit;
    } else if(name == "skybox") {
        return PipelineType::Skybox;
    } else if(name == "particles") {
        return PipelineType::Particles;
    }
    return PipelineType::Unknown;
}

void Carrot::Pipeline::onSwapchainImageCountChange(std::size_t newCount) {
    allocateDescriptorSets();
}

void Carrot::Pipeline::onSwapchainSizeChange(int newWidth, int newHeight) {

}

Carrot::PipelineDescription::PipelineDescription(const Carrot::IO::Resource jsonFile) {
    rapidjson::Document json;
    json.Parse(jsonFile.readText().c_str());

    if(json.HasMember("texturesBindingIndex"))
        texturesBindingIndex = json["texturesBindingIndex"].GetUint64();
    if(json.HasMember("materialStorageBufferBindingIndex"))
        materialStorageBufferBindingIndex = json["materialStorageBufferBindingIndex"].GetUint64();

    subpassIndex = json["subpassIndex"].GetUint64();
    vertexShader = json["vertexShader"].GetString();
    fragmentShader = json["fragmentShader"].GetString();
    vertexFormat = Carrot::getVertexFormat(json["vertexFormat"].GetString());
    type = Pipeline::getPipelineType(json["type"].GetString());

    depthWrite = json["depthWrite"].GetBool();
    depthTest = json["depthTest"].GetBool();
    if(json.HasMember("reserveSet2ForCamera")) {
        reserveSet2ForCamera = json["reserveSet2ForCamera"].GetBool();
    }

    if(json.HasMember("constants")) {
        for(const auto& constant : json["constants"].GetObject()) {
            constants[std::string(constant.name.GetString())] = static_cast<std::uint32_t>(constant.value.GetUint64());
        }
    }

    if(json.HasMember("cull")) {
        cull = json["cull"].GetBool();
    }
    if(json.HasMember("alphaBlending")) {
        alphaBlending = json["alphaBlending"].GetBool();
    }

    if(json.HasMember("topology")) {
        std::string topologyStr = Carrot::toLowerCase(json["topology"].GetString());
        if(topologyStr == "triangles") {
            topology = vk::PrimitiveTopology::eTriangleList;
        } else if(topologyStr == "lines") {
            topology = vk::PrimitiveTopology::eLineList;
        } else {
            verify(false, "Unknown topology: " + topologyStr);
        }
    }

    if(json.HasMember("tmptmptmpUseMaterialSystem")) {
        tmptmptmpUseMaterialSystem = json["tmptmptmpUseMaterialSystem"].GetBool();
    }
}
