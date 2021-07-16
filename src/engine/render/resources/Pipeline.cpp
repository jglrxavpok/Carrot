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
#include "engine/render/Material.h"
#include "Vertex.h"
#include "engine/render/DebugBufferObject.h"
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
            .vertexBindingDescriptionCount = static_cast<uint32_t>(pipelineTemplate.vertexBindingDescriptions.size()),
            .pVertexBindingDescriptions = pipelineTemplate.vertexBindingDescriptions.data(),

            .vertexAttributeDescriptionCount = static_cast<uint32_t>(pipelineTemplate.vertexAttributes.size()),
            .pVertexAttributeDescriptions = pipelineTemplate.vertexAttributes.data(),
    };

    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;


    pipelineTemplate.inputAssembly = {
            .topology = topology,
            .primitiveRestartEnable = false,
    };

    pipelineTemplate.rasterizer = {
            // TODO: change for shadow maps
            .depthClampEnable = false,

            .rasterizerDiscardEnable = false,

            .polygonMode = vk::PolygonMode::eFill,

            .cullMode = description.type == PipelineType::Skybox ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eFront,
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

    pipelineTemplate.colorBlendAttachments = {
            static_cast<size_t>(description.type == PipelineType::Particles || description.type == PipelineType::GBuffer ? 4 : 1),
            {
                    // TODO: blending
                    .blendEnable = false,

                    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
            }
    };

    pipelineTemplate.colorBlending = vk::PipelineColorBlendStateCreateInfo {
            .logicOpEnable = false,
            .attachmentCount = static_cast<uint32_t>(pipelineTemplate.colorBlendAttachments.size()),
            .pAttachments = pipelineTemplate.colorBlendAttachments.data(),
    };

    vector<vk::PushConstantRange> pushConstants{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addPushConstants(stage, pushConstants);
    }

    vector<vk::DescriptorSetLayout> layouts{};
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

    if(description.reserveSet2ForCamera) {
        while(layouts.size() < 2) {
            layouts.push_back(driver.getEmptyDescriptorSetLayout());
        }
        layouts.push_back(driver.getMainCameraDescriptorSetLayout());
    }

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .setLayoutCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size()),
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

        .minDepth = 0.0f,
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

    vector<Specialization> specializations{};

    for(const auto& [constantName, constantValue] : description.constants) {
        if("MAX_MATERIALS" == constantName) {
            maxMaterialID = constantValue;
        }
        if("MAX_TEXTURES" == constantName) {
            maxTextureID = constantValue;
        }
        Carrot::Log::info("Specializing constant %s to value %lu", constantName.c_str(), constantValue);
        specializations.push_back(Specialization {
                .offset = static_cast<uint32_t>(specializations.size()*sizeof(uint32_t)),
                .size = sizeof(uint32_t),
                .constantID = static_cast<uint32_t>(specializations.size()), // TODO: Map name to index
        });
        pipelineTemplate.specializationData.push_back(static_cast<uint32_t>(constantValue));
    }

    uint32_t totalDataSize = 0;
    vector<vk::SpecializationMapEntry> entries{};
    for(const auto& spec : specializations) {
        entries.emplace_back(spec.convertToEntry());
        totalDataSize += spec.size;
    }
    pipelineTemplate.specializationEntries = entries;
    pipelineTemplate.specialization = {
            .mapEntryCount = static_cast<uint32_t>(pipelineTemplate.specializationEntries.size()),
            .pMapEntries = pipelineTemplate.specializationEntries.data(),
            .dataSize = totalDataSize,
            .pData = pipelineTemplate.specializationData.data(),
    };

    pipelineTemplate.shaderStageCreation = stages->createPipelineShaderStages(&pipelineTemplate.specialization);
    pipelineTemplate.pipelineInfo = vk::GraphicsPipelineCreateInfo {
            .stageCount = static_cast<uint32_t>(pipelineTemplate.shaderStageCreation.size()),
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
            .subpass = static_cast<uint32_t>(description.subpassIndex),
    };

    if(description.type == PipelineType::GBuffer) {
        materialStorageBuffer = make_unique<Buffer>(driver,
                                                    sizeof(MaterialData)*maxMaterialID,
                                                    vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
                                                    vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                    driver.createGraphicsAndTransferFamiliesSet());
        vector<char> zeroes{};
        zeroes.resize(materialStorageBuffer->getSize());
        materialStorageBuffer->stageUploadWithOffsets(make_pair(static_cast<uint64_t>(0), zeroes));
    }

    recreateDescriptorPool(driver.getSwapchainImageCount());
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
    commands.bindPipeline(bindPoint, getOrCreatePipelineForRenderPass(pass));
    if(description.reserveSet2ForCamera && description.vertexFormat != VertexFormat::SkinnedVertex) {
        bindDescriptorSets(commands, {driver.getEmptyDescriptorSet()}, {}, 1);
        renderContext.renderer.bindCameraSet(bindPoint, getPipelineLayout(), renderContext, commands);
    }
    if(description.type == PipelineType::Blit || description.type == PipelineType::Skybox) {
        bindDescriptorSets(commands, {descriptorSets0[renderContext.swapchainIndex]}, {});
    } else if(description.type == PipelineType::GBuffer) {
        bindDescriptorSets(commands, {descriptorSets0[renderContext.swapchainIndex]}, {0});
    } else {
        bindDescriptorSets(commands, {descriptorSets0[renderContext.swapchainIndex]}, {0});
    }
}

const vk::PipelineLayout& Carrot::Pipeline::getPipelineLayout() const {
    return *layout;
}

const vk::DescriptorSetLayout& Carrot::Pipeline::getDescriptorSetLayout() const {
    return *descriptorSetLayout0;
}

void Carrot::Pipeline::bindDescriptorSets(vk::CommandBuffer& commands,
                                          const vector<vk::DescriptorSet>& descriptors,
                                          const vector<uint32_t>& dynamicOffsets,
                                          uint32_t firstSet) const {
    commands.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *layout, firstSet, descriptors, dynamicOffsets);
}

void Carrot::Pipeline::recreateDescriptorPool(uint32_t imageCount) {
    vector<vk::DescriptorPoolSize> sizes{};

    vector<NamedBinding> bindings{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addBindingsSet0(stage, bindings, description.constants);
    }

    for(const auto& binding : bindings) {
        sizes.emplace_back(vk::DescriptorPoolSize {
            .type = binding.vkBinding.descriptorType,
            .descriptorCount = imageCount*binding.vkBinding.descriptorCount,
        });
    }

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
    driver.getLogicalDevice().resetDescriptorPool(*descriptorPool);
    descriptorSets0 = allocateDescriptorSets0();

    if(description.type == PipelineType::GBuffer) {
        for(uint64_t imageIndex = 0; imageIndex < driver.getSwapchainImageCount(); imageIndex++) {
            vk::DescriptorBufferInfo storageBufferInfo{
                    .buffer = materialStorageBuffer->getVulkanBuffer(),
                    .offset = 0,
                    .range = materialStorageBuffer->getSize(),
            };
            vector<vk::WriteDescriptorSet> writes{1+1 /*textures+material storage buffer*/};
            writes[0].dstSet = descriptorSets0[imageIndex];
            writes[0].descriptorCount = 1;
            writes[0].descriptorType = vk::DescriptorType::eStorageBufferDynamic;
            writes[0].pBufferInfo = &storageBufferInfo;
            writes[0].dstBinding = description.materialStorageBufferBindingIndex;

            writes[1].dstSet = descriptorSets0[imageIndex];
            writes[1].descriptorCount = maxTextureID;
            writes[1].descriptorType = vk::DescriptorType::eSampledImage;
            writes[1].dstBinding = description.texturesBindingIndex;

            vector<vk::DescriptorImageInfo> imageInfoStructs {maxTextureID};
            writes[1].pImageInfo = imageInfoStructs.data();

            for(TextureID textureID = 0; textureID < maxTextureID; textureID++) {
                auto& info = imageInfoStructs[textureID];
                info.imageView = driver.getDefaultTexture().getView();
                info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            }

            driver.getLogicalDevice().updateDescriptorSets(writes, {});
        }
    }
}

vector<vk::DescriptorSet> Carrot::Pipeline::allocateDescriptorSets0() {
    vector<vk::DescriptorSetLayout> layouts{driver.getSwapchainImageCount(), *descriptorSetLayout0};
    vk::DescriptorSetAllocateInfo allocateInfo {
        .descriptorPool = *descriptorPool,
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts = layouts.data(),
    };
    vector<vk::DescriptorSet> sets = driver.getLogicalDevice().allocateDescriptorSets(allocateInfo);

    vector<NamedBinding> bindings{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addBindingsSet0(stage, bindings, description.constants);
    }

    // allocate default values
    vector<vk::WriteDescriptorSet> writes{bindings.size() * driver.getSwapchainImageCount()};
    vector<vk::DescriptorBufferInfo> buffers{bindings.size() * driver.getSwapchainImageCount()};
    vector<vk::DescriptorImageInfo> samplers{bindings.size() * driver.getSwapchainImageCount()};
    vector<vk::DescriptorImageInfo> images{bindings.size() * driver.getSwapchainImageCount()};

    uint32_t writeIndex = 0;
    for(const auto& binding : bindings) {
        for(size_t i = 0; i < driver.getSwapchainImageCount(); i++) {
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

    driver.getLogicalDevice().updateDescriptorSets(writeIndex, writes.data(), 0, nullptr);
    return sets;
}

const vector<vk::DescriptorSet>& Carrot::Pipeline::getDescriptorSets0() const {
    return descriptorSets0;
}

Carrot::MaterialID Carrot::Pipeline::reserveMaterialSlot(const Carrot::Material& material) {
    if(materialID >= maxMaterialID) {
        throw runtime_error("Max material id is " + to_string(maxMaterialID));
    }
    MaterialID id = materialID++;
    updateMaterial(material, id);
    return id;
}

Carrot::TextureID Carrot::Pipeline::reserveTextureSlot(const vk::ImageView& textureView) {
    if(textureID >= maxTextureID) {
        throw runtime_error("Max texture id is " + to_string(maxTextureID));
    }
    TextureID id = textureID++;
    reservedTextures[id] = textureView;

    for(size_t imageIndex = 0; imageIndex < driver.getSwapchainImageCount(); imageIndex++) {
        updateTextureReservation(textureView, id, imageIndex);
    }
    return id;
}

void Carrot::Pipeline::updateTextureReservation(const vk::ImageView& textureView, TextureID id, size_t imageIndex) {
    vk::DescriptorImageInfo imageInfo {
            .imageView = textureView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
    };
    vk::WriteDescriptorSet write {
            .dstSet = descriptorSets0[imageIndex],
            .dstBinding = static_cast<std::uint32_t>(description.texturesBindingIndex),
            .dstArrayElement = id,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eSampledImage,
            .pImageInfo = &imageInfo
    };
    driver.getLogicalDevice().updateDescriptorSets(write, {});
}

void Carrot::Pipeline::updateMaterial(const Carrot::Material& material, MaterialID materialID) {
    MaterialData data = {
        material.getTextureID(),
        material.ignoresInstanceColor(),
    };
    materialStorageBuffer->stageUploadWithOffset(static_cast<uint64_t>(materialID)*sizeof(MaterialData), &data);
}

void Carrot::Pipeline::updateMaterial(const Carrot::Material& material) {
    updateMaterial(material, material.getMaterialID());
}

Carrot::VertexFormat Carrot::Pipeline::getVertexFormat() const {
    return description.vertexFormat;
}

Carrot::PipelineType Carrot::Pipeline::getPipelineType(const string& name) {
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

void Carrot::Pipeline::onSwapchainImageCountChange(size_t newCount) {
    allocateDescriptorSets();

    // pipeline will be refreshed, so update reserved slots
    for(TextureID texID = 0; texID < textureID; texID++) {
        auto& textureView = reservedTextures[texID];
        for(size_t imageIndex = 0; imageIndex < driver.getSwapchainImageCount(); imageIndex++) {
            updateTextureReservation(textureView, texID, imageIndex);
        }
    }
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
}
