//
// Created by jglrxavpok on 24/10/2021.
//

#pragma once

#include <cstdint>
#include <memory>
#include <queue>
#include <vector>
#include <unordered_set>
#include "engine/vulkan/includes.h"
#include "engine/render/RenderContext.h"
#include "resources/Texture.h"
#include <core/utils/WeakPool.hpp>
#include <core/async/Locks.h>

namespace Carrot::Render {

    class MaterialSystem;

    /// Data sent to the GPU
    struct MaterialData {
        glm::vec4 baseColor {1.0f};

        glm::vec3 emissiveColor {0.0f};
        std::uint32_t emissive = 0;

        glm::vec2 metallicRoughnessFactor{0.0f};

        std::uint32_t albedo = 0;
        std::uint32_t normalMap = 0;
        std::uint32_t metallicRoughness = 0;
    };

    class TextureHandle: public WeakPoolHandle {
    public:
        Texture::Ref texture;

        /*[[deprecated]] */explicit TextureHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, MaterialSystem& system);

        ~TextureHandle();

    private:
        void updateHandle(const Carrot::Render::Context& renderContext);

        Texture::Ref previousTexture = nullptr;
        MaterialSystem& materialSystem;
        friend class MaterialSystem;
    };

    class MaterialHandle: public WeakPoolHandle {
    public:
        glm::vec4 baseColor {1.0f};

        glm::vec3 emissiveColor {0.0f};
        std::shared_ptr<TextureHandle> emissive;

        float roughnessFactor = 0.0f;
        float metallicFactor = 0.0f;

        std::shared_ptr<TextureHandle> albedo;
        std::shared_ptr<TextureHandle> normalMap;
        std::shared_ptr<TextureHandle> metallicRoughness;

        /*[[deprecated]] */explicit MaterialHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, MaterialSystem& system);

        ~MaterialHandle();

    private:
        void updateHandle(const Carrot::Render::Context& renderContext);

        MaterialSystem& materialSystem;
        friend class MaterialSystem;
    };

    class MaterialSystem: public SwapchainAware {
    public:
        constexpr static std::uint32_t MaxTextures = 1024;

        explicit MaterialSystem();

        std::shared_ptr<TextureHandle> createTextureHandle(Texture::Ref texture);
        std::shared_ptr<MaterialHandle> createMaterialHandle();

        void bind(const Context& renderContext, vk::CommandBuffer& cmds, std::uint32_t index, vk::PipelineLayout pipelineLayout, vk::PipelineBindPoint bindPoint = vk::PipelineBindPoint::eGraphics);
        void beginFrame(const Context& renderContext);

    public:
        const vk::DescriptorSetLayout& getDescriptorSetLayout() const { return *descriptorSetLayout; }
        const std::vector<vk::DescriptorSet>& getDescriptorSets() const { return descriptorSets; }

    public:
        void onSwapchainImageCountChange(size_t newCount) override;
        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        void reallocateDescriptorSets();
        void reallocateMaterialBuffer(std::uint32_t materialCount);
        MaterialData* getData(MaterialHandle& handle);
        void updateDescriptorSets(const Carrot::Render::Context& renderContext);

    public:
        std::shared_ptr<TextureHandle> getWhiteTexture() const { return whiteTextureHandle; }
        std::shared_ptr<TextureHandle> getBlackTexture() const { return blackTextureHandle; }
        std::shared_ptr<TextureHandle> getFlatNormalTexture() const { return flatNormalTextureHandle; }

    private:
        Texture::Ref whiteTexture = nullptr;
        Texture::Ref blackTexture = nullptr;
        Texture::Ref flatNormalTexture = nullptr;

    private:
        vk::UniqueDescriptorSetLayout descriptorSetLayout{};
        vk::UniqueDescriptorPool descriptorSetPool{};
        std::vector<vk::DescriptorSet> descriptorSets;
        std::vector<bool> descriptorNeedsUpdate;

    private:
        Carrot::Render::Texture::Ref invalidTexture = nullptr;
        std::vector<std::unordered_map<std::uint32_t, vk::ImageView>> boundTextures;
        WeakPool<TextureHandle> textureHandles;

        vk::ImageView getBoundImageView(std::uint32_t index, const Carrot::Render::Context& renderContext);

    private:
        constexpr static std::uint32_t DefaultMaterialBufferSize = 16;

        WeakPool<MaterialHandle> materialHandles;
        MaterialData* materialDataPtr = nullptr;
        std::size_t materialBufferSize = 0; // in number of materials
        std::unique_ptr<Carrot::Buffer> materialBuffer = nullptr;
        Async::SpinLock accessLock;

    private: // need to be destroyed before the corresponding WeakPoolHandle
        std::shared_ptr<MaterialHandle> invalidMaterialHandle = nullptr;
        std::shared_ptr<TextureHandle> invalidTextureHandle = nullptr;
        std::shared_ptr<TextureHandle> whiteTextureHandle = nullptr;
        std::shared_ptr<TextureHandle> blackTextureHandle = nullptr;
        std::shared_ptr<TextureHandle> flatNormalTextureHandle = nullptr;


    private:
        friend class TextureHandle;
        friend class MaterialHandle;
    };
}
