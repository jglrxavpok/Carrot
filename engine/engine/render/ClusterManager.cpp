//
// Created by jglrxavpok on 20/11/2023.
//

#include "ClusterManager.h"

#include <core/containers/Vector.hpp>
#include <core/io/Logging.hpp>
#include <engine/utils/Profiling.h>
#include <engine/utils/Macros.h>
#include <engine/render/resources/ResourceAllocator.h>
#include <engine/render/VulkanRenderer.h>
#include <engine/Engine.h>
#include <engine/console/RuntimeOption.hpp>
#include <engine/utils/Time.h>

#include "raytracing/ASBuilder.h"
#include "resources/LightMesh.h"

namespace Carrot::Render {

    using ClusterVertex = Carrot::PackedVertex;
    using ClusterIndex = std::uint16_t; // would love to use uint8_t, but raytracing imposes at least u16 (from what I understand of the doc)

    static Carrot::RuntimeOption ShowLODOverride("Debug/Clusters/Show LOD override", false);

    struct ClusterBasedModelData {
        Carrot::InstanceData instanceData;

        // include padding (due to alignment, ClusterBasedModelData cannot be less than 176 bytes)
        std::uint8_t visible;
        std::uint8_t pad[15];
    };

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

    ClusterModel::ClusterModel(std::size_t index, std::function<void(WeakPoolHandle*)> destructor,
                                       ClusterManager& manager,
                                       std::span<std::shared_ptr<ClustersTemplate>> _templates,
                                       std::span<std::shared_ptr<MaterialHandle>> _materials,
                                       Viewport* pViewport,
                                       std::uint32_t firstInstance,
                                       std::uint32_t instanceCount)
                                       : WeakPoolHandle(index, destructor)
                                       , manager(manager)
                                       , templates{_templates.begin(), _templates.end()}
                                       , pViewport(pViewport)
                                       , pMaterials{_materials.begin(), _materials.end()}
                                       , firstInstance(firstInstance)
                                       , instanceCount(instanceCount)
                                       {

    }

    std::shared_ptr<ClusterModel> ClusterModel::clone() {
        ClustersInstanceDescription cloneDesc;
        cloneDesc.pViewport = pViewport;
        cloneDesc.templates = templates;
        cloneDesc.pMaterials = pMaterials;
        return manager.addModel(cloneDesc);
    }

    ClusterManager::ClusterManager(VulkanRenderer& renderer): renderer(renderer) {
        onSwapchainImageCountChange(renderer.getSwapchainImageCount());
        templateClusterGroups.setGrowthFactor(2);
    }

    std::shared_ptr<ClustersTemplate> ClusterManager::addGeometry(const ClustersDescription& desc) {
        verify(desc.meshlets.size() > 0, "Cannot add 0 meshlets to this manager!");
        Async::LockGuard l { accessLock };
        const std::size_t firstClusterIndex = gpuClusters.size();
        const std::size_t firstGroupIndex = templateClusterGroups.size();

        gpuClusters.resize(gpuClusters.size() + desc.meshlets.size());
        templatesFromClusters.resize(gpuClusters.size());
        groupsFromClusters.resize(gpuClusters.size());

        std::vector<Carrot::PackedVertex> vertices;
        std::vector<ClusterIndex> indices;
        for(std::size_t i = 0; i < desc.meshlets.size(); i++) {
            Meshlet& meshlet = desc.meshlets[i];
            Cluster& cluster = gpuClusters[i + firstClusterIndex];

            cluster.transform = desc.transform;
            cluster.lod = meshlet.lod;
            cluster.triangleCount = static_cast<std::uint8_t>(meshlet.indexCount/3);
            cluster.vertexCount = static_cast<std::uint8_t>(meshlet.vertexCount);

            cluster.boundingSphere = meshlet.boundingSphere;
            cluster.parentBoundingSphere = meshlet.parentBoundingSphere;
            cluster.parentError = meshlet.parentError;
            cluster.error = meshlet.clusterError;

            const std::size_t firstVertexIndex = vertices.size();
            vertices.resize(firstVertexIndex + meshlet.vertexCount);

            const std::size_t firstIndexIndex = indices.size();
            indices.resize(firstIndexIndex + meshlet.indexCount);

            for(std::size_t index = 0; index < meshlet.vertexCount; index++) {
                vertices[index + firstVertexIndex] = Carrot::PackedVertex{ desc.originalVertices[desc.meshletVertexIndices[index + meshlet.vertexOffset]] };
            }
            for(std::size_t index = 0; index < meshlet.indexCount; index++) {
                indices[index + firstIndexIndex] = static_cast<ClusterIndex>(desc.meshletIndices[index + meshlet.indexOffset]);
            }

            std::uint32_t globalGroupIndex = firstGroupIndex + meshlet.groupIndex;
            if(templateClusterGroups.size() <= globalGroupIndex) {
                templateClusterGroups.resize(globalGroupIndex+1);

                templateClusterGroups[globalGroupIndex].clusters.ensureReserve(4); // expect most groups to contain 4 clusters
            }

            templateClusterGroups[globalGroupIndex].clusters.pushBack(firstClusterIndex + i);
        }

        BufferAllocation vertexData = GetResourceAllocator().allocateDeviceBuffer(sizeof(Carrot::PackedVertex) * vertices.size(), vk::BufferUsageFlagBits::eStorageBuffer);
        vertexData.name(Carrot::sprintf("Virtual geometry vertex buffer %llu meshlets", desc.meshlets.size()));
        vertexData.view.stageUpload(std::span<const Carrot::PackedVertex>{vertices});
        BufferAllocation indexData = GetResourceAllocator().allocateDeviceBuffer(sizeof(ClusterIndex) * indices.size(), vk::BufferUsageFlagBits::eStorageBuffer);
        indexData.name(Carrot::sprintf("Virtual geometry index buffer %llu meshlets", desc.meshlets.size()));
        indexData.view.stageUpload(std::span<const ClusterIndex>{indices});

        std::size_t vertexOffset = 0;
        std::size_t indexOffset = 0;
        for(std::size_t i = 0; i < desc.meshlets.size(); i++) {
            auto& cluster = gpuClusters[i + firstClusterIndex];
            cluster.vertexBufferAddress = vertexData.view.getDeviceAddress() + vertexOffset;
            cluster.indexBufferAddress = indexData.view.getDeviceAddress() + indexOffset;

            const auto& meshlet = desc.meshlets[i];
            vertexOffset += sizeof(Carrot::PackedVertex) * meshlet.vertexCount;
            indexOffset += sizeof(ClusterIndex) * meshlet.indexCount;
        }

        requireClusterUpdate = true;
        std::shared_ptr<ClustersTemplate> pTemplate = geometries.create(std::ref(*this),
                                 firstClusterIndex, std::span{ gpuClusters.data() + firstClusterIndex, desc.meshlets.size() },
                                 std::move(vertexData), std::move(indexData));
        for(std::size_t i = 0; i < desc.meshlets.size(); i++) {
            templatesFromClusters[i + firstClusterIndex] = pTemplate->getSlot();
            groupsFromClusters[i + firstClusterIndex] = firstGroupIndex + desc.meshlets[i].groupIndex;
        }

        return pTemplate;
    }

    std::shared_ptr<ClusterModel> ClusterManager::addModel(const ClustersInstanceDescription& desc) {
        verify(desc.templates.size() == desc.pMaterials.size(), "There must be as many templates as material handles!");
        auto& gpuInstances = perViewport[desc.pViewport].gpuClusterInstances;
        auto& groupInstances = perViewport[desc.pViewport].groupInstances;

        std::uint32_t clusterCount = 0;

        for(const auto& pTemplate : desc.templates) {
            clusterCount += pTemplate->clusters.size();
        }

        Async::LockGuard l { accessLock };
        perViewport[desc.pViewport].requireInstanceUpdate = true;
        const std::uint32_t firstInstanceID = gpuInstances.size();
        // TODO: gpu instances should be inside a weakpool map too (to reuse slots and memory)
        gpuInstances.resize(firstInstanceID + clusterCount);
        groupInstances.perCluster.resize(gpuInstances.size());

        std::uint32_t clusterIndex = 0;
        std::uint32_t templateIndex = 0;
        std::uint32_t minGroupInstanceID = std::numeric_limits<std::uint32_t>::max();
        std::uint32_t maxGroupInstanceID = 0;


        std::unordered_map<ClustersTemplate*, std::unordered_map<std::uint32_t, std::uint32_t>> groupInstanceIDs;
        // fill group instances for this model
        std::uint32_t firstGroupInstanceID = groupInstances.groups.size();
        std::uint32_t localGroupInstanceID = 0;
        for(const auto& pTemplate : desc.templates) {
            auto& groupInstanceIDsForThisTemplate = groupInstanceIDs[pTemplate.get()];
            for(std::size_t i = 0; i < pTemplate->clusters.size(); i++) {
                std::size_t clusterInstanceID = firstInstanceID + clusterIndex;
                auto& gpuInstance = gpuInstances[clusterInstanceID];
                gpuInstance.materialIndex = desc.pMaterials[templateIndex]->getSlot();
                gpuInstance.clusterID = pTemplate->firstCluster + i;

                const std::uint32_t groupID = groupsFromClusters[gpuInstance.clusterID];

                auto [iter, isNew] = groupInstanceIDsForThisTemplate.try_emplace(groupID);
                if(isNew) {
                    iter->second = localGroupInstanceID++;
                    groupInstances.groups.resize(firstGroupInstanceID + localGroupInstanceID);
                }
                std::uint32_t groupInstanceID = firstGroupInstanceID + iter->second;
                groupInstances.groups[groupInstanceID].group.clusters.pushBack(clusterInstanceID);
                groupInstances.groups[groupInstanceID].templateID = groupID;
                groupInstances.perCluster[clusterInstanceID] = groupInstanceID;

                clusterIndex++;

                minGroupInstanceID = std::min(groupInstanceID, minGroupInstanceID);
                maxGroupInstanceID = std::max(groupInstanceID, maxGroupInstanceID);
            }
            templateIndex++;
        }

        auto pModel = models.create(std::ref(*this),
                                desc.templates,
                                desc.pMaterials,
                                desc.pViewport,
                                firstInstanceID, clusterCount);
        // each instance will point to the instance data of the ClusterModel we just created
        for(std::size_t i = 0; i < clusterCount; i++) {
            std::size_t clusterInstanceID = firstInstanceID + i;
            auto& gpuInstance = gpuInstances[clusterInstanceID];
            gpuInstance.instanceDataIndex = pModel->getSlot();
        }

        // create RTData for each group inside this model:
        GroupRTData& rtDataMap = groupRTDataPerModel[pModel->getSlot()];
        rtDataMap.firstGroupInstanceIndex = minGroupInstanceID;
        rtDataMap.data.resize(maxGroupInstanceID +1 - minGroupInstanceID); // create RTData for this group if does not already exist

        groupInstances.blases.resize(groupInstances.groups.size());

        return pModel;
    }

    Carrot::BufferView ClusterManager::getClusters(const Carrot::Render::Context& renderContext) {
        auto& pAlloc = clusterDataPerFrame[renderContext.swapchainIndex];
        return pAlloc ? pAlloc->view : Carrot::BufferView{};
    }

    Carrot::BufferView ClusterManager::getClusterInstances(const Carrot::Render::Context& renderContext) {
        auto iter = perViewport.find(renderContext.pViewport);
        if(iter == perViewport.end()) {
            return Carrot::BufferView{};
        }
        auto& pAlloc = iter->second.instancesPerFrame[renderContext.swapchainIndex];
        return pAlloc ? pAlloc->view : Carrot::BufferView{};
    }

    Carrot::BufferView ClusterManager::getClusterInstanceData(const Carrot::Render::Context& renderContext) {
        auto& pAlloc = instanceDataPerFrame[renderContext.swapchainIndex];
        return pAlloc ? pAlloc->view : Carrot::BufferView{};
    }

    void ClusterManager::beginFrame(const Carrot::Render::Context& mainRenderContext) {
        ZoneScoped;

        {
            Async::LockGuard l { accessLock };
            auto purge = [](auto& pool) {
                pool.erase(std::find_if(WHOLE_CONTAINER(pool), [](auto a) { return a.second.expired(); }), pool.end());
            };
            purge(models);
            purge(geometries);
        }

        if(GetEngine().getCapabilities().supportsRaytracing) {
            if(mainRenderContext.lastSwapchainIndex != static_cast<std::size_t>(-1)) {
                queryVisibleClustersAndActivateRTInstances(mainRenderContext.lastSwapchainIndex);
            }
        }
    }

    static std::uint64_t triangleCount = 0;

    void ClusterManager::render(const Carrot::Render::Context& renderContext) {
        static int globalLOD = 0;
        static int lodSelectionMode = 0;
        static float errorThreshold = 1.0f;
        static bool showTriangleCount = false;
        const bool isMainViewport = renderContext.pViewport == &GetEngine().getMainViewport();
        if(ShowLODOverride && isMainViewport) {
            bool keepOpen = true;

            if(ImGui::Begin("Debug clusters", &keepOpen)) {
                ImGui::RadioButton("Automatic LOD selection", &lodSelectionMode, 0);
                ImGui::RadioButton("Manual LOD selection", &lodSelectionMode, 1);

                if(lodSelectionMode == 0) {
                    ImGui::SliderFloat("Threshold", &errorThreshold, 0.0f, 10.0f);
                } else if(lodSelectionMode == 1) {
                    ImGui::SliderInt("LOD", &globalLOD, 0, 25);
                }
                ImGui::Checkbox("Show triangle count (eats performance)", &showTriangleCount);
                if(showTriangleCount) {
                    ImGui::Text("Current triangle count: %llu", triangleCount);
                }
            }
            ImGui::End();
            if(!keepOpen) {
                ShowLODOverride.setValue(false);
            }
            triangleCount = 0;
        }

        if(gpuClusters.empty()) {
            return;
        }

        auto& gpuInstances = perViewport[renderContext.pViewport].gpuClusterInstances;
        if(gpuInstances.empty()) {
            return;
        }

        auto& statsCPUBufferPerFrame = perViewport[renderContext.pViewport].statsCPUBuffersPerFrame;
        if(statsCPUBufferPerFrame.empty()) {
            statsCPUBufferPerFrame.resize(GetEngine().getSwapchainImageCount());

            for(std::size_t i = 0; i< statsCPUBufferPerFrame.size(); i++) {
                statsCPUBufferPerFrame[i] = GetResourceAllocator().allocateStagingBuffer(sizeof(StatsBuffer));
            }
        }
        auto& statsCPUBuffer = statsCPUBufferPerFrame[renderContext.swapchainIndex];

        StatsBuffer* pStats = statsCPUBuffer.view.map<StatsBuffer>();
        triangleCount += pStats->totalTriangleCount;
        pStats->totalTriangleCount = 0;

        const Carrot::Camera& camera = renderContext.getCamera();

        // draw all instances that match with the given render context
        auto& packet = renderer.makeRenderPacket(PassEnum::VisibilityBuffer, Render::PacketType::Mesh, renderContext);
        packet.pipeline = getPipeline(renderContext);

        if(requireClusterUpdate) {
            clusterGPUVisibleArray = std::make_shared<BufferAllocation>(std::move(GetResourceAllocator().allocateDeviceBuffer(sizeof(Cluster) * gpuClusters.size(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)));
            clusterGPUVisibleArray->view.stageUpload(std::span<const Cluster>{ gpuClusters });
            requireClusterUpdate = false;
        }

        auto& instanceGPUVisibleArray = perViewport[renderContext.pViewport].instanceGPUVisibleArray;
        // TODO: allow material update once instance are already created? => needs something similar to MaterialSystem::getData
        bool& requireInstanceUpdate = perViewport[renderContext.pViewport].requireInstanceUpdate;
        if(requireInstanceUpdate) {
            instanceGPUVisibleArray = std::make_shared<BufferAllocation>(std::move(GetResourceAllocator().allocateDeviceBuffer(sizeof(ClusterInstance) * gpuInstances.size(), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst)));
            instanceGPUVisibleArray->view.stageUpload(std::span<const ClusterInstance>{ gpuInstances });

            instanceDataGPUVisibleArray = std::make_shared<BufferAllocation>(std::move(GetResourceAllocator().allocateStagingBuffer(sizeof(ClusterBasedModelData) * models.getRequiredStorageCount(), alignof(InstanceData))));

            requireInstanceUpdate = false;
        }

        BufferView activeModelsBufferView;
        activeInstancesAllocator.clear();
        Vector<std::uint32_t> activeInstances { activeInstancesAllocator };
        if(instanceDataGPUVisibleArray) {
            ClusterBasedModelData* pModelData = instanceDataGPUVisibleArray->view.map<ClusterBasedModelData>();

            for(auto& [slot, pModel] : models) {
                if(auto pLockedModel = pModel.lock()) {
                    if(pLockedModel->pViewport != renderContext.pViewport) {
                        continue;
                    }
                    pModelData[slot].visible = pLockedModel->enabled;
                    pModelData[slot].instanceData = pLockedModel->instanceData;

                    activeInstances.ensureReserve(activeInstances.size() + pLockedModel->instanceCount);
                    const std::size_t endInstance = pLockedModel->firstInstance + pLockedModel->instanceCount;
                    for(std::size_t instanceIndex = pLockedModel->firstInstance; instanceIndex < endInstance; instanceIndex++) {
                        activeInstances.pushBack(instanceIndex);
                    }
                }
            }

            activeModelsBufferView = renderer.getSingleFrameBuffer(activeInstances.size() * sizeof(std::uint32_t));
            activeModelsBufferView.directUpload(std::span<const std::uint32_t>(activeInstances));
        }

        clusterDataPerFrame[renderContext.swapchainIndex] = clusterGPUVisibleArray; // keep ref to avoid allocation going back to heap while still in use
        auto& instancesPerFrame = perViewport[renderContext.pViewport].instancesPerFrame;
        if(instancesPerFrame.size() != GetEngine().getSwapchainImageCount()) {
            instancesPerFrame.resize(GetEngine().getSwapchainImageCount());
        }
        instancesPerFrame[renderContext.swapchainIndex] = instanceGPUVisibleArray; // keep ref to avoid allocation going back to heap while still in use
        instanceDataPerFrame[renderContext.swapchainIndex] = instanceDataGPUVisibleArray; // keep ref to avoid allocation going back to heap while still in use

        const Carrot::BufferView clusterRefs = clusterDataPerFrame[renderContext.swapchainIndex]->view;
        const Carrot::BufferView instanceRefs = instancesPerFrame[renderContext.swapchainIndex]->view;
        const Carrot::BufferView instanceDataRefs = instanceDataPerFrame[renderContext.swapchainIndex]->view;
        const Carrot::BufferView readbackBufferView = getReadbackBuffer(renderContext.pViewport, renderContext.swapchainIndex)->getWholeView();
        if(clusterRefs) {
            renderer.bindBuffer(*packet.pipeline, renderContext, clusterRefs, 0, 0);
            renderer.bindBuffer(*packet.pipeline, renderContext, instanceRefs, 0, 1);
            renderer.bindBuffer(*packet.pipeline, renderContext, instanceDataRefs, 0, 2);
            renderer.bindBuffer(*packet.pipeline, renderContext, statsCPUBuffer.view, 0, 4);
            renderer.bindBuffer(*packet.pipeline, renderContext, activeModelsBufferView, 0, 5);
            renderer.bindBuffer(*packet.pipeline, renderContext, readbackBufferView, 0, 6);
        }


        {
            auto& pushConstant = packet.addPushConstant("push", vk::ShaderStageFlagBits::eMeshEXT);
            using Flags = std::uint8_t;
            constexpr std::uint8_t Flags_None = 0;
            constexpr std::uint8_t Flags_OutputTriangleCount = 1;
            struct PushConstantData {
                std::uint32_t maxClusterID;
                std::uint32_t lodSelectionMode;
                float lodErrorThreshold;
                std::uint32_t forcedLOD;
                float screenHeight;
                Flags flags;
            };
            PushConstantData data{};
            data.maxClusterID = gpuInstances.size();
            data.lodSelectionMode = lodSelectionMode;
            data.lodErrorThreshold = errorThreshold;
            data.forcedLOD = globalLOD;
            data.screenHeight = renderContext.pViewport->getHeight();
            data.flags = Flags_None;
            if(ShowLODOverride && showTriangleCount) {
                data.flags |= Flags_OutputTriangleCount;
            }
            pushConstant.setData(std::move(data));
        }

        Render::PacketCommand& drawCommand = packet.commands.emplace_back();
        const int groupSize = 32;
        drawCommand.drawMeshTasks.groupCountX = activeInstances.size() / groupSize;
        drawCommand.drawMeshTasks.groupCountY = 1;
        drawCommand.drawMeshTasks.groupCountZ = 1;
        renderer.render(packet);
    }

    void ClusterManager::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
        // no-op
    }

    void ClusterManager::onSwapchainImageCountChange(size_t newCount) {
        clusterDataPerFrame.resize(newCount);
        perViewport.clear();
        instanceDataPerFrame.resize(newCount);

        readbackJobsSync.resize(newCount);
    }

    std::shared_ptr<Carrot::Pipeline> ClusterManager::getPipeline(const Carrot::Render::Context& renderContext) {
        auto& pPipeline = perViewport[renderContext.pViewport].pipeline;
        if(!pPipeline) {
            pPipeline = renderer.getOrCreatePipelineFullPath("resources/pipelines/visibility-buffer.json", (std::uint64_t)renderContext.pViewport);
        }
        return pPipeline;
    }

    Memory::OptionalRef<Carrot::Buffer> ClusterManager::getReadbackBuffer(Carrot::Render::Viewport* pViewport, std::size_t frameIndex) {
        auto& readbackBuffersPerFrame = perViewport[pViewport].readbackBuffersPerFrame;
        if(readbackBuffersPerFrame.empty()) {
            readbackBuffersPerFrame.resize(GetEngine().getSwapchainImageCount());
        }
        auto& gpuInstances = perViewport[pViewport].gpuClusterInstances;
        std::unique_ptr<Carrot::Buffer>& pReadbackBuffer = readbackBuffersPerFrame[frameIndex];
        Async::Counter& syncCounter = readbackJobsSync[frameIndex];
        syncCounter.sleepWait();
        if(!pReadbackBuffer || pReadbackBuffer->getSize() < gpuInstances.size() * sizeof(ClusterReadbackData)) {
            if(gpuInstances.empty()) {
                return Memory::OptionalRef<Carrot::Buffer>{};
            }
            pReadbackBuffer = GetResourceAllocator().allocateDedicatedBuffer(gpuInstances.size() * sizeof(ClusterReadbackData),
                vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCached);
            pReadbackBuffer->name("ClusterManager Readback");
        }

        return *pReadbackBuffer;
    }

    std::shared_ptr<Carrot::InstanceHandle> ClusterManager::createGroupInstanceAS(std::span<const ClusterInstance> clusterInstances, GroupInstances& groupInstances, const ClusterModel& modelInstance, std::uint32_t groupInstanceID) {
        auto& asBuilder = GetRenderer().getASBuilder();

        // will need synchronisation if done on multiple threads in the future
        BLASHolder& correspondingBLAS = groupInstances.blases[groupInstanceID];
        std::shared_ptr<BLASHandle> blas;
        {
            Async::LockGuard g { correspondingBLAS.lock };
            if(!correspondingBLAS.blas) {
                const GroupInstance& groupInstance = groupInstances.groups[groupInstanceID];

                std::vector<std::shared_ptr<Carrot::Mesh>> meshes;
                std::vector<glm::mat4> transforms;
                std::vector<std::uint32_t> materialIndices;
                meshes.reserve(groupInstance.group.clusters.size());
                transforms.reserve(meshes.size());
                materialIndices.reserve(meshes.size());

                for(std::uint32_t clusterInstanceID : groupInstance.group.clusters) {
                    const ClusterInstance& clusterInstance = clusterInstances[clusterInstanceID];
                    if(auto pTemplate = geometries.find(templatesFromClusters[clusterInstance.clusterID]).lock()) {
                        const Cluster& cluster = gpuClusters[clusterInstance.clusterID];
                        const BufferView vertexBuffer = pTemplate->vertexData.view.subViewFromAddress(cluster.vertexBufferAddress, cluster.vertexCount * sizeof(PackedVertex));
                        const BufferView indexBuffer = pTemplate->indexData.view.subViewFromAddress(cluster.indexBufferAddress, cluster.triangleCount * 3 * sizeof(ClusterIndex));
                        std::shared_ptr<LightMesh> mesh = std::make_shared<LightMesh>(vertexBuffer, indexBuffer, sizeof(PackedVertex), sizeof(ClusterIndex));

                        meshes.push_back(mesh);
                        transforms.push_back(cluster.transform);
                        materialIndices.push_back(clusterInstance.materialIndex);
                    }
                }

                if(meshes.empty()) {
                    return nullptr;
                }

                correspondingBLAS.blas = asBuilder.addBottomLevel(meshes, transforms, materialIndices);
            }

            blas = correspondingBLAS.blas;
        }

        return asBuilder.addInstance(blas);
    }

    void ClusterManager::processSingleClusterReadbackData(
        Carrot::TaskHandle& task,
        std::uint32_t clusterIndex,
        double currentTime,
        GroupInstances& groupInstances,
        std::span<const ClusterInstance> clusterInstances,
        const ClusterReadbackData* pData)
    {
        const ClusterReadbackData& readbackData = pData[clusterIndex];
        const ClusterInstance& clusterInstance = clusterInstances[clusterIndex];
        const std::uint32_t groupInstanceID = groupInstances.perCluster[clusterIndex];

        if(auto pModel = models.find(clusterInstance.instanceDataIndex).lock()) {
            GroupRTData& groupRTData = groupRTDataPerModel[pModel->getSlot()];
            RTData& rtData = groupRTData.data[groupInstanceID - groupRTData.firstGroupInstanceIndex];
            //Cider::LockGuard g { task, rtData.mutex };
            Async::LockGuard g { rtData.mutex };
            if(readbackData.visible) {
                rtData.lastUpdateTime = currentTime;

                if(!rtData.as) {
                    rtData.as = createGroupInstanceAS(clusterInstances, groupInstances, *pModel, groupInstanceID);
                    rtData.as->enabled = true;
                    rtData.as->transform = pModel->instanceData.transform;
                }
                // update instance AS transform
                rtData.as->enabled = true;
                rtData.as->transform = pModel->instanceData.transform;
            }
        }
    }

    void ClusterManager::processReadbackData(Carrot::Render::Viewport* pViewport, const ClusterReadbackData* pData, std::size_t count) {
        Async::LockGuard l { accessLock };
        auto& clusterInstances = perViewport[pViewport].gpuClusterInstances;
        auto& groupInstances = perViewport[pViewport].groupInstances;
        double currentTime = Time::getCurrentTime();

        Async::Counter sync;
        std::size_t granularity = 64;
        std::size_t parallelJobs = count / granularity; // truncate on purpose, the calling thread will participate
        auto processRange = [&](std::size_t start) {
            GetTaskScheduler().schedule(TaskDescription {
                .name = "processSingleClusterReadbackData",
                .task = [&, start](Carrot::TaskHandle& task) {
                    for(std::size_t i = start; i < start + granularity && i < count; i++) {
                        processSingleClusterReadbackData(task, i, currentTime, groupInstances, clusterInstances, pData);
                    }
                },
                .joiner = &sync,
            }, TaskScheduler::FrameParallelWork);
        };

        for(std::size_t jobIndex = 0; jobIndex <= parallelJobs; jobIndex++) {
            processRange(jobIndex * granularity);
        }

        //12409

        sync.sleepWait();
    }

    /// Readbacks culled instances from the GPU, and prepares acceleration structures for raytracing, based on which clusters are culled or not
    void ClusterManager::queryVisibleClustersAndActivateRTInstances(std::size_t lastFrameIndex) {
        bool allowReadback = false;
        if(ImGui::Begin("debug readback")) {
            allowReadback = ImGui::Button("go");
        }
        ImGui::End();

        if(!allowReadback) {
            return;
        }

        // reset state
        for(auto [slot, pModel] : models) {
            if(auto pLockedModel = pModel.lock()) {
                auto& rtData = groupRTDataPerModel[slot];
                rtData.resetForNewFrame();
            }
        }

        for(auto& [pViewport, _] : perViewport) {
            auto ref = getReadbackBuffer(pViewport, lastFrameIndex);
            if(!ref.hasValue()) {
                continue;
            }

            Carrot::Buffer& readbackBuffer = ref;
            const ClusterReadbackData* pData = readbackBuffer.map<const ClusterReadbackData>();
            readbackBuffer.invalidateMappedRange(0, VK_WHOLE_SIZE);

            Async::Counter& syncCounter = readbackJobsSync[lastFrameIndex];
            std::size_t count = static_cast<std::size_t>(perViewport[pViewport].gpuClusterInstances.size());
            processReadbackData(pViewport, pData, count);
        }
    }

    void ClusterManager::GroupRTData::resetForNewFrame() {
        const double currentTime = Time::getCurrentTime();
        constexpr double timeBeforeDelete = 10;

        for(auto& rtData : data) {
            if(currentTime - rtData.lastUpdateTime >= timeBeforeDelete) {
                rtData.as = nullptr;
            } else {
                if(rtData.as) {
                    rtData.as->enabled = false;
                }
            }
        }
    }

} // Carrot::Render