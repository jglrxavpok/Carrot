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
#include <engine/utils/WeakPool.hpp>

namespace Carrot::Render {

    class MaterialSystem;

    /// Data sent to the GPU
//#pragma pack(push, 1)
    struct MaterialData {
        std::uint32_t diffuseTexture;
        //std::uint32_t pad;
    };
//#pragma pack(pop)

    class TextureHandle: public WeakPoolHandle {
    public:
        Texture::Ref texture;

        /*[[deprecated]] */explicit TextureHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, MaterialSystem& system);

    private:
        void updateHandle(const Carrot::Render::Context& renderContext);

        Texture::Ref previousTexture = nullptr;
        MaterialSystem& materialSystem;
        friend class MaterialSystem;
    };

    class MaterialHandle: public WeakPoolHandle {
    public:
        std::shared_ptr<TextureHandle> diffuseTexture;

        /*[[deprecated]] */explicit MaterialHandle(std::uint32_t index, std::function<void(WeakPoolHandle*)> destructor, MaterialSystem& system);

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
        void onFrame(const Context& renderContext);

    public:
        vk::DescriptorSetLayout getDescriptorSetLayout() const { return *descriptorSetLayout; }

    public:
        void onSwapchainImageCountChange(size_t newCount) override;
        void onSwapchainSizeChange(int newWidth, int newHeight) override;

    private:
        void reallocateDescriptorSets();
        void reallocateMaterialBuffer(std::uint32_t materialCount);
        MaterialData* getData(MaterialHandle& handle);

    private:
        vk::UniqueDescriptorSetLayout descriptorSetLayout{};
        vk::UniqueDescriptorPool descriptorSetPool{};
        std::vector<vk::DescriptorSet> descriptorSets;
        std::vector<bool> descriptorNeedsUpdate;

    private:
        std::vector<std::unordered_map<std::uint32_t, vk::ImageView>> boundTextures;

        WeakPool<TextureHandle> textureHandles;
        vk::ImageView getBoundImageView(std::uint32_t index, const Carrot::Render::Context& renderContext);

    private:
        constexpr static std::uint32_t DefaultMaterialBufferSize = 16;

        WeakPool<MaterialHandle> materialHandles;
        MaterialData* materialDataPtr = nullptr;
        std::size_t materialBufferSize = 0; // in number of materials
        std::unique_ptr<Carrot::Buffer> materialBuffer = nullptr;

    private:
        friend class TextureHandle;
        friend class MaterialHandle;
    };
}
