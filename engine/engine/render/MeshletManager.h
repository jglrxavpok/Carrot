//
// Created by jglrxavpok on 20/11/2023.
//

#pragma once

#include <core/utils/WeakPool.hpp>
#include <engine/render/InstanceData.h>
#include <core/render/Meshlet.h>
#include <engine/render/resources/Vertex.h>
#include <engine/render/resources/BufferAllocation.h>
#include <engine/render/resources/PerFrame.h>
#include <engine/vulkan/SwapchainAware.h>
#include <engine/render/RenderContext.h>
#include <engine/render/resources/Pipeline.h>

/**
 * The difference between Meshlets and Clusters is that:
 *  Meshlets reference triangles from an original mesh while Clusters hold their own triangle data
 */

namespace Carrot {
    class VulkanRenderer;
}

namespace Carrot::Render {
    class MeshletManager;
    class Viewport;

    /**
     * Sent as-is to the GPU
     */
    struct Cluster {
        vk::DeviceAddress vertexBufferAddress = (vk::DeviceAddress)-1;
        vk::DeviceAddress indexBufferAddress = (vk::DeviceAddress)-1;
        std::uint8_t triangleCount;
        std::uint32_t lod;
    };

    /**
     * Handle to a set of meshlets
     */
    struct MeshletsTemplate: public WeakPoolHandle {
        const std::size_t firstCluster;
        const std::vector<Cluster> clusters;
        const Carrot::BufferAllocation vertexData;
        const Carrot::BufferAllocation indexData;

        explicit MeshletsTemplate(std::size_t index, std::function<void(WeakPoolHandle*)> destructor,
                                  MeshletManager& manager,
                                  std::size_t firstCluster, std::span<const Cluster> clusters,
                                  Carrot::BufferAllocation&& vertexData, Carrot::BufferAllocation&& indexData);

        ~MeshletsTemplate();

    private:
        MeshletManager& manager;
    };

    /**
     * Handle to an instance rendered via meshlets.
     */
    struct MeshletsInstance: public WeakPoolHandle {
        std::vector<std::shared_ptr<MeshletsTemplate>> templates;
        Viewport* pViewport = nullptr; //< which viewport is this instance for?
        Carrot::InstanceData instanceData;
        bool enabled = false;

        explicit MeshletsInstance(std::size_t index, std::function<void(WeakPoolHandle*)> destructor,
                                  MeshletManager& manager,
                                  std::span<std::shared_ptr<MeshletsTemplate>>,
                                  Viewport* pViewport);

        std::shared_ptr<MeshletsInstance> clone();

    private:
        MeshletManager& manager;
    };

    struct MeshletsDescription {
        /// Meshlets of the mesh, must be valid for the entire duration of the 'addGeometry' call
        std::span<Render::Meshlet> meshlets;

        /// Original vertices of the mesh, must be valid for the entire duration of the 'addGeometry' call
        std::span<Carrot::Vertex> originalVertices;

        /// Indices of vertices inside 'originalVertices', used by meshlets to know which vertices they use
        std::span<std::uint32_t> meshletVertexIndices;

        /// Indices of vertices inside 'meshletVertexIndices', used by meshlets to describe their triangles
        std::span<std::uint32_t> meshletIndices;
    };

    struct MeshletsInstanceDescription {
        Viewport* pViewport = nullptr;
        std::span<std::shared_ptr<MeshletsTemplate>> templates;
    };

    /**
     * Manager of Meshlets, meant to be rendered to the visibility buffer
     */
    class MeshletManager: public SwapchainAware {
    public:
        explicit MeshletManager(VulkanRenderer& renderer);

    public:
        std::shared_ptr<MeshletsTemplate> addGeometry(const MeshletsDescription& desc);
        std::shared_ptr<MeshletsInstance> addInstance(const MeshletsInstanceDescription& desc);

    public:
        void beginFrame(const Carrot::Render::Context& mainRenderContext);
        void render(const Carrot::Render::Context& renderContext);

    public:
        void onSwapchainImageCountChange(size_t newCount) override;

        void onSwapchainSizeChange(Window& window, int newWidth, int newHeight) override;

    private:
        std::shared_ptr<Carrot::Pipeline> getPipeline(const Carrot::Render::Context& renderContext);

        VulkanRenderer& renderer;
        Async::SpinLock accessLock;
        WeakPool<MeshletsTemplate> geometries;
        WeakPool<MeshletsInstance> instances;

        bool requireClusterUpdate = true;
        std::shared_ptr<Carrot::BufferAllocation> clusterGPUVisibleArray;
        std::vector<Cluster> clusters;
        std::unordered_map<Viewport*, std::shared_ptr<Carrot::Pipeline>> pipelines;
        Render::PerFrame<std::shared_ptr<Carrot::BufferAllocation>> clusterDataPerFrame;
    };

} // Carrot::Render
