//
// Created by jglrxavpok on 06/03/2021.
//

#pragma once
#include <core/UniquePtr.hpp>

#include "core/utils/WeakPool.hpp"
#include "engine/render/RenderContext.h"
#include <glm/glm.hpp>

namespace Carrot::Render {
    using bool32 = uint32_t;

    enum class LightType: std::uint8_t {
        Point,
        Directional,
        Spot,
    };

    enum class LightFlags : std::uint8_t {
        None = 0,
        Enabled = 1 << 0,
    };

    struct Light {
        glm::vec3 color{1.0f};
        LightFlags flags = LightFlags::None;

        LightType type = LightType::Point;
        float intensity = 1.0f;

        union {
            struct PointLight {
                glm::vec3 position;
                float constantAttenuation;
                float linearAttenuation;
                float quadraticAttenuation;
            } point;

            struct SpotLight {
                glm::vec3 position;
                glm::vec3 direction;
                float cutoffCosAngle;
                float outerCutoffCosAngle;
            } spot;

            struct DirectionalLight {
                glm::vec3 direction;
            } directional;
        };

        Light();

        static LightType fromString(std::string_view str);
        static const char* nameOf(const LightType& type);
    };

    static Render::LightFlags operator&(Render::LightFlags lhs, Render::LightFlags rhs) {
        using IntType = std::underlying_type_t<LightFlags>;
        return static_cast<Render::LightFlags>(static_cast<IntType>(lhs) & static_cast<IntType>(rhs));
    }

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

    public:
        void bind(const Context& renderContext, vk::CommandBuffer& cmds, std::uint32_t index, vk::PipelineLayout pipelineLayout, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics);
        void beginFrame(const Carrot::Render::Context& renderContext);

    public:
        const vk::DescriptorSetLayout& getDescriptorSetLayout() const { return *descriptorSetLayout; }
        vk::DescriptorSet getDescriptorSet(const Render::Context& context) { return descriptorSets[context.swapchainIndex]; }

    public:
        void onSwapchainImageCountChange(size_t newCount) override;
        void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override;

    private:
        Light& getLightData(LightHandle& handle) {
            auto* lightPtr = data->lights;
            return lightPtr[handle.getSlot()];
        }

        void reallocateBuffers(std::uint32_t lightCount);
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
            glm::vec3 ambient {1.0f};
            std::uint32_t lightCount;

            // Color of fog
            glm::vec3 fogColor { 0.0 };
            float fogDistance = std::numeric_limits<float>::infinity();

            // Distance over which fog fades out
            float fogDepth = 1.0f;

            Light lights[];
        };

        struct ActiveLightsData {
            std::uint32_t count = 0;

            std::uint32_t indices[];
        };

        Data* data = nullptr;
        ActiveLightsData* activeLightsData = nullptr;
        std::size_t lightBufferSize = 0; // in number of lights
        UniquePtr<Carrot::Buffer> lightBuffer = nullptr;
        UniquePtr<Carrot::Buffer> activeLightsBuffer = nullptr;

        // Distance at which fog starts
        float fogDistance = std::numeric_limits<float>::infinity();

        // Distance over which fog fades out
        float fogDepth = 1.0f;

        // Color of fog
        glm::vec3 fogColor { 0.0 };


        friend class LightHandle;
    };
}