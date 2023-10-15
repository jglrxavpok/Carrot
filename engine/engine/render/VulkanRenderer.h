//
// Created by jglrxavpok on 13/03/2021.
//

#pragma once

#include <memory>
#include <map>
#include <source_location>
#include <functional>
#include "engine/vulkan/VulkanDriver.h"
#include "engine/render/resources/PerFrame.h"
#include "engine/render/resources/Pipeline.h"
#include "engine/render/resources/Texture.h"
#include "engine/vulkan/SwapchainAware.h"
#include "engine/render/RenderContext.h"
#include "engine/render/RenderGraph.h"
#include "engine/render/MaterialSystem.h"
#include "engine/render/lighting/Lights.h"
#include "engine/render/RenderPacket.h"
#include "engine/render/resources/SingleFrameStackGPUAllocator.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_glfw.h"
#include "RenderPacketContainer.h"
#include "GBufferDrawData.h"
#include <core/async/Coroutines.hpp>
#include <core/async/Locks.h>
#include <core/async/ParallelMap.hpp>

namespace sol {
    class state;
}

namespace Carrot {
    struct BindingKey {
        Carrot::Pipeline& pipeline;
        std::size_t swapchainIndex;
        std::uint32_t setID;
        std::uint32_t bindingID;

        bool operator==(const BindingKey& other) const {
            return &pipeline == &other.pipeline && swapchainIndex == other.swapchainIndex && setID == other.setID && bindingID == other.bindingID;
        }
    };

    struct ImageBindingKey: public BindingKey {
        vk::ImageLayout layout = vk::ImageLayout::eUndefined;

        bool operator==(const ImageBindingKey& other) const {
            return &pipeline == &other.pipeline && swapchainIndex == other.swapchainIndex && setID == other.setID && bindingID == other.bindingID
            && layout == other.layout;
        }
    };
};

namespace std {
    template<>
    struct hash<Carrot::BindingKey> {
        std::size_t operator()(const Carrot::BindingKey& key) const {
            const std::size_t prime = 31;

            std::size_t hash = static_cast<std::size_t>((std::uint64_t)&key.pipeline);
            hash = key.swapchainIndex + hash * prime;
            hash = key.setID + hash * prime;
            hash = key.bindingID + hash * prime;
            return hash;
        }
    };

    template<>
    struct hash<Carrot::ImageBindingKey> {
        std::size_t operator()(const Carrot::ImageBindingKey& key) const {
            const std::size_t prime = 31;

            std::size_t hash = static_cast<std::size_t>((std::uint64_t)&key.pipeline);
            hash = key.swapchainIndex + hash * prime;
            hash = key.setID + hash * prime;
            hash = key.bindingID + hash * prime;
            hash = static_cast<std::size_t>(key.layout) + hash * prime;
            return hash;
        }
    };
}

namespace Carrot {
    class GBuffer;
    class ASBuilder;
    class AccelerationStructure;
    class RayTracer;
    class Mesh;
    class Engine;
    class BufferView;
    class Model;

    namespace Render {
        class Font;
    }

    using CommandBufferConsumer = std::function<void(vk::CommandBuffer&)>;

    class VulkanRenderer: public SwapchainAware {
    public:
        using DeferredCarrotBufferDestruction = DeferredDestruction<Carrot::Buffer*>;

        static constexpr std::uint32_t MaxCameras = 20; // used to determine descriptor set pool size
        static constexpr std::uint32_t MaxViewports = 32; // used to determine descriptor set pool size
        static constexpr double BlinkDuration = 0.100f;

        explicit VulkanRenderer(VulkanDriver& driver, Configuration config);
        ~VulkanRenderer();

        /// Init everything that is not immediately needed during construction. Allows to initialise resources during a loading screen
        void lateInit();

        void recreateDescriptorPools(std::size_t frameCount);

    public: // resource creation
        /// Gets or creates the pipeline with the given name (see resources/pipelines).
        /// Instance offset can be used to force the engine to create a new instance. (Can be used for different blit pipelines, each with a different texture)
        std::shared_ptr<Pipeline> getOrCreatePipeline(const std::string& name, std::uint64_t instanceOffset = 0);
        std::shared_ptr<Pipeline> getOrCreatePipelineFullPath(const std::string& name, std::uint64_t instanceOffset = 0);

        /// Gets or creates the pipeline with the given name (see resources/pipelines).
        /// Different render passes can be used to force the engine to create a new instance. (Can be used for different blit pipelines, each with a different texture)
        std::shared_ptr<Pipeline> getOrCreateRenderPassSpecificPipeline(const std::string& name, const vk::RenderPass& pass);

        std::shared_ptr<Render::Texture> getOrCreateTexture(const std::string& textureName);

        std::shared_ptr<Render::Font> getOrCreateFront(const Carrot::IO::Resource& from);

    public:
        void beforeFrameCommand(const CommandBufferConsumer& command);
        void afterFrameCommand(const CommandBufferConsumer& command);

        /// Call at start of frame (sets ups ImGui stuff)
        void newFrame();
        void preFrame(const Render::Context& renderContext);
        void postFrame();

    public:
        std::uint32_t getFrameCount() const { return frameCount; }
        std::size_t getSwapchainImageCount() { return driver.getSwapchainImageCount(); };
        VulkanDriver& getVulkanDriver() { return driver; };

        ASBuilder& getASBuilder() { return *asBuilder; };
        RayTracer& getRayTracer() { return *raytracer; };

        GBuffer& getGBuffer() { return *gBuffer; };

        vk::Device& getLogicalDevice() { return driver.getLogicalDevice(); };

        Mesh& getFullscreenQuad() { return *fullscreenQuad; }

        const Configuration& getConfiguration() { return config; }

        Engine& getEngine();

        Render::MaterialSystem& getMaterialSystem();

        const Render::MaterialHandle& getWhiteMaterial() const { return *whiteMaterial; }

        Render::Lighting& getLighting() { return lighting; }

    public:
        void onSwapchainSizeChange(int newWidth, int newHeight) override;

        void onSwapchainImageCountChange(std::size_t newCount) override;

        void beginFrame(const Carrot::Render::Context& renderContext);
        void onFrame(const Carrot::Render::Context& renderContext);
        void beforeRecord(const Carrot::Render::Context& renderContext);

    public:
        void initImGuiPass(const vk::RenderPass& renderPass);
        Render::Pass<Carrot::Render::PassData::ImGui>& addImGuiPass(Render::GraphBuilder& graph);

    public:
        // Camera => Camera matrices
        // Debug => Anything can that be used for debugging shaders
        // Viewport => Per-viewport data
        // Per-Draw => Per-draw call data, used with indirect indexed commands

        const vk::DescriptorSetLayout& getCameraDescriptorSetLayout() const;
        const vk::DescriptorSetLayout& getDebugDescriptorSetLayout() const;
        const vk::DescriptorSetLayout& getViewportDescriptorSetLayout() const;
        const vk::DescriptorSetLayout& getPerDrawDescriptorSetLayout() const;
        const vk::DescriptorSet& getDebugDescriptorSet(const Render::Context& renderContext) const;
        const vk::DescriptorSet& getPerDrawDescriptorSet(const Render::Context& renderContext) const;

        std::vector<vk::DescriptorSet> createDescriptorSetForCamera(const std::vector<Carrot::BufferView>& uniformBuffers);
        void destroyCameraDescriptorSets(const std::vector<vk::DescriptorSet>& sets);

        std::vector<vk::DescriptorSet> createDescriptorSetForViewport(const std::vector<Carrot::BufferView>& uniformBuffers);
        void destroyViewportDescriptorSets(const std::vector<vk::DescriptorSet>& sets);

        void deferDestroy(const std::string& name, Carrot::Buffer* resource);

    public:
        void bindSampler(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, const vk::Sampler& samplerToBind, std::uint32_t setID, std::uint32_t bindingID);
        void bindTexture(Pipeline& pipeline, const Render::Context& data, const Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t arrayIndex = 0);
        void bindTexture(Pipeline& pipeline, const Render::Context& data, const Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::Sampler sampler, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t arrayIndex = 0, vk::ImageLayout textureLayout = vk::ImageLayout::eShaderReadOnlyOptimal);
        void bindStorageImage(Pipeline& pipeline, const Render::Context& data, const Render::Texture& textureToBind, std::uint32_t setID, std::uint32_t bindingID, vk::ImageAspectFlags aspect = vk::ImageAspectFlagBits::eColor, vk::ImageViewType viewType = vk::ImageViewType::e2D, std::uint32_t arrayIndex = 0, vk::ImageLayout textureLayout = vk::ImageLayout::eShaderReadOnlyOptimal);
        void bindAccelerationStructure(Pipeline& pipeline, const Render::Context& data, AccelerationStructure& as, std::uint32_t setID, std::uint32_t bindingID);
        void bindBuffer(Pipeline& pipeline, const Render::Context& data, const BufferView& view, std::uint32_t setID, std::uint32_t bindingID);
        void bindUniformBuffer(Pipeline& pipeline, const Render::Context& data, const BufferView& view, std::uint32_t setID, std::uint32_t bindingID);

        void unbindSampler(Carrot::Pipeline& pipeline, const Carrot::Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID);
        void unbindTexture(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID, std::uint32_t arrayIndex = 0);
        void unbindStorageImage(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID, std::uint32_t arrayIndex = 0);
        void unbindAccelerationStructure(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID);
        void unbindBuffer(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID);
        void unbindUniformBuffer(Pipeline& pipeline, const Render::Context& frame, std::uint32_t setID, std::uint32_t bindingID);

        template<typename ConstantBlock>
        void pushConstantBlock(std::string_view pushName, const Carrot::Pipeline& pipeline, const Carrot::Render::Context& context, vk::ShaderStageFlags stageFlags, vk::CommandBuffer& cmds, const ConstantBlock& block);

        void blit(Carrot::Render::Texture& source, Carrot::Render::Texture& destination, vk::CommandBuffer& cmds, vk::Offset3D srcOffset = {}, vk::Offset3D dstOffset = {});

        void fullscreenBlit(const vk::RenderPass& pass, const Carrot::Render::Context& frame, Carrot::Render::Texture& textureToBlit, Carrot::Render::Texture& targetTexture, vk::CommandBuffer& cmds);

    public:
        std::shared_ptr<Carrot::Model> getUnitSphere();
        std::shared_ptr<Carrot::Model> getUnitCapsule();
        std::shared_ptr<Carrot::Model> getUnitCube();

    public:
        /// Reference is valid only for the current frame
        Render::Packet& makeRenderPacket(Render::PassEnum pass, Render::Viewport& viewport, std::source_location location = std::source_location::current());

        void renderSphere(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderCapsule(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, float height, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderCuboid(const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderWireframeSphere(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderWireframeCapsule(const Carrot::Render::Context& renderContext, const glm::mat4& transform, float radius, float height, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderWireframeCuboid(const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec3& halfExtents, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());

        /**
         * Render 3D arrow pointing towards -Z, with origin on tip of arrow
         */
        void render3DArrow(const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());

        /// Must be called between startFrame and endFrame. Otherwise it is not safe.
        void render(const Render::Packet& packet);

    public:
        /**
         * Shows a blackscreen for at minimum one frame, and for a duration of VulkanRenderer::BlinkDuration.
         */
        void blink();

    public:
        /// Returns a portion of buffer that can be used for the current frame
        Carrot::BufferView getSingleFrameBuffer(vk::DeviceSize size);
        Carrot::BufferView getInstanceBuffer(vk::DeviceSize size);
        const Carrot::BufferView getNullBufferInfo() const;

        void recordPassPackets(Carrot::Render::PassEnum passEnum, vk::RenderPass pass, Render::Context renderContext, vk::CommandBuffer& commands);
        void recordOpaqueGBufferPass(vk::RenderPass pass, Render::Context renderContext, vk::CommandBuffer& commands);
        void recordTransparentGBufferPass(vk::RenderPass pass, Render::Context renderContext, vk::CommandBuffer& commands);

    public:
        Render::Texture::Ref getDefaultImage();
        Render::Texture::Ref getBlackCubeMapTexture();

    public:
        struct ThreadPackets {
            std::vector<Carrot::Render::Packet> unsorted;
            std::vector<Carrot::Render::Packet> sorted;
        };

        /// Must be called before any call to VulkanRenderer::render in a given thread. Allows for fast rendering submission
        void makeCurrentThreadRenderCapable();

    public:
        static void registerUsertype(sol::state& destination);

    private:
        VulkanDriver& driver;
        Configuration config;

        Async::Counter mustBeDoneByNextFrameCounter; // use for work that needs to be done before the next call to beginFrame

        SingleFrameStackGPUAllocator singleFrameAllocator;
        std::unique_ptr<Carrot::Buffer> nullBuffer;
        vk::DescriptorBufferInfo nullBufferInfo;

        vk::UniqueDescriptorSetLayout cameraDescriptorSetLayout{};
        vk::UniqueDescriptorPool cameraDescriptorPool{};
        vk::UniqueDescriptorSetLayout viewportDescriptorSetLayout{};
        vk::UniqueDescriptorPool viewportDescriptorPool{};
        vk::UniqueDescriptorSetLayout debugDescriptorSetLayout{};
        vk::UniqueDescriptorPool debugDescriptorPool{};

        vk::UniqueDescriptorSetLayout perDrawDescriptorSetLayout{};
        vk::UniqueDescriptorPool perDrawDescriptorPool{};

        Render::PerFrame<std::unique_ptr<Carrot::Buffer>> debugBuffers;
        Render::PerFrame<vk::DescriptorSet> debugDescriptorSets;

        // dynamic SSBO
        Render::PerFrame<std::unique_ptr<Carrot::Buffer>> perDrawOffsetBuffers;
        Render::PerFrame<std::unique_ptr<Carrot::Buffer>> perDrawBuffers;
        Render::PerFrame<vk::DescriptorSet> perDrawDescriptorSets;

        // Counts of the vectors below once they will be filled. Used to preallocate per-draw buffers before recording RenderPackets
        std::atomic<std::uint32_t> perDrawElementCount{0};
        std::atomic<std::uint32_t> perDrawOffsetCount{0};
        std::vector<GBufferDrawData> perDrawData;
        std::vector<std::uint32_t> perDrawOffsets;

        std::unique_ptr<ASBuilder> asBuilder = nullptr;

        Render::MaterialSystem materialSystem;
        Render::Lighting lighting;

        vk::UniqueDescriptorPool imguiDescriptorPool{};

        std::unique_ptr<RayTracer> raytracer = nullptr;
        std::unique_ptr<GBuffer> gBuffer = nullptr;
        Render::PerFrame<std::unique_ptr<Carrot::Buffer>> forwardRenderingFrameInfo;

        std::list<CommandBufferConsumer> beforeFrameCommands;
        std::list<CommandBufferConsumer> afterFrameCommands;

        ImGui_ImplVulkan_InitInfo imguiInitInfo;
        bool imguiIsInitialized = false;
        std::unique_ptr<Carrot::Mesh> fullscreenQuad = nullptr;
        std::list<std::unique_ptr<std::uint8_t[]>> pushConstants;

        std::unordered_map<ImageBindingKey, vk::Image> boundTextures;
        std::unordered_map<ImageBindingKey, vk::Image> boundStorageImages;
        std::unordered_map<BindingKey, vk::AccelerationStructureKHR> boundAS;
        std::unordered_map<BindingKey, vk::Sampler> boundSamplers;
        std::unordered_map<BindingKey, vk::DescriptorBufferInfo> boundBuffers;

        Async::SpinLock threadRegistrationLock;

        Async::ParallelMap<std::thread::id, ThreadPackets> threadRenderPackets{};
        Async::ParallelMap<std::thread::id, std::unique_ptr<Render::PacketContainer>> perThreadPacketStorage{};
        std::vector<Render::Packet> renderPackets;
        std::vector<Render::Packet> preparedRenderPackets;

        std::shared_ptr<Carrot::Model> unitSphereModel;
        std::shared_ptr<Carrot::Model> unitCubeModel;
        std::shared_ptr<Carrot::Model> unitCapsuleModel;
        std::shared_ptr<Carrot::Model> debugArrowModel;
        std::shared_ptr<Carrot::Render::MaterialHandle> whiteMaterial;
        std::shared_ptr<Carrot::Pipeline> wireframeGBufferPipeline;
        std::shared_ptr<Carrot::Pipeline> gBufferPipeline;
        std::shared_ptr<Carrot::Render::Texture> defaultTexture;
        std::shared_ptr<Carrot::Render::Texture> blackCubeMapTexture;

        std::list<DeferredCarrotBufferDestruction> deferredCarrotBufferDestructions;

    private:
        bool hasBlinked = false;
        double blinkTime = -1.0;

        std::uint32_t frameCount = 0;

    private:
        void createCameraSetResources();
        void createViewportSetResources();

        void createPerDrawSetResources();
        void preallocatePerDrawBuffers(const Carrot::Render::Context& renderContext);
        void updatePerDrawBuffers(const Carrot::Render::Context& renderContext);

        void createDebugSetResources();
        void createDefaultResources();

        void createUIResources();

        /// Create the pipeline responsible for lighting via the gbuffer
        void createGBuffer();

        /// Create the object responsible of raytracing operations and subpasses
        void createRayTracer();

        void initImGui();

    private:
        std::span<const Render::Packet> getRenderPackets(Carrot::Render::Viewport* viewport, Carrot::Render::PassEnum pass) const;
        void collectRenderPackets();
        void sortRenderPackets(std::vector<Carrot::Render::Packet>& packets);
        void mergeRenderPackets(const std::vector<Carrot::Render::Packet>& inputPackets, std::vector<Carrot::Render::Packet>& outputPackets);

        // debug
        void renderModel(const Carrot::Model& model, const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());
        void renderWireframeModel(const Carrot::Model& model, const Carrot::Render::Context& renderContext, const glm::mat4& transform, const glm::vec4& color, const Carrot::UUID& objectID = Carrot::UUID::null());

        /**
         * Uploads a list of drawdata to the per_draw buffer and returns the dynamic offset to use when binding the descriptor set
         * @param drawData
         * @return
         */
        std::uint32_t uploadPerDrawData(const std::span<GBufferDrawData>& drawData);

        friend class Render::Packet;
    };
}

#include "VulkanRenderer.ipp"
