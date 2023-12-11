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

#include "MaterialSystem.h"

/**
 * The difference between Meshlets and Clusters is that:
 *  Meshlets reference triangles from an original mesh while Clusters hold their own triangle data
 */

namespace Carrot {
    class VulkanRenderer;
}

namespace Carrot::Render {
    class ClusterManager;
    class Viewport;

    /**
     * Sent as-is to the GPU
     */
    struct Cluster {
        vk::DeviceAddress vertexBufferAddress = (vk::DeviceAddress)-1;
        vk::DeviceAddress indexBufferAddress = (vk::DeviceAddress)-1;
        std::uint8_t triangleCount;
        std::uint32_t lod;
        glm::mat4 transform{ 1.0f };
    };

    /**
     * Sent as-is to the GPU.
     * Represents a single cluster as rendered on screen: each ClusterInstance is a unique cluster, which references:
     * - its geometry via clusterID
     * - its material via materialIndex
     * - its per-instance data via instanceDataIndex (in this case, instance means a ClusterModel)
     */
    struct ClusterInstance {
        std::uint32_t clusterID;
        std::uint32_t materialIndex;
        std::uint32_t instanceDataIndex;
    };

    /**
     * Handle to a set of meshlets
     */
    struct ClustersTemplate: public WeakPoolHandle {
        const std::size_t firstCluster;
        const std::vector<Cluster> clusters;
        const Carrot::BufferAllocation vertexData;
        const Carrot::BufferAllocation indexData;

        explicit ClustersTemplate(std::size_t index, std::function<void(WeakPoolHandle*)> destructor,
                                  ClusterManager& manager,
                                  std::size_t firstCluster, std::span<const Cluster> clusters,
                                  Carrot::BufferAllocation&& vertexData, Carrot::BufferAllocation&& indexData);

        ~ClustersTemplate();

    private:
        ClusterManager& manager;
    };

    /**
     * Handle to an model rendered via clusters.
     */
    struct ClusterModel: public WeakPoolHandle {
        std::vector<std::shared_ptr<ClustersTemplate>> templates; // expected to be one per mesh of the model
        std::vector<std::shared_ptr<MaterialHandle>> pMaterials; // one per template

        Viewport* pViewport = nullptr; //< which viewport is this instance for?
        Carrot::InstanceData instanceData;
        bool enabled = false;
        std::uint32_t firstInstance; // index of first instance, points to a ClusterInstance (instances are expected to be contiguous)
        std::uint32_t instanceCount; // count of ClusterInstances related to this model

        explicit ClusterModel(std::size_t index, std::function<void(WeakPoolHandle*)> destructor,
                                  ClusterManager& manager,
                                  std::span<std::shared_ptr<ClustersTemplate>>,
                                  std::span<std::shared_ptr<MaterialHandle>>,
                                  Viewport* pViewport,
                                  std::uint32_t firstInstance, std::uint32_t instanceCount);

        std::shared_ptr<ClusterModel> clone();

    private:
        ClusterManager& manager;
    };

    struct ClustersDescription {
        /// Meshlets of the mesh, must be valid for the entire duration of the 'addGeometry' call
        std::span<Render::Meshlet> meshlets;

        /// Original vertices of the mesh, must be valid for the entire duration of the 'addGeometry' call
        std::span<Carrot::Vertex> originalVertices;

        /// Indices of vertices inside 'originalVertices', used by meshlets to know which vertices they use
        std::span<std::uint32_t> meshletVertexIndices;

        /// Indices of vertices inside 'meshletVertexIndices', used by meshlets to describe their triangles
        std::span<std::uint32_t> meshletIndices;

        glm::mat4 transform{1.0f};
    };

    struct ClustersInstanceDescription {
        Viewport* pViewport = nullptr;
        std::span<std::shared_ptr<ClustersTemplate>> templates;
        std::span<std::shared_ptr<MaterialHandle>> pMaterials; // one per template
    };

    /**
     * Manager of Meshlets, meant to be rendered to the visibility buffer
     */
    class ClusterManager: public SwapchainAware {
    public:
        explicit ClusterManager(VulkanRenderer& renderer);

    public:
        std::shared_ptr<ClustersTemplate> addGeometry(const ClustersDescription& desc);
        std::shared_ptr<ClusterModel> addModel(const ClustersInstanceDescription& desc);

        /**
         * Buffer of all Cluster currently used
         */
        Carrot::BufferView getClusters(const Carrot::Render::Context& renderContext);

        /**
         * Buffer of all ClusterInstance currently used
         */
        Carrot::BufferView getClusterInstances(const Carrot::Render::Context& renderContext);

        /**
         * Buffer of all InstanceData currently used
         */
        Carrot::BufferView getClusterInstanceData(const Carrot::Render::Context& renderContext);

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
        WeakPool<ClustersTemplate> geometries;
        WeakPool<ClusterModel> models;

        std::vector<Cluster> gpuClusters;
        std::vector<ClusterInstance> gpuInstances;

        bool requireClusterUpdate = true;
        bool requireInstanceUpdate = true;
        std::shared_ptr<Carrot::BufferAllocation> clusterGPUVisibleArray;
        std::shared_ptr<Carrot::BufferAllocation> instanceGPUVisibleArray;
        std::shared_ptr<Carrot::BufferAllocation> instanceDataGPUVisibleArray;
        Carrot::InstanceData* pInstanceData = nullptr; // CPU visible version of instanceDataGPUVisibleArray

        std::unordered_map<Viewport*, std::shared_ptr<Carrot::Pipeline>> pipelines;
        Render::PerFrame<std::shared_ptr<Carrot::BufferAllocation>> clusterDataPerFrame;
        Render::PerFrame<std::shared_ptr<Carrot::BufferAllocation>> instancesPerFrame;
        Render::PerFrame<std::shared_ptr<Carrot::BufferAllocation>> instanceDataPerFrame;
    };

} // Carrot::Render
