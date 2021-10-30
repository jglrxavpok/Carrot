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

namespace Carrot::Render {

    class MaterialSystem;

    /// Data sent to the GPU
//#pragma pack(push, 1)
    struct MaterialData {
        std::uint32_t diffuseTexture;
        //std::uint32_t pad;
    };
//#pragma pack(pop)

    class TextureHandle {
    public:
        Texture::Ref texture;

        /*[[deprecated]] */explicit TextureHandle(MaterialSystem& system, std::uint32_t index);
        ~TextureHandle();

        std::uint32_t getSlot() const { return index; }

    private:
        void updateHandle(const Carrot::Render::Context& renderContext);

        Texture::Ref previousTexture = nullptr;
        MaterialSystem& materialSystem;
        std::uint32_t index;
        friend class MaterialSystem;
    };

    class MaterialHandle {
    public:
        std::shared_ptr<TextureHandle> diffuseTexture;

        /*[[deprecated]] */explicit MaterialHandle(MaterialSystem& system, std::uint32_t index);
        ~MaterialHandle();

        std::uint32_t getSlot() const { return index; }

    private:
        void updateHandle(const Carrot::Render::Context& renderContext);

        MaterialSystem& materialSystem;
        std::uint32_t index;
        friend class MaterialSystem;
    };

    class MaterialSystem: public SwapchainAware {
    public:
        template<typename T>
        using Registry = std::unordered_map<std::uint32_t, std::weak_ptr<T>>;

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
        template<typename T>
        std::shared_ptr<T> create(Registry<T>& registry, std::queue<std::uint32_t>& freeSlots, std::uint32_t& nextID) {
            std::uint32_t slot;
            if(!freeSlots.empty()) {
                slot = freeSlots.back();
                freeSlots.pop();
            } else {
                slot = nextID++;
            }
            auto ptr = std::make_shared<T>(*this, slot);
            registry[slot] = ptr;
            return ptr;
        }

        template<typename T>
        void free(T& handle, Registry<T>& registry, std::queue<std::uint32_t>& freeSlots) {
            registry.erase(handle.index);
            freeSlots.push(handle.index);
        }

        void free(MaterialHandle& handle);
        void free(TextureHandle& handle);

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
        Registry<TextureHandle> textureHandles;
        std::queue<std::uint32_t> freeTextureSlots;
        std::uint32_t nextTextureID = 1;

        std::vector<std::unordered_map<std::uint32_t, vk::ImageView>> boundTextures;

        vk::ImageView getBoundImageView(std::uint32_t index, const Carrot::Render::Context& renderContext);

    private:
        constexpr static std::uint32_t DefaultMaterialBufferSize = 16;
        Registry<MaterialHandle> materialHandles;
        std::queue<std::uint32_t> freeMaterialSlots;
        std::uint32_t nextMaterialID = 1;

        MaterialData* materialDataPtr = nullptr;
        std::size_t materialBufferSize = 0; // in number of materials
        std::unique_ptr<Carrot::Buffer> materialBuffer = nullptr;

    private:
        friend class TextureHandle;
        friend class MaterialHandle;
    };
}
