//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include "engine/vulkan/includes.h"
#include "core/utils/WeakPool.hpp"
#include "engine/render/RenderContext.h"
#include <glm/glm.hpp>

namespace Carrot::Render {
    using bool32 = uint32_t;

    enum class LightType: std::uint32_t {
        Point,
        Directional,
        Spot,
    };

    struct Light {
        alignas(16) glm::vec3 position{0.0f};
        float intensity = 1.0f;

        alignas(16) glm::vec3 direction{1.0f};
        LightType type = LightType::Point;

        alignas(16) glm::vec3 color{1.0f};
        bool32 enabled = false;

        // point light
        float constantAttenuation = 1.0f;
        float linearAttenuation = 0.09f;
        float quadraticAttenuation = 0.032f;

        // spot light
        float cutoffCosAngle = glm::cos(glm::pi<float>()/7.0f);
        float outerCutoffCosAngle = glm::cos(glm::pi<float>()/8.0f);

        static LightType fromString(std::string_view str);
        static const char* nameOf(LightType type);
    };

    class Lighting;

    class LightHandle: public WeakPoolHandle {
    public:
        Light light;

        /*[[deprecated]] */explicit LightHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, Lighting& system);

        ~LightHandle();

    private:
        void updateHandle(const Carrot::Render::Context& renderContext);

        Lighting& lightingSystem;
        friend class Lighting;
    };

    class Lighting: public SwapchainAware {
    public:
        explicit Lighting();

    public:
        glm::vec3& getAmbientLight() { return ambientColor; }

        std::shared_ptr<LightHandle> create();

        Buffer& getBuffer() const { return *lightBuffer; }

    public:
        void bind(const Context& renderContext, vk::CommandBuffer& cmds, std::uint32_t index, vk::PipelineLayout pipelineLayout, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics);
        void beginFrame(const Carrot::Render::Context& renderContext);

    public:
        const vk::DescriptorSetLayout& getDescriptorSetLayout() const { return *descriptorSetLayout; }
        vk::DescriptorSet getCameraDescriptorSet(const Render::Context& context) { return descriptorSets[context.swapchainIndex]; }

    public:
        void onSwapchainImageCountChange(size_t newCount) override;
        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        Light& getLightData(LightHandle& handle) {
            auto* lightPtr = data->lights;
            return lightPtr[handle.getSlot()];
        }

        void reallocateBuffer(std::uint32_t lightCount);
        void reallocateDescriptorSets();

    private:
        vk::UniqueDescriptorSetLayout descriptorSetLayout{};
        vk::UniqueDescriptorPool descriptorSetPool{};
        std::vector<vk::DescriptorSet> descriptorSets;
        std::vector<bool> descriptorNeedsUpdate;

    private:
        constexpr static std::uint32_t DefaultLightBufferSize = 16;

        WeakPool<LightHandle> lightHandles;
        glm::vec3 ambientColor {1.0f};

        struct Data {
            alignas(alignof(glm::vec4)) glm::vec3 ambient {1.0f};
            std::uint32_t lightCount;

            // Color of fog
            glm::vec3 fogColor { 0.0 };
            float fogDistance = std::numeric_limits<float>::infinity();

            // Distance over which fog fades out
            float fogDepth = 1.0f;

            alignas(alignof(glm::vec4)) Light lights[];
        };

        Data* data = nullptr;
        std::size_t lightBufferSize = 0; // in number of lights
        std::unique_ptr<Carrot::Buffer> lightBuffer = nullptr;

        // Distance at which fog starts
        float fogDistance = std::numeric_limits<float>::infinity();

        // Distance over which fog fades out
        float fogDepth = 1.0f;

        // Color of fog
        glm::vec3 fogColor { 0.0 };


        friend class LightHandle;
    };
}