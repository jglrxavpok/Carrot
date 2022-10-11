//
// Created by jglrxavpok on 24/10/2021.
//

#include "MaterialSystem.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/Engine.h"
#include "engine/console/RuntimeOption.hpp"
#include <core/io/Logging.hpp>

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
        if(diffuseTexture) {
            data->diffuseTexture = diffuseTexture->getSlot();
        } else {
            data->diffuseTexture = materialSystem.whiteTextureHandle->getSlot();
        }
        if(normalMap) {
            data->normalMap = normalMap->getSlot();
        } else {
            data->normalMap = materialSystem.blueTextureHandle->getSlot();
        }
    }

    MaterialHandle::~MaterialHandle() noexcept {
        Carrot::Render::MaterialData* data = materialSystem.getData(*this);
        data->diffuseTexture = materialSystem.invalidMaterialHandle->diffuseTexture->getSlot();
        data->normalMap = materialSystem.invalidMaterialHandle->normalMap->getSlot();
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

        std::unique_ptr<Carrot::Image> blueImage = std::make_unique<Image>(GetVulkanDriver(),
                                                                            vk::Extent3D{.width = 1, .height = 1, .depth = 1},
                                                                            vk::ImageUsageFlagBits::eSampled,
                                                                            vk::Format::eR8G8B8A8Unorm);
        std::uint8_t bluePixel[4] = {0x00, 0x00, 0xFF, 0xFF};
        blueImage->stageUpload(std::span{bluePixel, 4});

        whiteTexture = std::make_shared<Texture>(std::move(whiteImage));
        blackTexture = std::make_shared<Texture>(std::move(blackImage));
        blueTexture = std::make_shared<Texture>(std::move(blueImage));

        invalidTexture = GetRenderer().getOrCreateTexture("invalid_texture_reference.png");
        invalidTextureHandle = createTextureHandle(invalidTexture);
        invalidMaterialHandle = createMaterialHandle();
        invalidMaterialHandle->diffuseTexture = invalidTextureHandle;
        invalidMaterialHandle->normalMap = invalidTextureHandle;

        whiteTextureHandle = createTextureHandle(whiteTexture);
        blackTextureHandle = createTextureHandle(blackTexture);
        blueTextureHandle = createTextureHandle(blueTexture);
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
                                    handle = material->diffuseTexture;
                                    break;

                                case 1:
                                    handle = material->normalMap;
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
            reallocateMaterialBuffer(materialBufferSize*2);
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
        materialCount = std::max(materialCount, DefaultMaterialBufferSize);
        materialBufferSize = materialCount;
        materialBuffer = GetResourceAllocator().allocateDedicatedBuffer(
                sizeof(MaterialData) * materialCount,
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        );
        materialDataPtr = materialBuffer->map<MaterialData>();
        for (std::size_t i = 0; i < materialCount; i++) {
            if(invalidTextureHandle) {
                materialDataPtr[i].diffuseTexture = invalidTextureHandle->getSlot();
                materialDataPtr[i].normalMap = invalidTextureHandle->getSlot();
            } else {
                materialDataPtr[i].diffuseTexture = 0;
                materialDataPtr[i].normalMap = 0;
            }
        }
        descriptorNeedsUpdate = std::vector<bool>(descriptorSets.size(), true);
    }

    MaterialData* MaterialSystem::getData(MaterialHandle& handle) {
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
