//
// Created by jglrxavpok on 24/10/2021.
//

#include "MaterialSystem.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/Engine.h"

namespace Carrot::Render {
    static const std::uint32_t BindingCount = 4;

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

    MaterialHandle::MaterialHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, MaterialSystem& system): WeakPoolHandle::WeakPoolHandle(index, destructor), materialSystem(system) {

    }

    void MaterialHandle::updateHandle(const Carrot::Render::Context& renderContext) {
        auto* data = materialSystem.getData(*this);
        if(diffuseTexture) {
            data->diffuseTexture = diffuseTexture->getSlot();
        }
        if(normalMap) {
            data->normalMap = normalMap->getSlot();
        }
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
        whiteTextureHandle = createTextureHandle(whiteTexture);
        blackTextureHandle = createTextureHandle(blackTexture);
        blueTextureHandle = createTextureHandle(blueTexture);
    }

    void MaterialSystem::beginFrame(const Context& renderContext) {
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

        auto updateType = [&](auto registry) {
            registry.erase(std::find_if(WHOLE_CONTAINER(registry), [](auto handlePtr) { return handlePtr.second.expired(); }), registry.end());
            for(auto& [slot, handlePtr] : registry) {
                if(auto handle = handlePtr.lock()) {
                    handle->updateHandle(renderContext);
                }
            }
        };
        updateType(materialHandles);
        updateType(textureHandles);

        if(materialHandles.size() >= materialBufferSize) {
            WaitDeviceIdle();
            reallocateMaterialBuffer(materialBufferSize*2);
        }
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
        ptr->texture = std::move(texture);
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
