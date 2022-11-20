//
// Created by jglrxavpok on 01/12/2020.
//

#include <engine/constants.h>
#include "Pipeline.h"
#include "engine/render/CameraBufferObject.h"
#include "engine/render/resources/Buffer.h"
#include <rapidjson/document.h>
#include "core/io/IO.h"
#include "engine/render/DrawData.h"
#include "Vertex.h"
#include "engine/render/MaterialSystem.h"
#include "Mesh.h"
#include <core/io/Logging.hpp>
#include <core/utils/Lookup.hpp>

static Carrot::Lookup PolygonModes = std::array {
        Carrot::LookupEntry<vk::PolygonMode>(vk::PolygonMode::eFill, "fill"),
        Carrot::LookupEntry<vk::PolygonMode>(vk::PolygonMode::eLine, "wireframe"),
        Carrot::LookupEntry<vk::PolygonMode>(vk::PolygonMode::eFill, "point_cloud"),
};

static Carrot::Lookup DescriptorSetType = std::array {
        Carrot::LookupEntry<Carrot::PipelineDescription::DescriptorSet::Type>(Carrot::PipelineDescription::DescriptorSet::Type::Autofill, "autofill"),
        Carrot::LookupEntry<Carrot::PipelineDescription::DescriptorSet::Type>(Carrot::PipelineDescription::DescriptorSet::Type::Empty, "empty"),
        Carrot::LookupEntry<Carrot::PipelineDescription::DescriptorSet::Type>(Carrot::PipelineDescription::DescriptorSet::Type::Camera, "camera"),
        Carrot::LookupEntry<Carrot::PipelineDescription::DescriptorSet::Type>(Carrot::PipelineDescription::DescriptorSet::Type::Materials, "materials"),
        Carrot::LookupEntry<Carrot::PipelineDescription::DescriptorSet::Type>(Carrot::PipelineDescription::DescriptorSet::Type::Lights, "lights"),
        Carrot::LookupEntry<Carrot::PipelineDescription::DescriptorSet::Type>(Carrot::PipelineDescription::DescriptorSet::Type::Debug, "debug"),
};

Carrot::Pipeline::Pipeline(Carrot::VulkanDriver& driver, const Carrot::IO::Resource pipelineDescription):
    Carrot::Pipeline::Pipeline(driver, PipelineDescription(pipelineDescription)) {

}

Carrot::Pipeline::Pipeline(Carrot::VulkanDriver& driver, const PipelineDescription description): driver(driver), description(description) {
    auto& device = driver.getLogicalDevice();
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

            .polygonMode = description.polygonMode,

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
            .depthTestEnable = description.depthTest,
            .depthWriteEnable = description.depthWrite,
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

            // R8G8B8A8 metallic+roughness
            pipelineTemplate.colorBlendAttachments.push_back(vk::PipelineColorBlendAttachmentState {
                    .blendEnable = false,

                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
            });

            // R8G8B8A8 emissive
            pipelineTemplate.colorBlendAttachments.push_back(vk::PipelineColorBlendAttachmentState {
                    .blendEnable = false,

                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
            });

            // R32G32B32 tangents
            pipelineTemplate.colorBlendAttachments.push_back(vk::PipelineColorBlendAttachmentState {
                    .blendEnable = false,

                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
            });
        }
    } else {
        pipelineTemplate.colorBlendAttachments = {
                static_cast<std::size_t>(description.type == PipelineType::Particles || description.type == PipelineType::GBuffer ? 7 : 1),
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

    stages = make_unique<Carrot::ShaderStages>(driver,
                                               std::vector<Render::ShaderSource> {
                                                       description.vertexShader,
                                                       description.fragmentShader
                                               },
                                               std::vector<vk::ShaderStageFlagBits> {
                                                       vk::ShaderStageFlagBits::eVertex,
                                                       vk::ShaderStageFlagBits::eFragment
                                               }
    );

    reloadShaders();
}

void Carrot::Pipeline::reloadShaders() {
    WaitDeviceIdle();
    vkPipelines.clear(); // flush existing pipelines
    stages->reload();

    std::vector<vk::DescriptorSetLayout> layouts{};
    descriptorSetLayouts.clear();
    descriptorSetLayouts.resize(description.setCount);
    for (std::uint32_t i = 0; i < description.setCount; ++i) {
        auto setIt = description.descriptorSets.find(i);
        if(setIt == description.descriptorSets.end()) {
            description.descriptorSets[i].setID = i;
            setIt = description.descriptorSets.find(i);
        }
        const auto& set = setIt->second;
        switch(set.type) {
            case PipelineDescription::DescriptorSet::Type::Autofill: {
                descriptorSetLayouts[i] = stages->createDescriptorSetLayout(i, description.constants);
                layouts.push_back(*descriptorSetLayouts[i]);
            } break;

            default:
                descriptorSetLayouts[i].reset();
                layouts.push_back(getDescriptorSetLayout(i));
                break;
        }
    }

    std::vector<vk::PushConstantRange> pushConstants{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addPushConstants(stage, pushConstantMap);
    }
    for(const auto& [_, range] : pushConstantMap) {
        pushConstants.push_back(range);
    }

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .setLayoutCount = static_cast<std::uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = static_cast<std::uint32_t>(pushConstants.size()),
            .pPushConstantRanges = pushConstants.data(),
    };

    layout = GetVulkanDevice().createPipelineLayoutUnique(pipelineLayoutCreateInfo, driver.getAllocationCallbacks());

    std::vector<Specialization> specializations{};

    for(const auto& [constantName, constantValue] : description.constants) {
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

void Carrot::Pipeline::checkForReloadableShaders() {
    if(stages->shouldReload()) {
        reloadShaders();
    }
}

const vk::PushConstantRange& Carrot::Pipeline::getPushConstant(std::string_view name) const {
    return pushConstantMap[std::string(name)];
}

vk::Pipeline& Carrot::Pipeline::getOrCreatePipelineForRenderPass(vk::RenderPass pass) const {
    auto it = vkPipelines.find(pass);
    if(it == vkPipelines.end()) {
        vk::GraphicsPipelineCreateInfo info = pipelineTemplate.pipelineInfo;
        info.renderPass = pass;
        Carrot::Log::flush();
        vkPipelines[pass] = std::move(driver.getLogicalDevice().createGraphicsPipelineUnique(nullptr, info, nullptr));
    }
    return *vkPipelines[pass];
}

void Carrot::Pipeline::bind(vk::RenderPass pass, const Carrot::Render::Context& renderContext, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint) const {
    ZoneScopedN("Bind pipeline");
#ifdef TRACY_ENABLE
    std::string zoneText = Carrot::sprintf("Vertex = %s ; Fragment = %s", description.vertexShader.getName().c_str(), description.fragmentShader.getName().c_str());
    ZoneText(zoneText.c_str(), zoneText.size());
#endif
    auto& pipeline = getOrCreatePipelineForRenderPass(pass);
    commands.bindPipeline(bindPoint, pipeline);

    std::vector<vk::DescriptorSet> setsToBind { description.setCount, VK_NULL_HANDLE };
    for(std::uint32_t i = 0; i < setsToBind.size(); i++) {
        setsToBind[i] = getDescriptorSets(renderContext, i)[renderContext.swapchainIndex];
    }

    commands.bindDescriptorSets(bindPoint, *layout, 0, setsToBind, {});
}

const vk::PipelineLayout& Carrot::Pipeline::getPipelineLayout() const {
    return *layout;
}

const vk::DescriptorSetLayout& Carrot::Pipeline::getDescriptorSetLayout(std::uint32_t setID) const {
    const auto& set = description.descriptorSets.at(setID);
    switch(set.type) {
        case PipelineDescription::DescriptorSet::Type::Autofill:
            return *descriptorSetLayouts[setID];

        case PipelineDescription::DescriptorSet::Type::Empty:
            return GetVulkanDriver().getEmptyDescriptorSetLayout();

        case PipelineDescription::DescriptorSet::Type::Materials:
            return GetRenderer().getMaterialSystem().getDescriptorSetLayout();

        case PipelineDescription::DescriptorSet::Type::Camera:
            return GetRenderer().getCameraDescriptorSetLayout();

        case PipelineDescription::DescriptorSet::Type::Lights:
            return GetRenderer().getLighting().getDescriptorSetLayout();

        case PipelineDescription::DescriptorSet::Type::Debug:
            return GetRenderer().getDebugDescriptorSetLayout();

        default:
            std::terminate();
            return *((vk::DescriptorSetLayout*)nullptr);
    }
}

void Carrot::Pipeline::recreateDescriptorPool(uint32_t imageCount) {
    std::vector<vk::DescriptorPoolSize> sizes{};

    std::vector<NamedBinding> bindings{};
    for(std::uint32_t setID = 0; setID < description.setCount; setID++) {
        for(const auto& [stage, module] : stages->getModuleMap()) {
            module->addBindingsSet(stage, setID, bindings, description.constants);
        }
    }

    for(auto binding : bindings) {
        if(binding.vkBinding.descriptorCount == 0) {
            Carrot::Log::warn("DescriptorCount is set to 0, replacing to 255 not to crash");
            binding.vkBinding.descriptorCount = 255;
        }
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
    descriptorSets.clear();
    descriptorSets.resize(description.setCount);
    for (std::uint32_t i = 0; i < description.setCount; ++i) {
        auto setIt = description.descriptorSets.find(i);
        if(setIt != description.descriptorSets.end()) { // not an empty set
            if(setIt->second.type == PipelineDescription::DescriptorSet::Type::Autofill) {
                descriptorSets[i] = allocateAutofillDescriptorSets(i);
            }
        }
    }
}

std::vector<vk::DescriptorSet> Carrot::Pipeline::allocateAutofillDescriptorSets(std::uint32_t setID) {
    if(!descriptorPool) {
        return std::vector<vk::DescriptorSet>{driver.getSwapchainImageCount(), vk::DescriptorSet{}};
    }
    std::vector<vk::DescriptorSetLayout> layouts{driver.getSwapchainImageCount(), getDescriptorSetLayout(setID)};
    vk::DescriptorSetAllocateInfo allocateInfo {
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };
    std::vector<vk::DescriptorSet> sets = driver.getLogicalDevice().allocateDescriptorSets(allocateInfo);

    std::vector<NamedBinding> bindings{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addBindingsSet(stage, setID, bindings, description.constants);
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
            bool wrote = false;

            switch (binding.vkBinding.descriptorType) {
                case vk::DescriptorType::eUniformBufferDynamic: {
                    auto& buffer = buffers[writeIndex];
                    if(binding.name == "Particles") {

                    } else {
                        TODO;
                    }
                    buffer.offset = 0;
                    write.pBufferInfo = buffers.data()+writeIndex;

                    writeIndex++;
                    wrote = true;
                }
                break;

                case vk::DescriptorType::eSampler: {
                    auto& sampler = samplers[writeIndex];
                    sampler.sampler = driver.getLinearSampler();
                    write.pImageInfo = samplers.data()+writeIndex;

                    writeIndex++;
                    wrote = true;
                }
                break;
            }

            if(wrote) {
                write.dstBinding = binding.vkBinding.binding;
                write.descriptorCount = binding.vkBinding.descriptorCount;
                write.descriptorType = binding.vkBinding.descriptorType;
                write.dstSet = sets[i];
            }
        }
    }

    driver.getLogicalDevice().updateDescriptorSets(writeIndex, writes.data(), 0, nullptr);
    return sets;
}

std::vector<vk::DescriptorSet> Carrot::Pipeline::getDescriptorSets(const Render::Context& renderContext, std::uint32_t setID) const {
    const auto& set = description.descriptorSets.at(setID);
    switch(set.type) {
        case PipelineDescription::DescriptorSet::Type::Autofill:
            return descriptorSets[setID];

        case PipelineDescription::DescriptorSet::Type::Materials:
            return GetRenderer().getMaterialSystem().getDescriptorSets();

        case PipelineDescription::DescriptorSet::Type::Camera:
        {
            std::vector<vk::DescriptorSet> sets { GetEngine().getSwapchainImageCount(), renderContext.viewport.getCameraDescriptorSet(renderContext) };
            return sets;
        }

        case PipelineDescription::DescriptorSet::Type::Lights:
        {
            std::vector<vk::DescriptorSet> sets { GetEngine().getSwapchainImageCount(), GetRenderer().getLighting().getCameraDescriptorSet(renderContext) };
            return sets;
        }

        case PipelineDescription::DescriptorSet::Type::Debug:
        {
            std::vector<vk::DescriptorSet> sets { GetEngine().getSwapchainImageCount(), GetRenderer().getDebugDescriptorSet(renderContext) };
            return sets;
        }

        case PipelineDescription::DescriptorSet::Type::Empty:
        default:
        {
            std::terminate(); // not allowed for this set type
            return *((std::vector<vk::DescriptorSet>*)nullptr);
        }
    }
}

Carrot::VertexFormat Carrot::Pipeline::getVertexFormat() const {
    return description.vertexFormat;
}

Carrot::PipelineType Carrot::Pipeline::getPipelineType(const std::string& name) {
    if(name == "lighting") {
        return PipelineType::Lighting;
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

    if(json.HasMember("_import")) {
        Carrot::IO::Resource imported = jsonFile.relative(json["_import"].GetString());
        *this = PipelineDescription(imported);
    }

    if(json.HasMember("subpassIndex")) {
        subpassIndex = json["subpassIndex"].GetUint64();
    }
    if(json.HasMember("vertexShader")) {
        vertexShader = json["vertexShader"].GetString();
    }
    if(json.HasMember("fragmentShader")) {
        fragmentShader = json["fragmentShader"].GetString();
    }
    if(json.HasMember("vertexFormat")) {
        vertexFormat = Carrot::getVertexFormat(json["vertexFormat"].GetString());
    }
    if(json.HasMember("type")) {
        type = Pipeline::getPipelineType(json["type"].GetString());
    }

    if(json.HasMember("depthWrite")) {
        depthWrite = json["depthWrite"].GetBool();
    }
    if(json.HasMember("depthTest")) {
        depthTest = json["depthTest"].GetBool();
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

    if(json.HasMember("fillMode")) {
        polygonMode = PolygonModes[json["fillMode"].GetString()];
    }

    if(json.HasMember("descriptorSets")) {
        setCount = 0;
        auto sets = json["descriptorSets"].GetArray();
        for(const auto& setData : sets) {
            const auto set = setData.GetObject();
            const DescriptorSet::Type descriptorType = DescriptorSetType[set["type"].GetString()];
            const std::uint32_t setID = set["setID"].GetUint();
            descriptorSets[setID] = DescriptorSet {
                .type = descriptorType,
                .setID = setID,
            };

            setCount = std::max(setCount, setID+1);
        }
    }
}
