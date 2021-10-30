//
// Created by jglrxavpok on 24/10/2021.
//

#include "MaterialSystem.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/Engine.h"

namespace Carrot::Render {
    static const std::uint32_t BindingCount = 4;

    TextureHandle::TextureHandle(MaterialSystem& system, std::uint32_t index): materialSystem(system), index(index) {

    }

    void TextureHandle::updateHandle(const Carrot::Render::Context& renderContext) {
        auto& boundTextures = materialSystem.boundTextures[renderContext.swapchainIndex];
        auto imageViewToBind = texture ? texture->getView() : GetRenderer().getDefaultImage()->getView();
        auto it = boundTextures.find(index);

        if(it != boundTextures.end() && it->second == imageViewToBind) {
            return;
        }

        boundTextures[index] = imageViewToBind;
        vk::DescriptorImageInfo imageInfo {
                .sampler = nullptr,
                .imageView = imageViewToBind,
                .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
        };
        vk::WriteDescriptorSet write {
                .dstSet = materialSystem.descriptorSets[renderContext.swapchainIndex],
                .dstBinding = 1,
                .dstArrayElement = index,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eSampledImage,
                .pImageInfo = &imageInfo,
        };
        GetVulkanDevice().updateDescriptorSets(write, {});
    }

    TextureHandle::~TextureHandle() {
        materialSystem.free(*this);
    }

    MaterialHandle::MaterialHandle(MaterialSystem& system, std::uint32_t index): materialSystem(system), index(index) {

    }

    void MaterialHandle::updateHandle(const Carrot::Render::Context& renderContext) {
        auto* data = materialSystem.getData(*this);
        if(diffuseTexture) {
            data->diffuseTexture = diffuseTexture->getSlot();
        }
    }

    MaterialHandle::~MaterialHandle() {
        materialSystem.free(*this);
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
    }

    void MaterialSystem::free(MaterialHandle& handle) {
        free(handle, materialHandles, freeMaterialSlots);
    }

    void MaterialSystem::free(TextureHandle& handle) {
        free(handle, textureHandles, freeTextureSlots);
    }

    void MaterialSystem::onFrame(const Context& renderContext) {
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
            std::erase_if(registry, [](auto handlePtr) { return handlePtr.second.expired(); });
            for(auto& [slot, handlePtr] : registry) {
                if(auto handle = handlePtr.lock()) {
                    handle->updateHandle(renderContext);
                }
            }
        };
        updateType(materialHandles);
        updateType(textureHandles);
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
        return &materialDataPtr[handle.index];
    }

    std::shared_ptr<MaterialHandle> MaterialSystem::createMaterialHandle() {
        auto ptr = create<MaterialHandle>(materialHandles, freeMaterialSlots, nextMaterialID);
        if(materialHandles.size() >= materialBufferSize) {
            reallocateMaterialBuffer(materialBufferSize*2);
        }
        return ptr;
    }

    std::shared_ptr<TextureHandle> MaterialSystem::createTextureHandle(Texture::Ref texture) {
        auto ptr = create<TextureHandle>(textureHandles, freeTextureSlots, nextTextureID);
        ptr->texture = std::move(texture);
        return ptr;
    }

    void MaterialSystem::reallocateDescriptorSets() {
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
