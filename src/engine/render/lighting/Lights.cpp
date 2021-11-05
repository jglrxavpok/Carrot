//
// Created by jglrxavpok on 05/11/2021.
//

#include "Lights.h"

#include <utility>
#include "engine/Engine.h"
#include "engine/render/resources/ResourceAllocator.h"
#include "engine/utils/Macros.h"

namespace Carrot::Render {
    LightHandle::LightHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, Lighting& system): WeakPoolHandle::WeakPoolHandle(index, std::move(destructor)), lightingSystem(system) {}

    void LightHandle::updateHandle(const Carrot::Render::Context& renderContext) {
        auto data = lightingSystem.getLightData(*this);
        data = light;
    }

    Lighting::Lighting() {
        reallocateBuffer(DefaultLightBufferSize);
    }

    std::shared_ptr<LightHandle> Lighting::create() {
        auto ptr = lightHandles.create(std::ref(*this));
        if(lightHandles.size() >= lightBufferSize) {
            reallocateBuffer(lightBufferSize*2);
        }
        return ptr;
    }

    void Lighting::reallocateBuffer(std::uint32_t lightCount) {
        lightBufferSize = std::max(lightCount, DefaultLightBufferSize);
        lightBuffer = GetResourceAllocator().allocateDedicatedBuffer(
                sizeof(Data) + lightBufferSize * sizeof(Light),
                vk::BufferUsageFlagBits::eStorageBuffer,
                vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible
        );
        data = lightBuffer->map<Data>();
    }

    void Lighting::onFrame(const Context& renderContext) {
        lightHandles.erase(std::find_if(WHOLE_CONTAINER(lightHandles), [](auto handlePtr) { return handlePtr.second.expired(); }), lightHandles.end());
        data->lightCount = lightHandles.size();
        data->ambient = ambientColor;
        for(auto& [slot, handlePtr] : lightHandles) {
            if(auto handle = handlePtr.lock()) {
                handle->updateHandle(renderContext);
            }
        }
    }
}
