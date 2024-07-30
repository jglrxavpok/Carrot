//
// Created by jglrxavpok on 05/11/2021.
//

#include "Lights.h"

#include <utility>
#include <engine/vulkan/VulkanDefines.h>

#include "engine/Engine.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/utils/Macros.h"
#include "engine/console/RuntimeOption.hpp"
#include "core/math/BasicFunctions.h"

static Carrot::RuntimeOption DebugFogConfig("Engine/Fog config", false);

namespace Carrot::Render {
    static const std::uint32_t BindingCount = 2;

    Light::Light() {
        point.position = glm::vec3{0};
        point.constantAttenuation = 1.0f;
        point.linearAttenuation = 0.09f;
        point.quadraticAttenuation = 0.032f;
    }

    LightHandle::LightHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, Lighting& system): WeakPoolHandle::WeakPoolHandle(index, std::move(destructor)), lightingSystem(system) {}

    void LightHandle::updateHandle(const Carrot::Render::Context& renderContext) {
        auto& data = lightingSystem.getLightData(*this);
        data = light;
    }

    LightHandle::~LightHandle() {
        auto& data = lightingSystem.getLightData(*this);
        data.flags = LightFlags::None;
    }

    Lighting::Lighting() {
        reallocateBuffers(DefaultLightBufferSize);
        vk::ShaderStageFlags stageFlags = Carrot::AllVkStages;
        std::array<vk::DescriptorSetLayoutBinding, BindingCount> bindings = {
                // Light Buffer
                vk::DescriptorSetLayoutBinding {
                        .binding = 0,
                        .descriptorType = vk::DescriptorType::eStorageBuffer,
                        .descriptorCount = 1,
                        .stageFlags = stageFlags
                },

                // Active Lights Buffer
                vk::DescriptorSetLayoutBinding {
                        .binding = 1,
                        .descriptorType = vk::DescriptorType::eStorageBuffer,
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
                        .type = vk::DescriptorType::eStorageBuffer,
                        .descriptorCount = GetEngine().getSwapchainImageCount(),
                },
        };
        descriptorSetPool = GetVulkanDevice().createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo {
                .flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind | vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                .maxSets = GetEngine().getSwapchainImageCount(),
                .poolSizeCount = static_cast<std::uint32_t>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
        });

        reallocateDescriptorSets();
    }

    std::shared_ptr<LightHandle> Lighting::create() {
        auto ptr = lightHandles.create(std::ref(*this));
        if(lightHandles.getRequiredStorageCount() > lightBufferSize) {
            reallocateBuffers(Carrot::Math::nextPowerOf2(lightHandles.size()));
        }
        return ptr;
    }

    void Lighting::reallocateBuffers(std::uint32_t lightCount) {
        lightBufferSize = std::max(lightCount, DefaultLightBufferSize);
        lightBuffer = GetResourceAllocator().allocateDedicatedBuffer(
                sizeof(Data) + lightBufferSize * sizeof(Light),
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        );
        data = lightBuffer->map<Data>();

        // TODO: device local buffer?
        activeLightsBuffer = GetResourceAllocator().allocateDedicatedBuffer(
                sizeof(Data) + lightBufferSize * sizeof(std::uint32_t),
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        );
        activeLightsData = activeLightsBuffer->map<ActiveLightsData>();

        descriptorNeedsUpdate = std::vector<bool>(descriptorSets.size(), true);
    }

    void Lighting::bind(const Context& renderContext, vk::CommandBuffer& cmds, std::uint32_t index, vk::PipelineLayout pipelineLayout, vk::PipelineBindPoint bindPoint) {
        cmds.bindDescriptorSets(bindPoint, pipelineLayout, index, {descriptorSets[renderContext.swapchainIndex]}, {});
    }

    void Lighting::beginFrame(const Context& renderContext) {
        if(DebugFogConfig) {
            bool open = true;
            if(ImGui::Begin("Fog config", &open)) {
                ImGui::DragFloat("Distance", &fogDistance);
                ImGui::DragFloat("Depth", &fogDepth);
                float color[3] = { fogColor.x, fogColor.y, fogColor.z };
                if(ImGui::DragFloat3("Depth", color)) {
                    fogColor = { color[0], color[1], color[2] };
                }
            }
            ImGui::End();

            if(!open) {
                DebugFogConfig.setValue(false);
            }
        }

        lightHandles.erase(std::find_if(WHOLE_CONTAINER(lightHandles), [](auto& handlePtr) { return handlePtr.second.expired(); }), lightHandles.end());
        data->lightCount = lightBufferSize;
        data->ambient = ambientColor;
        data->fogColor = fogColor;
        data->fogDepth = fogDepth;
        data->fogDistance = fogDistance;

        std::uint32_t activeCount = 0;
        for(auto& [slot, handlePtr] : lightHandles) {
            if(auto handle = handlePtr.lock()) {
                handle->updateHandle(renderContext);
                if((handle->light.flags & LightFlags::Enabled) != LightFlags::None) {
                    activeLightsData->indices[activeCount] = slot;
                    activeCount++;
                }
            }
        }

        activeLightsData->count = activeCount;

        if(descriptorNeedsUpdate[renderContext.swapchainIndex]) {
            auto& set = descriptorSets[renderContext.swapchainIndex];
            auto lightBufferInfo = lightBuffer->getWholeView().asBufferInfo();
            auto activeLightsInfo = activeLightsBuffer->getWholeView().asBufferInfo();
            std::array<vk::WriteDescriptorSet, BindingCount> writes = {
                    // Lights buffer
                    vk::WriteDescriptorSet {
                            .dstSet = set,
                            .dstBinding = 0,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo = &lightBufferInfo,
                    },

                    // Active lights buffer
                    vk::WriteDescriptorSet {
                            .dstSet = set,
                            .dstBinding = 1,
                            .descriptorCount = 1,
                            .descriptorType = vk::DescriptorType::eStorageBuffer,
                            .pBufferInfo = &activeLightsInfo,
                    },
            };
            GetVulkanDevice().updateDescriptorSets(writes, {});
            descriptorNeedsUpdate[renderContext.swapchainIndex] = false;
        }
    }

    void Lighting::reallocateDescriptorSets() {
        std::vector<vk::DescriptorSetLayout> layouts{GetEngine().getSwapchainImageCount(), *descriptorSetLayout};
        descriptorSets = GetVulkanDevice().allocateDescriptorSets(vk::DescriptorSetAllocateInfo {
                .descriptorPool = *descriptorSetPool,
                .descriptorSetCount = GetEngine().getSwapchainImageCount(),
                .pSetLayouts = layouts.data(),
        });
        descriptorNeedsUpdate = std::vector<bool>(descriptorSets.size(), true);
    }

    void Lighting::onSwapchainImageCountChange(size_t newCount) {
        reallocateDescriptorSets();
    }

    void Lighting::onSwapchainSizeChange(Carrot::Window& window, int newWidth, int newHeight) {
        // no-op
    }

    LightType Light::fromString(std::string_view str) {
        if(_stricmp(str.data(), "point") == 0) {
            return LightType::Point;
        }
        if(_stricmp(str.data(), "directional") == 0) {
            return LightType::Directional;
        }
        if(_stricmp(str.data(), "spot") == 0) {
            return LightType::Spot;
        }
        verify(false, "Unknown light type!");
    }

    const char* Light::nameOf(const LightType& type) {
        switch(type) {
            case LightType::Point:
                return "Point";

            case LightType::Directional:
                return "Directional";

            case LightType::Spot:
                return "Spot";
        }
        verify(false, "Unknown light type!");
    }
}
