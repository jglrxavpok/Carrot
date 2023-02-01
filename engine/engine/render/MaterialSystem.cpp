//
// Created by jglrxavpok on 24/10/2021.
//

#include "MaterialSystem.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/Engine.h"
#include "engine/console/RuntimeOption.hpp"
#include <core/io/Logging.hpp>
#include <core/math/BasicFunctions.h>

namespace Carrot::Render {
    static const std::uint32_t BindingCount = 4;
    static Carrot::RuntimeOption ShowDebug("Engine/Materials Debug", false);

    TextureHandle::TextureHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, MaterialSystem& system): WeakPoolHandle::WeakPoolHandle(index, destructor), materialSystem(system) {

    }

    void TextureHandle::updateHandle(const Carrot::Render::Context& renderContext) {
        auto& boundTextures = materialSystem.boundTextures[renderContext.swapchainIndex];
        auto imageViewToBind = texture ? texture->getView() : GetRenderer().getDefaultImage()->getView();
        auto it = boundTextures.find(getSlot());

        if(it != boundTextures.end() && it->second == imageViewToBind) {
            return;
        }

        boundTextures[getSlot()] = imageViewToBind;
        vk::DescriptorImageInfo imageInfo {
                .sampler = nullptr,
                .imageView = imageViewToBind,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        vk::WriteDescriptorSet write {
                .dstSet = materialSystem.descriptorSets[renderContext.swapchainIndex],
                .dstBinding = 1,
                .dstArrayElement = getSlot(),
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .pImageInfo = &imageInfo,
        };
        GetVulkanDevice().updateDescriptorSets(write, {});
    }

    TextureHandle::~TextureHandle() noexcept {
        for(std::size_t index = 0; index < GetEngine().getSwapchainImageCount(); index++) {
            materialSystem.boundTextures[index][getSlot()] = materialSystem.invalidTexture->getView();
        }
        materialSystem.descriptorNeedsUpdate = std::vector<bool>(materialSystem.descriptorSets.size(), true);
    }

    MaterialHandle::MaterialHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, MaterialSystem& system): WeakPoolHandle::WeakPoolHandle(index, destructor), materialSystem(system) {

    }

    void MaterialHandle::updateHandle(const Carrot::Render::Context& renderContext) {
        auto* data = materialSystem.getData(*this);

        auto getSlot = [&](const std::shared_ptr<TextureHandle>& texture, const std::shared_ptr<TextureHandle>& defaultTexture) {
            return texture ? texture->getSlot() : defaultTexture->getSlot();
        };

        data->baseColor = baseColor;
        data->emissiveColor = emissiveColor;
        data->emissive = getSlot(emissive, materialSystem.blackTextureHandle);
        data->metallicRoughnessFactor = glm::vec2 { roughnessFactor, metallicFactor };
        data->albedo = getSlot(albedo, materialSystem.whiteTextureHandle);
        data->normalMap = getSlot(normalMap, materialSystem.flatNormalTextureHandle);

        data->metallicRoughness = getSlot(metallicRoughness, materialSystem.blackTextureHandle);
        everUpdated = true;
    }

    MaterialHandle::~MaterialHandle() noexcept {
        if(!everUpdated) {
            return;
        }
        Carrot::Render::MaterialData* data = materialSystem.getData(*this);
        data->emissive = materialSystem.blackTextureHandle->getSlot();
        data->albedo = materialSystem.whiteTextureHandle->getSlot();
        data->normalMap = materialSystem.flatNormalTextureHandle->getSlot();

        data->metallicRoughness = materialSystem.blackTextureHandle->getSlot();
    }

    MaterialSystem::MaterialSystem() {
        reallocateMaterialBuffer(DefaultMaterialBufferSize);
        boundTextures.resize(GetEngine().getSwapchainImageCount());

        vk::ShaderStageFlags stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute | vk::ShaderStageFlagBits::eRaygenKHR | vk::ShaderStageFlagBits::eClosestHitKHR;
        std::array<vk::DescriptorSetLayoutBinding, BindingCount> bindings = {
            // Material Buffer
            vk::DescriptorSetLayoutBinding {
                    .binding = 0,
                    .descriptorType = vk::DescriptorType::eStorageBuffer,
                    .descriptorCount = 1,
                    .stageFlags = stageFlags
            },
            // Texture array
            vk::DescriptorSetLayoutBinding {
                    .binding = 1,
                    .descriptorType = vk::DescriptorType::eSampledImage,
                    .descriptorCount = MaxTextures,
                    .stageFlags = stageFlags
            },
            // Linear sampler
            vk::DescriptorSetLayoutBinding {
                    .binding = 2,
                    .descriptorType = vk::DescriptorType::eSampler,
                    .descriptorCount = 1,
                    .stageFlags = stageFlags
            },
            // Nearest sampler
            vk::DescriptorSetLayoutBinding {
                    .binding = 3,
                    .descriptorType = vk::DescriptorType::eSampler,
                    .descriptorCount = 1,
                    .stageFlags = stageFlags
            },
        };
        descriptorSetLayout = GetVulkanDevice().createDescriptorSetLayoutUnique(vk::DescriptorSetLayoutCreateInfo {
                .bindingCount = static_cast<std::uint32_t>(bindings.size()),
                .pBindings = bindings.data(),
        });
        std::array<vk::DescriptorPoolSize, BindingCount> poolSizes = {
                vk::DescriptorPoolSize {
                        .type = vk::DescriptorType::eStorageBuffer,
                        .descriptorCount = GetEngine().getSwapchainImageCount(),
                },
                vk::DescriptorPoolSize {
                        .type = vk::DescriptorType::eSampledImage,
                        .descriptorCount = GetEngine().getSwapchainImageCount() * MaxTextures,
                },
                vk::DescriptorPoolSize {
                        .type = vk::DescriptorType::eSampler,
                        .descriptorCount = GetEngine().getSwapchainImageCount(),
                },
                vk::DescriptorPoolSize {
                        .type = vk::DescriptorType::eSampler,
                        .descriptorCount = GetEngine().getSwapchainImageCount(),
                }
        };
        descriptorSetPool = GetVulkanDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
                .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = GetEngine().getSwapchainImageCount(),
                .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
        });
        reallocateDescriptorSets();

        std::unique_ptr<Carrot::Image> whiteImage = std::make_unique<Image>(GetVulkanDriver(),
            vk::Extent3D{.width = 1, .height = 1, .depth = 1},
            vk::ImageUsageFlagBits::eSampled,
            vk::Format::eR8G8B8A8Unorm);
        std::uint8_t whitePixel[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        whiteImage->stageUpload(std::span{whitePixel, 4});

        std::unique_ptr<Carrot::Image> blackImage = std::make_unique<Image>(GetVulkanDriver(),
                                                                            vk::Extent3D{.width = 1, .height = 1, .depth = 1},
                                                                            vk::ImageUsageFlagBits::eSampled,
                                                                            vk::Format::eR8G8B8A8Unorm);
        std::uint8_t blackPixel[4] = {0x0, 0x00, 0x00, 0xFF};
        blackImage->stageUpload(std::span{blackPixel, 4});

        std::unique_ptr<Carrot::Image> flatNormalImage = std::make_unique<Image>(GetVulkanDriver(),
                                                                            vk::Extent3D{.width = 1, .height = 1, .depth = 1},
                                                                            vk::ImageUsageFlagBits::eSampled,
                                                                            vk::Format::eR8G8B8A8Unorm);
        std::uint8_t flatNormalPixel[4] = {128, 128, 0xFF, 0xFF};
        flatNormalImage->stageUpload(std::span{flatNormalPixel, 4});

        whiteTexture = std::make_shared<Texture>(std::move(whiteImage));
        blackTexture = std::make_shared<Texture>(std::move(blackImage));
        flatNormalTexture = std::make_shared<Texture>(std::move(flatNormalImage));

        invalidTexture = GetRenderer().getOrCreateTexture("invalid_texture_reference.png");
        invalidTextureHandle = createTextureHandle(invalidTexture);
        invalidMaterialHandle = createMaterialHandle();
        invalidMaterialHandle->albedo = invalidTextureHandle;
        invalidMaterialHandle->normalMap = invalidTextureHandle;
        invalidMaterialHandle->emissive = invalidTextureHandle;
        invalidMaterialHandle->metallicRoughness = invalidTextureHandle;

        whiteTextureHandle = createTextureHandle(whiteTexture);
        blackTextureHandle = createTextureHandle(blackTexture);
        flatNormalTextureHandle = createTextureHandle(flatNormalTexture);
    }

    void MaterialSystem::updateDescriptorSets(const Carrot::Render::Context& renderContext) {
        if(descriptorNeedsUpdate[renderContext.swapchainIndex]) {
            auto& set = descriptorSets[renderContext.swapchainIndex];
            auto materialBufferInfo = materialBuffer->getWholeView().asBufferInfo();
            std::array<vk::DescriptorImageInfo, MaxTextures> imageInfo;
            for(std::size_t textureIndex = 0; textureIndex < MaxTextures; textureIndex++) {
                auto& info = imageInfo[textureIndex];
                info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                info.imageView = getBoundImageView(textureIndex, renderContext);
                info.sampler = nullptr;
            }

            vk::DescriptorImageInfo linearSampler {
                    .sampler = GetVulkanDriver().getLinearSampler(),
            };
            vk::DescriptorImageInfo nearestSampler {
                    .sampler = GetVulkanDriver().getNearestSampler(),
            };
            std::array<vk::WriteDescriptorSet, BindingCount> writes = {
                    // Material buffer
                    vk::WriteDescriptorSet {
                            .dstSet = set,
                            .dstBinding = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo = &materialBufferInfo,
                    },
                    // Texture array
                    vk::WriteDescriptorSet {
                            .dstSet = set,
                            .dstBinding = 1,
                            .descriptorCount = MaxTextures,
                            .descriptorType = vk::DescriptorType::eSampledImage,
                            .pImageInfo = imageInfo.data(),
                    },
                    // Linear sampler
                    vk::WriteDescriptorSet {
                            .dstSet = set,
                            .dstBinding = 2,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eSampler,
                            .pImageInfo = &linearSampler,
                    },
                    // Nearest sampler
                    vk::WriteDescriptorSet {
                            .dstSet = set,
                            .dstBinding = 3,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eSampler,
                            .pImageInfo = &nearestSampler,
                    },
            };
            GetVulkanDevice().updateDescriptorSets(writes, {});
            descriptorNeedsUpdate[renderContext.swapchainIndex] = false;
        }
    }

    void MaterialSystem::beginFrame(const Context& renderContext) {
        Async::LockGuard g { accessLock };

        if(ShowDebug) {
            bool isOpen = true;
            if(ImGui::Begin("Material debug", &isOpen)) {
                static int textureType = 0;
                ImGui::RadioButton("Diffuse", &textureType, 0);
                ImGui::RadioButton("NormalMap", &textureType, 1);
                ImGui::RadioButton("MetallicRoughness", &textureType, 2);
                ImGui::RadioButton("Emissive", &textureType, 3);

                const int columnCount = 8;
                if(ImGui::BeginTable("Materials", columnCount)) {
                    int index = 0;
                    for (auto it = materialHandles.begin(); it != materialHandles.end(); it++, index++) {
                        if(index % columnCount == 0) {
                            ImGui::TableNextRow();
                        }
                        ImGui::TableSetColumnIndex(index % columnCount);

                        if(auto material = it->second.lock()) {
                            std::shared_ptr<TextureHandle> handle = nullptr;
                            std::int64_t textureID = -1;

                            switch(textureType) {
                                case 0:
                                    handle = material->albedo;
                                    break;

                                case 1:
                                    handle = material->normalMap;
                                    break;

                                case 2:
                                    handle = material->metallicRoughness;
                                    break;

                                case 3:
                                    handle = material->emissive;
                                    break;
                            }

                            auto texture = handle
                                    ? handle->texture
                                    : GetRenderer().getDefaultImage();

                            if(handle) {
                                textureID = handle->getSlot();
                            }

                            ImGui::Text("Slot %d", material->getSlot());
                            ImGui::Text("Texture ID: %lld", textureID);
                            ImGui::Image(texture->getImguiID(), ImVec2(128, 128));
                        } else {
                            ImGui::Text("Free slot");
                        }
                    }

                    ImGui::EndTable();
                }
            }

            if(!isOpen) {
                ShowDebug.setValue(false);
            }
            ImGui::End();
        }

        if(materialHandles.getRequiredStorageCount() >= materialBufferSize) {
            WaitDeviceIdle();
            reallocateMaterialBuffer(Carrot::Math::nextPowerOf2(materialHandles.getRequiredStorageCount()));
        }

        auto updateType = [&](auto registry) {
            for(auto& [slot, handlePtr] : registry) {
                if(auto handle = handlePtr.lock()) {
                    handle->updateHandle(renderContext);
                }
            }
        };
        updateType(materialHandles);
        updateType(textureHandles);

        updateDescriptorSets(renderContext);
    }

    void MaterialSystem::bind(const Context& renderContext, vk::CommandBuffer& cmds, std::uint32_t index, vk::PipelineLayout pipelineLayout, vk::PipelineBindPoint bindPoint) {
        cmds.bindDescriptorSets(bindPoint, pipelineLayout, index, {descriptorSets[renderContext.swapchainIndex]}, {});
    }

    void MaterialSystem::reallocateMaterialBuffer(std::uint32_t materialCount) {
        materialBufferSize = std::max(materialCount, DefaultMaterialBufferSize);
        materialBuffer = GetResourceAllocator().allocateDedicatedBuffer(
                sizeof(MaterialData) * materialBufferSize,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        );
        materialBuffer->name("Materials");
        materialDataPtr = materialBuffer->map<MaterialData>();
        for (std::size_t i = 0; i < materialBufferSize; i++) {
            materialDataPtr[i] = {};
            if(invalidTextureHandle) {
                materialDataPtr[i].albedo = invalidTextureHandle->getSlot();
                materialDataPtr[i].normalMap = invalidTextureHandle->getSlot();
                materialDataPtr[i].emissive = invalidTextureHandle->getSlot();
                materialDataPtr[i].metallicRoughness = invalidTextureHandle->getSlot();
            } else {
                materialDataPtr[i].albedo = 0;
                materialDataPtr[i].normalMap = 0;
                materialDataPtr[i].emissive = 0;
                materialDataPtr[i].metallicRoughness = 0;
            }
        }
        descriptorNeedsUpdate = std::vector<bool>(descriptorSets.size(), true);
    }

    MaterialData* MaterialSystem::getData(MaterialHandle& handle) {
        verify(handle.getSlot() < materialBufferSize, "out of bounds handle!");
        return &materialDataPtr[handle.getSlot()];
    }

    std::shared_ptr<MaterialHandle> MaterialSystem::createMaterialHandle() {
        Async::LockGuard l { accessLock };
        auto ptr = materialHandles.create(std::ref(*this));
        return ptr;
    }

    std::shared_ptr<TextureHandle> MaterialSystem::createTextureHandle(Texture::Ref texture) {
        Async::LockGuard l { accessLock };
        auto ptr = textureHandles.create(std::ref(*this));
        ptr->texture = texture;
        return ptr;
    }

    void MaterialSystem::reallocateDescriptorSets() {
        GetVulkanDevice().resetDescriptorPool(*descriptorSetPool);
        std::vector<vk::DescriptorSetLayout> layouts{GetEngine().getSwapchainImageCount(), *descriptorSetLayout};
        descriptorSets = GetVulkanDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *descriptorSetPool,
                .descriptorSetCount = GetEngine().getSwapchainImageCount(),
                .pSetLayouts = layouts.data(),
        });
        descriptorNeedsUpdate = std::vector<bool>(descriptorSets.size(), true);
    }

    vk::ImageView MaterialSystem::getBoundImageView(std::uint32_t index, const Carrot::Render::Context& renderContext) {
        auto& boundThisSwapchainFrame = boundTextures[renderContext.swapchainIndex];
        auto it = boundThisSwapchainFrame.find(index);
        if(it != boundThisSwapchainFrame.end())
            return it->second;
        return GetRenderer().getDefaultImage()->getView();
    }

    void MaterialSystem::onSwapchainImageCountChange(size_t newCount) {
        reallocateDescriptorSets();
        boundTextures.resize(newCount);
    }

    void MaterialSystem::onSwapchainSizeChange(int newWidth, int newHeight) {
        // no-op
    }
}
