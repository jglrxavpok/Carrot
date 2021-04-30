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

#include <iostream>

Carrot::Pipeline::Pipeline(Carrot::VulkanDriver& driver, vk::RenderPass& renderPass, const string& pipelineName): driver(driver), renderPass(renderPass) {
    rapidjson::Document description;
    description.Parse(IO::readFileAsText("resources/pipelines/"+pipelineName+".json").c_str());

    auto& device = driver.getLogicalDevice();
    stages = make_unique<Carrot::ShaderStages>(driver,
                                               vector<string> {
                                                     description["vertexShader"].GetString(),
                                                     description["fragmentShader"].GetString()
                                             });

    string typeStr = description["type"].GetString();
    type = getPipelineType(typeStr);

    if(description.HasMember("subpassIndex")) {
        subpassIndex = description["subpassIndex"].GetUint64();
    }

    if(description.HasMember("constants")) {
        for(const auto& constant : description["constants"].GetObject()) {
            constants[constant.name.GetString()] = constant.value.GetUint64();
        }
    }
    descriptorSetLayout0 = stages->createDescriptorSetLayout0(constants);

    string vertexFormatStr = description["vertexFormat"].GetString();
    vertexFormat = Carrot::getVertexFormat(vertexFormatStr);

    vector<vk::VertexInputBindingDescription> descriptions = Carrot::getBindingDescriptions(vertexFormat);
    vector<vk::VertexInputAttributeDescription> attributes = Carrot::getAttributeDescriptions(vertexFormat);

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount = static_cast<uint32_t>(descriptions.size()),
            .pVertexBindingDescriptions = descriptions.data(),

            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size()),
            .pVertexAttributeDescriptions = attributes.data(),
    };

    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
    if(description.HasMember("topology")) {
        // TODO
    }

    vk::PipelineInputAssemblyStateCreateInfo inputAssembly{
            .topology = topology,
            .primitiveRestartEnable = false,
    };

    vk::PipelineRasterizationStateCreateInfo rasterizer{
            // TODO: change for shadow maps
            .depthClampEnable = false,

            .rasterizerDiscardEnable = false,

            .polygonMode = vk::PolygonMode::eFill,

            .cullMode = type == Type::Skybox ? vk::CullModeFlagBits::eNone : vk::CullModeFlagBits::eFront,
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
            .depthTestEnable = type != Type::GResolve,
            .depthWriteEnable = type != Type::GResolve && type != Type::Skybox,
            .depthCompareOp = vk::CompareOp::eLessOrEqual,
            .stencilTestEnable = false,
    };

    vk::PipelineColorBlendAttachmentState colorBlendAttachment{
            // TODO: blending
            .blendEnable = false,

            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA,
    };

    vector<vk::PipelineColorBlendAttachmentState> colorBlendingStates =
            {
            static_cast<size_t>(type == Type::Particles || type == Type::GBuffer ? 4 : 1),
            colorBlendAttachment
            };

    vk::PipelineColorBlendStateCreateInfo colorBlending{
            .logicOpEnable = false,
            .attachmentCount = static_cast<uint32_t>(colorBlendingStates.size()),
            .pAttachments = colorBlendingStates.data(),
    };

    vector<vk::PushConstantRange> pushConstants{};
    for(const auto& [stage, module] : stages->getModuleMap()) {
        module->addPushConstants(stage, pushConstants);
    }

    vector<vk::DescriptorSetLayout> layouts{};
    layouts.push_back(*descriptorSetLayout0);
    if(vertexFormat == VertexFormat::SkinnedVertex) {
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

    vk::PipelineLayoutCreateInfo pipelineLayoutCreateInfo{
            .setLayoutCount = static_cast<uint32_t>(layouts.size()),
            .pSetLayouts = layouts.data(),
            .pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size()),
            .pPushConstantRanges = pushConstants.data(),
    };

    layout = device.createPipelineLayoutUnique(pipelineLayoutCreateInfo, driver.getAllocationCallbacks());

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
        .width = static_cast<float>(driver.getSwapchainExtent().width),
        .height = static_cast<float>(driver.getSwapchainExtent().height),

        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    vk::Rect2D scissor {
        .offset = {0,0},
        .extent = {
                .width = driver.getSwapchainExtent().width,
                .height = driver.getSwapchainExtent().height,
        }
    };

    vk::PipelineViewportStateCreateInfo viewportState {
        .viewportCount = 1,
        .pViewports = &viewport,

        .scissorCount = 1,
        .pScissors = &scissor,
    };

    vector<Specialization> specializations{};
    vector<uint32_t> specializationData{}; // TODO: support something else than uint32
    if(description.HasMember("constants")) {
        for(const auto& constants : description["constants"].GetObject()) {
            if("MAX_MATERIALS" == constants.name) {
                maxMaterialID = constants.value.GetUint64();
            }
            if("MAX_TEXTURES" == constants.name) {
                maxTextureID = constants.value.GetUint64();
            }
            cout << "Specializing constant " << constants.name.GetString() << " to value " << constants.value.GetUint64() << endl;
            specializations.push_back(Specialization {
                    .offset = static_cast<uint32_t>(specializations.size()*sizeof(uint32_t)),
                    .size = sizeof(uint32_t),
                    .constantID = static_cast<uint32_t>(specializations.size()), // TODO: Map name to index
            });
            specializationData.push_back(static_cast<uint32_t>(constants.value.GetUint64()));
        }
    }

    uint32_t totalDataSize = 0;
    vector<vk::SpecializationMapEntry> entries{};
    for(const auto& spec : specializations) {
        entries.emplace_back(spec.convertToEntry());
        totalDataSize += spec.size;
    }
    vk::SpecializationInfo specialization {
            .mapEntryCount = static_cast<uint32_t>(entries.size()),
            .pMapEntries = entries.data(),
            .dataSize = totalDataSize,
            .pData = specializationData.data(),
    };

    auto shaderStageCreation = stages->createPipelineShaderStages(&specialization);
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
            .renderPass = renderPass,
            .subpass = static_cast<uint32_t>(subpassIndex),
    };

    vkPipeline = device.createGraphicsPipelineUnique(nullptr, pipelineInfo, driver.getAllocationCallbacks());

    if(type == Type::GBuffer) {
        materialStorageBuffer = make_unique<Buffer>(driver,
                                                    sizeof(MaterialData)*maxMaterialID,
                                                    vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer,
                                                    vk::MemoryPropertyFlagBits::eDeviceLocal,
                                                    driver.createGraphicsAndTransferFamiliesSet());
        vector<char> zeroes{};
        zeroes.resize(materialStorageBuffer->getSize());
        materialStorageBuffer->stageUploadWithOffsets(make_pair(static_cast<uint64_t>(0), zeroes));
    }

    if(description.HasMember("texturesBindingIndex"))
        texturesBindingIndex = description["texturesBindingIndex"].GetUint64();
    if(description.HasMember("materialStorageBufferBindingIndex"))
        materialsBindingIndex = description["materialStorageBufferBindingIndex"].GetUint64();

    recreateDescriptorPool(driver.getSwapchainImageCount());
}

void Carrot::Pipeline::bind(uint32_t imageIndex, vk::CommandBuffer& commands, vk::PipelineBindPoint bindPoint) const {
    commands.bindPipeline(bindPoint, *vkPipeline);
    if(type == Type::GBuffer) {
        bindDescriptorSets(commands, {descriptorSets0[imageIndex]}, {0, 0});
    } else if(type == Type::Blit) {
        bindDescriptorSets(commands, {descriptorSets0[imageIndex]}, {});
    } else {
        bindDescriptorSets(commands, {descriptorSets0[imageIndex]}, {0, 0});
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
        module->addBindingsSet0(stage, bindings, constants);
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

    if(type == Type::GBuffer) {
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
            writes[0].dstBinding = materialsBindingIndex;

            writes[1].dstSet = descriptorSets0[imageIndex];
            writes[1].descriptorCount = maxTextureID;
            writes[1].descriptorType = vk::DescriptorType::eSampledImage;
            writes[1].dstBinding = texturesBindingIndex;

            vector<vk::DescriptorImageInfo> imageInfoStructs {maxTextureID};
            writes[1].pImageInfo = imageInfoStructs.data();

            for(TextureID textureID = 0; textureID < maxTextureID; textureID++) {
                auto& info = imageInfoStructs[textureID];
                info.imageView = *driver.getDefaultImageView();
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
        module->addBindingsSet0(stage, bindings, constants);
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
                        buffer.buffer = driver.getCameraUniformBuffers()[i]->getVulkanBuffer();
                        buffer.range = sizeof(CameraBufferObject); // TODO: customizable
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

Carrot::TextureID Carrot::Pipeline::reserveTextureSlot(const vk::UniqueImageView& textureView) {
    if(textureID >= maxTextureID) {
        throw runtime_error("Max texture id is " + to_string(maxTextureID));
    }
    TextureID id = textureID++;
    reservedTextures[id] = *textureView;

    for(size_t imageIndex = 0; imageIndex < driver.getSwapchainImageCount(); imageIndex++) {
        updateTextureReservation(*textureView, id, imageIndex);
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
            .dstBinding = texturesBindingIndex,
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
    return vertexFormat;
}

Carrot::Pipeline::Type Carrot::Pipeline::getPipelineType(const string& name) {
    if(name == "gResolve") {
        return Type::GResolve;
    } else if(name == "gbuffer") {
        return Type::GBuffer;
    } else if(name == "blit") {
        return Type::Blit;
    } else if(name == "skybox") {
        return Type::Skybox;
    } else if(name == "particles") {
        return Type::Particles;
    }
    return Type::Unknown;
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
