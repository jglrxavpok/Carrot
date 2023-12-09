//
// Created by jglrxavpok on 20/11/2023.
//

#include "ClusterManager.h"
#include <engine/utils/Profiling.h>
#include <engine/utils/Macros.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/Engine.h>

namespace Carrot::Render {

    ClustersTemplate::ClustersTemplate(std::size_t index, std::function<void(WeakPoolHandle*)> destructor,
                                       ClusterManager& manager,
                                       std::size_t firstCluster, std::span<const Cluster> clusters,
                                       Carrot::BufferAllocation&& vertexData, Carrot::BufferAllocation&& indexData)
                                       : WeakPoolHandle(index, destructor)
                                       , manager(manager)
                                       , firstCluster(firstCluster)
                                       , clusters(clusters.begin(), clusters.end())
                                       , vertexData(std::move(vertexData))
                                       , indexData(std::move(indexData))
    {

    }

    ClustersTemplate::~ClustersTemplate() {

    }

    ClustersInstance::ClustersInstance(std::size_t index, std::function<void(WeakPoolHandle*)> destructor,
                                       ClusterManager& manager,
                                       std::span<std::shared_ptr<ClustersTemplate>> _templates,
                                       Viewport* pViewport)
                                       : WeakPoolHandle(index, destructor)
                                       , manager(manager)
                                       , templates{_templates.begin(), _templates.end()}
                                       , pViewport(pViewport)
                                       {

    }

    std::shared_ptr<ClustersInstance> ClustersInstance::clone() {
        ClustersInstanceDescription cloneDesc;
        cloneDesc.pViewport = pViewport;
        cloneDesc.templates = templates;
        return manager.addInstance(cloneDesc);
    }

    ClusterManager::ClusterManager(VulkanRenderer& renderer): renderer(renderer) {
        onSwapchainImageCountChange(renderer.getSwapchainImageCount());
    }

    std::shared_ptr<ClustersTemplate> ClusterManager::addGeometry(const ClustersDescription& desc) {
        verify(desc.meshlets.size() > 0, "Cannot add 0 meshlets to this manager!");
        Async::LockGuard l { accessLock };
        const std::size_t firstClusterIndex = clusters.size();
        clusters.resize(clusters.size() + desc.meshlets.size());

        std::vector<Carrot::Vertex> vertices;
        std::vector<std::uint32_t> indices;
        for(std::size_t i = 0; i < desc.meshlets.size(); i++) {
            Meshlet& meshlet = desc.meshlets[i];
            Cluster& cluster = clusters[i + firstClusterIndex];

            cluster.transform = desc.transform;
            cluster.lod = meshlet.lod;
            cluster.triangleCount = static_cast<std::uint8_t>(meshlet.indexCount/3);

            const std::size_t firstVertexIndex = vertices.size();
            vertices.resize(firstVertexIndex + meshlet.vertexCount);

            const std::size_t firstIndexIndex = indices.size();
            indices.resize(firstIndexIndex + meshlet.indexCount);

            Carrot::Async::parallelFor(meshlet.vertexCount, [&](std::size_t index) {
                vertices[index + firstVertexIndex] = desc.originalVertices[desc.meshletVertexIndices[index + meshlet.vertexOffset]];
            }, 8);
            Carrot::Async::parallelFor(meshlet.indexCount, [&](std::size_t index) {
                indices[index + firstIndexIndex] = desc.meshletIndices[index + meshlet.indexOffset];
            }, 8);
        }

        BufferAllocation vertexData = GetResourceAllocator().allocateDeviceBuffer(sizeof(Carrot::Vertex) * vertices.size(), vk::BufferUsageFlagBits::eStorageBuffer);
        vertexData.view.stageUpload(std::span<const Carrot::Vertex>{vertices});
        BufferAllocation indexData = GetResourceAllocator().allocateDeviceBuffer(sizeof(std::uint32_t) * indices.size(), vk::BufferUsageFlagBits::eStorageBuffer);
        indexData.view.stageUpload(std::span<const std::uint32_t>{indices});

        std::size_t vertexOffset = 0;
        std::size_t indexOffset = 0;
        for(std::size_t i = 0; i < desc.meshlets.size(); i++) {
            auto& cluster = clusters[i + firstClusterIndex];
            cluster.vertexBufferAddress = vertexData.view.getDeviceAddress() + vertexOffset;
            cluster.indexBufferAddress = indexData.view.getDeviceAddress() + indexOffset;
            vertexOffset += sizeof(Carrot::Vertex) * desc.meshlets[i].vertexCount;
            indexOffset += sizeof(std::uint32_t) * desc.meshlets[i].indexCount;
        }

        requireClusterUpdate = true;
        return geometries.create(std::ref(*this),
                                 firstClusterIndex, std::span{ clusters.data() + firstClusterIndex, desc.meshlets.size() },
                                 std::move(vertexData), std::move(indexData));
    }

    std::shared_ptr<ClustersInstance> ClusterManager::addInstance(const ClustersInstanceDescription& desc) {
        Async::LockGuard l { accessLock };
        return instances.create(std::ref(*this),
                                desc.templates,
                                desc.pViewport);
    }

    void ClusterManager::beginFrame(const Carrot::Render::Context& mainRenderContext) {
        ZoneScoped;
        Async::LockGuard l { accessLock };
        auto purge = [](auto& pool) {
            pool.erase(std::find_if(WHOLE_CONTAINER(pool), [](auto a) { return a.second.expired(); }), pool.end());
        };
        purge(instances);
        purge(geometries);
    }

    static std::uint64_t triangleCount = 0;

    void ClusterManager::render(const Carrot::Render::Context& renderContext) {
        if(clusters.empty()) {
            return;
        }

        static int globalLOD = 0;
        const bool isMainViewport = renderContext.pViewport == &GetEngine().getMainViewport();
        if(isMainViewport) {
            if(ImGui::Begin("Debug clusters")) {
                ImGui::SliderInt("LOD", &globalLOD, 0, 10);
                ImGui::Text("Current triangle count: %llu", triangleCount);
            }
            ImGui::End();
            triangleCount = 0;
        }

        // draw all instances that match with the given render context
        auto& packet = renderer.makeRenderPacket(PassEnum::VisibilityBuffer, renderContext);
        packet.pipeline = getPipeline(renderContext);

        if(requireClusterUpdate) {
            clusterGPUVisibleArray = std::make_shared<BufferAllocation>(std::move(GetResourceAllocator().allocateDeviceBuffer(sizeof(Cluster) * clusters.size(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)));
            clusterGPUVisibleArray->view.stageUpload(std::span<const Cluster>{ clusters });
            requireClusterUpdate = false;
        }
        clusterDataPerFrame[renderContext.swapchainIndex] = clusterGPUVisibleArray; // keep ref to avoid allocation going back to heap while still in use

        Carrot::BufferView clusterRefs = clusterDataPerFrame[renderContext.swapchainIndex]->view;
        if(clusterRefs) {
            renderer.bindBuffer(*packet.pipeline, renderContext, clusterRefs, 0, 0);
        }

        for(auto& [index, pInstance] : instances) {
            if(auto instance = pInstance.lock()) {
                if(!instance->enabled) {
                    continue;
                }
                if(instance->pViewport != renderContext.pViewport) {
                    continue;
                }

                packet.clearPerDrawData();
                packet.unindexedDrawCommands.clear();
                packet.useInstance(instance->instanceData);

                for(const auto& pTemplate : instance->templates) {
                    std::size_t clusterOffset = 0;
                    for(const auto& cluster : pTemplate->clusters) {
                        if(cluster.lod == globalLOD) {
                            auto& drawCommand = packet.unindexedDrawCommands.emplace_back();
                            drawCommand.instanceCount = 1;
                            drawCommand.firstInstance = 0;
                            drawCommand.firstVertex = 0;
                            drawCommand.vertexCount = std::uint32_t(cluster.triangleCount)*3;

                            triangleCount += cluster.triangleCount;

                            GBufferDrawData drawData;
                            // TODO: drawData.materialIndex = instance.materialIndex;
                            drawData.materialIndex = 0;
                            drawData.uuid0 = clusterOffset + pTemplate->firstCluster;
                            packet.addPerDrawData(std::span{ &drawData, 1 });
                        }

                        clusterOffset++;
                    }
                }

                renderer.render(packet);
            }
        }
    }

    void ClusterManager::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
        // no-op
    }

    void ClusterManager::onSwapchainImageCountChange(size_t newCount) {
        clusterDataPerFrame.resize(newCount);
    }

    std::shared_ptr<Carrot::Pipeline> ClusterManager::getPipeline(const Carrot::Render::Context& renderContext) {
        auto& pPipeline = pipelines[renderContext.pViewport];
        if(!pPipeline) {
            pPipeline = renderer.getOrCreatePipelineFullPath("resources/pipelines/visibility-buffer.json", (std::uint64_t)renderContext.pViewport);
        }
        return pPipeline;
    }
} // Carrot::Render