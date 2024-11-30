//
// Created by jglrxavpok on 28/12/2020.
//

#include "GBuffer.h"

#include <core/io/Logging.hpp>

#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/Skybox.hpp"
#include "resources/ResourceAllocator.h"

static constexpr std::uint64_t HashGridMaxUpdatesPerFrame = 1024*1024*8;
static constexpr std::uint64_t HashGridCellsPerBucket = 128;
static constexpr std::uint64_t HashGridBucketCount = 1024;
static constexpr std::uint64_t HashGridTotalCellCount = HashGridBucketCount*HashGridCellsPerBucket;
static constexpr std::uint8_t MAX_RESERVOIRS = 10;

struct HashGrid {
    struct Reservoir {
        glm::vec3 bestSample;
        float weightSum;
        std::uint32_t sampleCount;
        float resamplingWeight;
    };

    struct HashCellKey {
        glm::vec3 hitPosition;
        glm::vec3 direction;
    };

    struct HashCell {
        HashCellKey key;
        std::uint32_t hash2;
        std::uint32_t reservoirCount;
        std::array<Reservoir, MAX_RESERVOIRS> reservoirs;
    };

    struct CellUpdate {
        HashCellKey key;
        std::uint32_t cellIndex;
        glm::vec3 surfaceNormal;
        float metallic;
        glm::vec3 surfaceColor;
        float roughness;
    };

    struct Header {
        // update requests
        std::uint32_t updateCount;
        std::uint32_t maxUpdates;
        vk::DeviceAddress pUpdates; // must be as many as there are total cells inside the grid

        // write the frame number when each cell was last touched (used to decay cells)
        vk::DeviceAddress pLastTouchedFrame; // must be as many as there are total cells inside the grid

        vk::DeviceAddress pCells;
    };

    struct Constants {
        std::uint32_t cellsPerBucket;
        std::uint32_t bucketCount;
    };

    struct Pointers {
        vk::DeviceAddress grids[2]; // 0: previous frame, 1: current frame
    };

    static Carrot::Render::PassData::HashGridResources createResources(Carrot::Render::GraphBuilder& graph) {
        Carrot::Render::PassData::HashGridResources r;

        const std::size_t hashGridSize = computeSizeOf(HashGridBucketCount, HashGridCellsPerBucket);
        r.hashGrid = graph.createBuffer("GI probes hashmap", hashGridSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, false/*we want to keep the header*/);
        r.hashGridUpdates = graph.createBuffer("GI probes hashmap update", sizeof(CellUpdate) * HashGridMaxUpdatesPerFrame, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, true);
        r.constants = graph.createBuffer("GI probes constants", sizeof(Constants), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, false/*filled once*/);
        r.gridPointers = graph.createBuffer("GI probes grid pointers", sizeof(Pointers), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, false/*filled once*/);

        graph.reuseBufferAcrossFrames(r.hashGrid, 1);
        graph.reuseBufferAcrossFrames(r.gridPointers, 1);
        return r;
    }

    static void prepareBuffers(const Carrot::Render::Graph& graph, const Carrot::Render::PassData::HashGridResources& r) {
        HashGrid::Header header{};
        header.maxUpdates = HashGridMaxUpdatesPerFrame;
        header.updateCount = 0;

        // only a single one for both frame
        {
            Constants constants{};
            constants.bucketCount = HashGridBucketCount;
            constants.cellsPerBucket = HashGridCellsPerBucket;
            graph.getBuffer(r.constants, 0).view.uploadForFrame(&constants, sizeof(constants));
        }

        for(int i = 0; i < 2/* history length: current frame and previous frame */; i++) {
            Carrot::BufferView gridBuffer = graph.getBuffer(r.hashGrid, i).view;
            Carrot::BufferView updatesBuffer = graph.getBuffer(r.hashGridUpdates, i).view;

            std::size_t cursor = sizeof(Header);
            header.pCells = gridBuffer.subView(cursor, sizeof(HashGrid::HashCell) * HashGridTotalCellCount).getDeviceAddress();

            cursor += sizeof(HashGrid::HashCell) * HashGridTotalCellCount;
            header.pLastTouchedFrame = gridBuffer.subView(cursor, sizeof(std::uint32_t) * HashGridTotalCellCount).getDeviceAddress();
            cursor += sizeof(std::uint32_t) * HashGridTotalCellCount;

            header.pUpdates = updatesBuffer.getDeviceAddress();

            gridBuffer.uploadForFrame(std::span<const HashGrid::Header>(&header, 1));
        }

        Pointers pointers{};
        // previous frame
        pointers.grids[0] = graph.getBuffer(r.hashGrid, 1).view.getDeviceAddress();

        // current frame
        pointers.grids[1] = graph.getBuffer(r.hashGrid, 0).view.getDeviceAddress();
        graph.getBuffer(r.gridPointers, 0).view.uploadForFrame(&pointers, sizeof(pointers));

        // previous frame
        pointers.grids[0] = graph.getBuffer(r.hashGrid, 0).view.getDeviceAddress();

        // current frame
        pointers.grids[1] = graph.getBuffer(r.hashGrid, 1).view.getDeviceAddress();
        graph.getBuffer(r.gridPointers, 1).view.uploadForFrame(&pointers, sizeof(pointers));
    }

    static void bind(const Carrot::Render::PassData::HashGridResources& r, const Carrot::Render::Graph& graph, const Carrot::Render::Context& renderContext, Carrot::Pipeline& pipeline, std::size_t setID) {
        renderContext.renderer.bindBuffer(pipeline, renderContext, graph.getBuffer(r.constants, renderContext.frameCount).view, setID, 0);
        renderContext.renderer.bindBuffer(pipeline, renderContext, graph.getBuffer(r.gridPointers, renderContext.frameCount).view, setID, 1);
    }

    static Carrot::Render::PassData::HashGridResources write(Carrot::Render::GraphBuilder& graph, const Carrot::Render::PassData::HashGridResources& r) {
        Carrot::Render::PassData::HashGridResources out;
        out.constants = graph.write(r.constants, {}, {}, {}, {});
        out.gridPointers = graph.write(r.gridPointers, {}, {}, {}, {});
        out.hashGrid = graph.write(r.hashGrid, {}, {}, {}, {});
        out.hashGridUpdates = graph.write(r.hashGridUpdates, {}, {}, {}, {});
        return out;
    }

    static std::size_t computeSizeOf(std::size_t bucketCount, std::size_t cellsPerBucket) {
        const std::size_t totalCellCount = bucketCount * cellsPerBucket;
        return sizeof(Header)
        + totalCellCount * sizeof(HashCell) // pCells buffer
        + totalCellCount * sizeof(std::uint32_t) // pLastTouchedFrame buffer
        ;
    }
};

Carrot::GBuffer::GBuffer(Carrot::VulkanRenderer& renderer, Carrot::RayTracer& raytracer): renderer(renderer), raytracer(raytracer) {

}

void Carrot::GBuffer::onSwapchainImageCountChange(size_t newCount) {

}

void Carrot::GBuffer::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
    // TODO
}

Carrot::Render::Pass<Carrot::Render::PassData::GBuffer>& Carrot::GBuffer::addGBufferPass(Carrot::Render::GraphBuilder& graph, std::function<void(const Carrot::Render::CompiledPass& pass, const Carrot::Render::Context&, vk::CommandBuffer&)> opaqueCallback, const Render::TextureSize& framebufferSize) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    vk::ClearValue positionClear = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});
    vk::ClearValue clearDepth = vk::ClearDepthStencilValue{
            .depth = 1.0f,
            .stencil = 0
    };
    vk::ClearValue clearIntProperties = vk::ClearColorValue();
    vk::ClearValue clearEntityID = vk::ClearColorValue(std::array<std::uint32_t,4>{0,0,0,0});
    auto& opaquePass = graph.addPass<Carrot::Render::PassData::GBuffer>("gbuffer",
           [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::GBuffer>& pass, Carrot::Render::PassData::GBuffer& data)
           {

                data.albedo = graph.createRenderTarget("Albedo",
                                                       vk::Format::eR8G8B8A8Unorm,
                                                       framebufferSize,
                                                       vk::AttachmentLoadOp::eClear,
                                                       clearColor,
                                                       vk::ImageLayout::eColorAttachmentOptimal);

                data.positions = graph.createRenderTarget("View Positions",
                                                          vk::Format::eR32G32B32A32Sfloat,
                                                          framebufferSize,
                                                          vk::AttachmentLoadOp::eClear,
                                                          positionClear,
                                                          vk::ImageLayout::eColorAttachmentOptimal);

                data.viewSpaceNormalTangents = graph.createRenderTarget("View space normals tangents",
                                                        vk::Format::eR32G32B32A32Sfloat,
                                                        framebufferSize,
                                                        vk::AttachmentLoadOp::eClear,
                                                        positionClear,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

                data.flags = graph.createRenderTarget("Flags",
                                                      vk::Format::eR32Uint,
                                                      framebufferSize,
                                                      vk::AttachmentLoadOp::eClear,
                                                      clearIntProperties,
                                                      vk::ImageLayout::eColorAttachmentOptimal);

                data.entityID = graph.createRenderTarget("EntityID",
                                                         vk::Format::eR32G32B32A32Uint,
                                                        framebufferSize,
                                                        vk::AttachmentLoadOp::eClear,
                                                        clearEntityID,
                                                        vk::ImageLayout::eColorAttachmentOptimal);

               data.metallicRoughnessVelocityXY = graph.createRenderTarget("MetallicnessRoughness+VelocityXY",
                                                                           vk::Format::eR32G32B32A32Sfloat,
                                                                           framebufferSize,
                                                                           vk::AttachmentLoadOp::eClear,
                                                                           clearColor,
                                                                           vk::ImageLayout::eColorAttachmentOptimal);

               data.emissiveVelocityZ = graph.createRenderTarget("Emissive + Velocity Z",
                                                                 vk::Format::eR32G32B32A32Sfloat,
                                                                 framebufferSize,
                                                                 vk::AttachmentLoadOp::eClear,
                                                                 clearColor,
                                                                 vk::ImageLayout::eColorAttachmentOptimal);

               data.depthStencil = graph.createRenderTarget("Depth Stencil",
                                                            renderer.getVulkanDriver().getDepthFormat(),
                                                            framebufferSize,
                                                            vk::AttachmentLoadOp::eClear,
                                                            clearDepth,
                                                            vk::ImageLayout::eDepthStencilAttachmentOptimal);
           },
           [opaqueCallback](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::GBuffer& data, vk::CommandBuffer& buffer){
                opaqueCallback(pass, frame, buffer);
           }
    );
    return opaquePass;
}

Carrot::Render::Pass<Carrot::Render::PassData::Lighting>& Carrot::GBuffer::addLightingPass(const Carrot::Render::PassData::GBuffer& opaqueData,
                                                                                           Carrot::Render::GraphBuilder& graph,
                                                                                           const Render::TextureSize& framebufferSize) {
    using namespace Carrot::Render;
    vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});


    struct GIData {
        PassData::HashGridResources hashGrid;
    };

    auto& clearGICells = graph.addPass<GIData>("clear-gi",
        [&](GraphBuilder& graph, Pass<GIData>& pass, GIData& data) {
            pass.rasterized = false;

            data.hashGrid = HashGrid::createResources(graph);
        },
        [this](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
            TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Clear GI cells");

            auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/clear-grid", (std::uint64_t)&pass);

            frame.renderer.pushConstants("push", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds,
                HashGridTotalCellCount, (std::uint32_t)frame.frameCount);

            HashGrid::bind(data.hashGrid, pass.getGraph(), frame, *pipeline, 0);
            pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

            const std::size_t localSize = 256;
            const std::size_t groupX = (HashGridTotalCellCount + localSize-1) / localSize;
            cmds.dispatch(groupX, 1, 1);
        });
    auto& lightingPass = graph.addPass<Carrot::Render::PassData::Lighting>("lighting",
                                                             [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::Lighting>& pass, Carrot::Render::PassData::Lighting& resolveData)
           {
                pass.rasterized = false;
                pass.prerecordable = false; //TODO: since it depends on Lighting descriptor sets which may change, it is not 100% pre-recordable now
                // TODO (or it should be re-recorded when changes happen)
                resolveData.gBuffer.readFrom(graph, opaqueData, vk::ImageLayout::eShaderReadOnlyOptimal);

                 auto addDenoisingResources = [&](const char* name, vk::Format format, PassData::Denoising& data) {
                     data.noisy = graph.createStorageTarget(Carrot::sprintf("%s (noisy)", name),
                                                                     format,
                                                                     framebufferSize,
                                                                     vk::ImageLayout::eGeneral);
                     data.samples = graph.createStorageTarget(Carrot::sprintf("%s (temporal supersampling)", name),
                                                                     format,
                                                                     framebufferSize,
                                                                     vk::ImageLayout::eGeneral);
                     data.historyLength = graph.createStorageTarget(Carrot::sprintf("%s (temporal history length)", name),
                                                                     vk::Format::eR32G32B32A32Sfloat,
                                                                     framebufferSize,
                                                                     vk::ImageLayout::eGeneral);
                     data.pingPong[0] = graph.createStorageTarget(Carrot::sprintf("%s (0)", name),
                                                                     format,
                                                                     framebufferSize,
                                                                     vk::ImageLayout::eGeneral);
                     data.pingPong[1] = graph.createStorageTarget(Carrot::sprintf("%s (1)", name),
                                                                     format,
                                                                     framebufferSize,
                                                                     vk::ImageLayout::eGeneral);
                     data.iterationCount = 3;
                 };
                addDenoisingResources("Ambient Occlusion", vk::Format::eR8Unorm, resolveData.ambientOcclusion);
                addDenoisingResources("Direct Lighting", vk::Format::eR32G32B32A32Sfloat, resolveData.directLighting);

                resolveData.reflectionsNoisy = graph.createStorageTarget("Reflections (noisy)",
                                                                vk::Format::eR32G32B32A32Sfloat,
                                                                framebufferSize,
                                                                vk::ImageLayout::eGeneral);

                resolveData.hashGrid = HashGrid::write(graph, clearGICells.getData().hashGrid);
           },
           [framebufferSize, this](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::Lighting& data, vk::CommandBuffer& cmds) {
               ZoneScopedN("CPU RenderGraph lighting");
               TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "lighting");
               bool useRaytracingVersion = GetCapabilities().supportsRaytracing;

               const char* directLightingShader = useRaytracingVersion ? "lighting/direct-raytracing" : "lighting/direct-noraytracing";
               auto directLightingPipeline = renderer.getOrCreatePipeline(directLightingShader,  (std::uint64_t)&pass);

               const char* aoShader = useRaytracingVersion ? "lighting/ao-raytracing" : "lighting/ao-noraytracing";
               auto aoPipeline = renderer.getOrCreatePipeline(aoShader,  (std::uint64_t)&pass);

               const char* reflectionsShader = useRaytracingVersion ? "lighting/reflections-raytracing" : "lighting/reflections-noraytracing";
               auto reflectionsPipeline = renderer.getOrCreatePipeline(reflectionsShader,  (std::uint64_t)&pass);

               // used for randomness
               struct PushConstantNoRT {
                   std::uint32_t frameCount;
                   std::uint32_t frameWidth;
                   std::uint32_t frameHeight;
               };
               struct PushConstantRT: PushConstantNoRT {
                   bool hasTLAS = false; // used only if RT is supported by GPU. Shader compilation will strip this member in shaders where it is unused!
               } block;

               block.frameCount = renderer.getFrameCount();
               if(framebufferSize.type == Render::TextureSize::Type::SwapchainProportional) {
                   block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                   block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
               } else {
                   block.frameWidth = framebufferSize.width;
                   block.frameHeight = framebufferSize.height;
               }

               auto setupPipeline = [&](const FrameResource& output, Carrot::Pipeline& pipeline, bool needSceneInfo, int hashGridSetID) {
                   Carrot::AccelerationStructure* pTLAS = nullptr;
                   if(useRaytracingVersion) {
                       pTLAS = frame.renderer.getASBuilder().getTopLevelAS(frame);
                       block.hasTLAS = pTLAS != nullptr;
                       renderer.pushConstantBlock<PushConstantRT>("push", pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
                   } else {
                       renderer.pushConstantBlock<PushConstantNoRT>("push", pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
                   }

                   // GBuffer inputs
                   data.gBuffer.bindInputs(pipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                   // outputs
                   auto& renderGraph = pass.getGraph();
                   auto& outputImage = renderGraph.getTexture(output, frame.swapchainIndex);
                   renderer.bindStorageImage(pipeline, frame, outputImage, 5, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                   if(useRaytracingVersion) {
                       renderer.bindTexture(pipeline, frame, *renderer.getMaterialSystem().getBlueNoiseTextures()[frame.swapchainIndex % Render::BlueNoiseTextureCount]->texture, 6, 1, nullptr);
                       if(pTLAS) {
                           renderer.bindAccelerationStructure(pipeline, frame, *pTLAS, 6, 0);
                           if(needSceneInfo) {
                               renderer.bindBuffer(pipeline, frame, renderer.getASBuilder().getGeometriesBuffer(frame), 6, 2);
                               renderer.bindBuffer(pipeline, frame, renderer.getASBuilder().getInstancesBuffer(frame), 6, 3);
                           }
                       } else {
                           renderer.unbindAccelerationStructure(pipeline, frame, 6, 0);
                           if(needSceneInfo) {
                               renderer.unbindBuffer(pipeline, frame, 6, 2);
                               renderer.unbindBuffer(pipeline, frame, 6, 3);
                           }
                       }
                   }

                   if(hashGridSetID >= 0) {
                       HashGrid::bind(data.hashGrid, pass.getGraph(), frame, pipeline, hashGridSetID);
                   }
               };

               {
                   TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "RT passes");
                   const std::uint8_t localSizeX = 32;
                   const std::uint8_t localSizeY = 32;
                   std::size_t dispatchX = (block.frameWidth + (localSizeX-1)) / localSizeX;
                   std::size_t dispatchY = (block.frameHeight + (localSizeY-1)) / localSizeY;
                   setupPipeline(data.directLighting.noisy, *directLightingPipeline, false, useRaytracingVersion ? 7 : 6);
                   directLightingPipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);
                   cmds.dispatch(dispatchX, dispatchY, 1);

                   setupPipeline(data.ambientOcclusion.noisy, *aoPipeline, false, -1);
                   aoPipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);
                   cmds.dispatch(dispatchX, dispatchY, 1);

                   setupPipeline(data.reflectionsNoisy, *reflectionsPipeline, true, useRaytracingVersion ? 7 : 6);
                   reflectionsPipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);
                   cmds.dispatch(dispatchX, dispatchY, 1);
               }

               vk::MemoryBarrier2KHR memoryBarrier {
                       .srcStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                       .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                       .dstStageMask = vk::PipelineStageFlagBits2KHR::eAllCommands,
                       .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
               };
               vk::DependencyInfoKHR dependencyInfo {
                       .memoryBarrierCount = 1,
                       .pMemoryBarriers = &memoryBarrier,
               };
               {
                   TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Barrier before denoising");
                   cmds.pipelineBarrier2KHR(dependencyInfo);
               }

               auto applyDenoising = [&](const char* denoisePipelineName, const PassData::GBuffer& gBuffer, const PassData::Denoising& data, std::uint8_t index) {
                   auto temporalDenoisePipeline = renderer.getOrCreatePipeline("lighting/temporal-denoise", (std::uint64_t)&pass + index);
                   auto spatialDenoisePipelines = std::array {
                       renderer.getOrCreatePipeline(denoisePipelineName, (std::uint64_t)&pass + 0 + index*3),
                       renderer.getOrCreatePipeline(denoisePipelineName, (std::uint64_t)&pass + 1 + index*3),
                       renderer.getOrCreatePipeline(denoisePipelineName, (std::uint64_t)&pass + 2 + index*3),
                   };

                   std::array pImages = std::array {
                       &pass.getGraph().getTexture(data.pingPong[0], frame.swapchainIndex),
                       &pass.getGraph().getTexture(data.pingPong[1], frame.swapchainIndex)
                   };

                   gBuffer.bindInputs(*temporalDenoisePipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                   renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.noisy, frame.swapchainIndex), 2, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                   renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.pingPong[(data.iterationCount+1)%2], frame.lastSwapchainIndex), 2, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                   renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.samples, frame.swapchainIndex), 2, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                   renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.historyLength, frame.swapchainIndex), 2, 3, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                   renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.historyLength, frame.lastSwapchainIndex), 2, 4, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                   renderer.bindTexture(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(gBuffer.positions, frame.lastSwapchainIndex), 2, 5, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                   renderer.bindUniformBuffer(*temporalDenoisePipeline, frame, frame.pViewport->getCameraUniformBuffer(frame), 1, 0);
                   renderer.bindUniformBuffer(*temporalDenoisePipeline, frame, frame.pViewport->getCameraUniformBuffer(frame.lastFrame()), 1, 1);


                   gBuffer.bindInputs(*spatialDenoisePipelines[0], frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                   gBuffer.bindInputs(*spatialDenoisePipelines[1], frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                   renderer.bindStorageImage(*spatialDenoisePipelines[0], frame, pass.getGraph().getTexture(data.samples, frame.swapchainIndex), 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                   renderer.bindStorageImage(*spatialDenoisePipelines[0], frame, *pImages[0], 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                   renderer.bindStorageImage(*spatialDenoisePipelines[1], frame, *pImages[1], 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                   renderer.bindStorageImage(*spatialDenoisePipelines[1], frame, *pImages[0], 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                   renderer.bindStorageImage(*spatialDenoisePipelines[1], frame, *pImages[0], 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                   renderer.bindStorageImage(*spatialDenoisePipelines[1], frame, *pImages[1], 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                   {
                       const std::size_t localSizeX = 16;
                       const std::size_t localSizeY = 8;
                       std::size_t dispatchX = (block.frameWidth + (localSizeX-1)) / localSizeX;
                       std::size_t dispatchY = (block.frameHeight + (localSizeY-1)) / localSizeY;

                       TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Denoising passes");

                       struct Block {
                           std::uint32_t iterationIndex;
                       } denoiseBlock;

                       // first iteration
                       {
                           temporalDenoisePipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);
                           cmds.dispatch(dispatchX, dispatchY, 1);
                       }
                       {
                           denoiseBlock.iterationIndex = 0;
                           renderer.pushConstantBlock("push", *spatialDenoisePipelines[0], frame, vk::ShaderStageFlagBits::eCompute, cmds, denoiseBlock);
                           spatialDenoisePipelines[0]->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);
                           cmds.dispatch(dispatchX, dispatchY, 1);
                       }
                       for(std::uint8_t i = 0; i < data.iterationCount-1; i++) {
                           auto& pipeline = *spatialDenoisePipelines[i % 2 +1];
                           denoiseBlock.iterationIndex = i;
                           renderer.pushConstantBlock("push", pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, denoiseBlock);
                           pipeline.bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);
                           cmds.dispatch(dispatchX, dispatchY, 1);
                       }

                       vk::MemoryBarrier2KHR memoryBarrier {
                           .srcStageMask = vk::PipelineStageFlagBits2KHR::eComputeShader,
                           .srcAccessMask = vk::AccessFlagBits2KHR::eShaderWrite,
                           .dstStageMask = vk::PipelineStageFlagBits2KHR::eAllCommands,
                           .dstAccessMask = vk::AccessFlagBits2KHR::eShaderRead,
                       };
                       vk::DependencyInfoKHR dependencyInfo {
                               .memoryBarrierCount = 1,
                               .pMemoryBarriers = &memoryBarrier,
                       };
                       {
                           TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Barrier between a-trous filter iterations");
                           cmds.pipelineBarrier2KHR(dependencyInfo);
                       }
                   }
                };
               applyDenoising("lighting/denoise-ao", data.gBuffer, data.ambientOcclusion, 0);
               applyDenoising("lighting/denoise-direct", data.gBuffer, data.directLighting, 0);
           },
           [&](const CompiledPass& pass, const PassData::Lighting& data) {
           }
    );

    lightingPass.setSwapchainRecreation([&](const CompiledPass& pass, const PassData::Lighting& data) {
        GetVulkanDriver().performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
            for(int i = 0; i < GetEngine().getSwapchainImageCount(); i++) {
                auto clear = [&](const PassData::Denoising& data) {
                    auto& texture = pass.getGraph().getTexture(data.historyLength, i);
                    cmds.clearColorImage(texture.getVulkanImage(), vk::ImageLayout::eGeneral, vk::ClearColorValue{std::array {0.0f, 0.0f, 0.0f, 0.0f} },
                        vk::ImageSubresourceRange {
                            .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .baseMipLevel = 0,
                            .levelCount = 1,
                            .baseArrayLayer = 0,
                            .layerCount = 1,
                        });
                };
                clear(data.ambientOcclusion);
                clear(data.directLighting);
            }
        });

        HashGrid::prepareBuffers(pass.getGraph(), data.hashGrid);
    });
    auto& decayGICells = graph.addPass<GIData>("decay-gi",
        [&](GraphBuilder& graph, Pass<GIData>& pass, GIData& data) {
            pass.rasterized = false;
            data.hashGrid = HashGrid::write(graph, lightingPass.getData().hashGrid);
        },
        [this](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
            TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Decay GI cells");

            auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/decay-cells", (std::uint64_t)&pass);

            frame.renderer.pushConstants("push", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds,
                HashGridTotalCellCount, (std::uint32_t)frame.frameCount);

            HashGrid::bind(data.hashGrid, pass.getGraph(), frame, *pipeline, 0);
            pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

            const std::size_t localSize = 256;
            const std::size_t groupX = (HashGridTotalCellCount + localSize-1) / localSize;
            cmds.dispatch(groupX, 1, 1);
        });
    auto updateGICells = graph.addPass<GIData>(Carrot::sprintf("update-gi"),
        [&](GraphBuilder& graph, Pass<GIData>& pass, GIData& data) {
            pass.rasterized = false;
            data.hashGrid = HashGrid::write(graph, decayGICells.getData().hashGrid);
        },
        [this, framebufferSize](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
            TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Update GI cells");

            auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/update-cells", (std::uint64_t)&pass);
            // used for randomness
            struct PushConstantNoRT {
                std::uint32_t frameCount;
                std::uint32_t frameWidth;
                std::uint32_t frameHeight;
            };
            struct PushConstantRT: PushConstantNoRT {
                VkBool32 hasTLAS = false; // used only if RT is supported by GPU. Shader compilation will strip this member in shaders where it is unused!
            } block;

            block.frameCount = renderer.getFrameCount();
            if(framebufferSize.type == Render::TextureSize::Type::SwapchainProportional) {
                block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
            } else {
                block.frameWidth = framebufferSize.width;
                block.frameHeight = framebufferSize.height;
            }

            bool useRaytracingVersion = true; // TODO
            Carrot::AccelerationStructure* pTLAS = nullptr;
            if(useRaytracingVersion) {
                pTLAS = frame.renderer.getASBuilder().getTopLevelAS(frame);
                block.hasTLAS = pTLAS != nullptr;
                renderer.pushConstantBlock<PushConstantRT>("push", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
            } else {
                renderer.pushConstantBlock<PushConstantNoRT>("push", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
            }
            bool needSceneInfo = true;
            if(useRaytracingVersion) {
               renderer.bindTexture(*pipeline, frame, *renderer.getMaterialSystem().getBlueNoiseTextures()[frame.swapchainIndex % Render::BlueNoiseTextureCount]->texture, 6, 1, nullptr);
               if(pTLAS) {
                   renderer.bindAccelerationStructure(*pipeline, frame, *pTLAS, 6, 0);
                   if(needSceneInfo) {
                       renderer.bindBuffer(*pipeline, frame, renderer.getASBuilder().getGeometriesBuffer(frame), 6, 2);
                       renderer.bindBuffer(*pipeline, frame, renderer.getASBuilder().getInstancesBuffer(frame), 6, 3);
                   }
               } else {
                   renderer.unbindAccelerationStructure(*pipeline, frame, 6, 0);
                   if(needSceneInfo) {
                       renderer.unbindBuffer(*pipeline, frame, 6, 2);
                       renderer.unbindBuffer(*pipeline, frame, 6, 3);
                   }
               }
            }

            Render::Texture::Ref skyboxCubeMap = GetEngine().getSkyboxCubeMap();
            if(!skyboxCubeMap || GetEngine().getSkybox() == Carrot::Skybox::Type::None) {
                skyboxCubeMap = renderer.getBlackCubeMapTexture();
            }

            renderer.bindTexture(*pipeline, frame, *skyboxCubeMap, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube, 0);
            HashGrid::bind(data.hashGrid, pass.getGraph(), frame, *pipeline, 0);
            pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

            const std::size_t localSize = 256;
            const std::size_t groupX = (HashGridTotalCellCount + localSize-1) / localSize;
            cmds.dispatch(groupX, 1, 1);
        });

    auto& reuseGICells = graph.addPass<GIData>("reuse-gi",
        [&](GraphBuilder& graph, Pass<GIData>& pass, GIData& data) {
            pass.rasterized = false;
            data.hashGrid = HashGrid::write(graph, updateGICells.getData().hashGrid);
        },
        [this](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
            TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Reuse GI cells");

            auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/reuse-gi", (std::uint64_t)&pass);

            frame.renderer.pushConstants("push", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds,
                HashGridTotalCellCount, (std::uint32_t)frame.frameCount);

            HashGrid::bind(data.hashGrid, pass.getGraph(), frame, *pipeline, 0);
            pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

            const std::size_t localSize = 256;
            const std::size_t groupX = (HashGridTotalCellCount + localSize-1) / localSize;
            cmds.dispatch(groupX, 1, 1);
        });

    // TODO: disableable
    struct GIDebug {
        PassData::GBuffer gbuffer;
        GIData gi;
        FrameResource output;
    };
    auto& debugGICells = graph.addPass<GIDebug>("debug-gi",
        [&](GraphBuilder& graph, Pass<GIDebug>& pass, GIDebug& data) {
            data.gbuffer.readFrom(graph, lightingPass.getData().gBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
            data.gi.hashGrid = HashGrid::write(graph, reuseGICells.getData().hashGrid);
            data.output = graph.createStorageTarget("gi-debug", vk::Format::eR8G8B8A8Unorm, framebufferSize, vk::ImageLayout::eGeneral);
        },
        [this](const Render::CompiledPass& pass, const Render::Context& frame, const GIDebug& data, vk::CommandBuffer& cmds) {
            TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Debug GI cells");

            auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/debug-gi", (std::uint64_t)&pass);
            auto& outputTexture = pass.getGraph().getTexture(data.output, frame.swapchainIndex);

            HashGrid::bind(data.gi.hashGrid, pass.getGraph(), frame, *pipeline, 0);
            frame.renderer.bindStorageImage(*pipeline, frame, outputTexture, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
            data.gbuffer.bindInputs(*pipeline, frame, pass.getGraph(), 2, vk::ImageLayout::eShaderReadOnlyOptimal);
            pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

            const std::size_t localSize = 32;
            const std::size_t groupX = (outputTexture.getSize().width + localSize-1) / localSize;
            const std::size_t groupY = (outputTexture.getSize().height + localSize-1) / localSize;
            cmds.dispatch(groupX, groupY, 1);
        });

    return lightingPass;
}

void Carrot::Render::PassData::GBuffer::readFrom(Render::GraphBuilder& graph, const GBuffer& other, vk::ImageLayout wantedLayout) {
    albedo = graph.read(other.albedo, wantedLayout);
    positions = graph.read(other.positions, wantedLayout);
    viewSpaceNormalTangents = graph.read(other.viewSpaceNormalTangents, wantedLayout);
    flags = graph.read(other.flags, wantedLayout);
    entityID = graph.read(other.entityID, wantedLayout);
    metallicRoughnessVelocityXY = graph.read(other.metallicRoughnessVelocityXY, wantedLayout);
    emissiveVelocityZ = graph.read(other.emissiveVelocityZ, wantedLayout);
    depthStencil = graph.read(other.depthStencil, vk::ImageLayout::eDepthStencilReadOnlyOptimal, vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);

    // TODO: fix (double read in two != passes result in no 'previousLayout' change
    depthStencil.previousLayout = depthStencil.layout;
    albedo.previousLayout = albedo.layout;
}

void Carrot::Render::PassData::GBuffer::writeTo(Render::GraphBuilder& graph, const GBuffer& other) {
    albedo = graph.write(other.albedo, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    positions = graph.write(other.positions, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    viewSpaceNormalTangents = graph.write(other.viewSpaceNormalTangents, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    flags = graph.write(other.flags, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    entityID = graph.write(other.entityID, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    metallicRoughnessVelocityXY = graph.write(other.metallicRoughnessVelocityXY, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    emissiveVelocityZ = graph.write(other.emissiveVelocityZ, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eColorAttachmentOptimal);
    depthStencil = graph.write(other.depthStencil, vk::AttachmentLoadOp::eLoad, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    // TODO: fix (double read in two != passes result in no 'previousLayout' change
    depthStencil.previousLayout = depthStencil.layout;
    albedo.previousLayout = albedo.layout;
}

void Carrot::Render::PassData::GBuffer::bindInputs(Carrot::Pipeline& pipeline, const Render::Context& frame, const Render::Graph& renderGraph, std::uint32_t setID, vk::ImageLayout expectedLayout) const {
    auto& renderer = GetRenderer();
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(albedo, frame.swapchainIndex), setID, 0, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(positions, frame.swapchainIndex), setID, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(viewSpaceNormalTangents, frame.swapchainIndex), setID, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(flags, frame.swapchainIndex), setID, 3, renderer.getVulkanDriver().getNearestSampler(), vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(entityID, frame.swapchainIndex), setID, 4, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(metallicRoughnessVelocityXY, frame.swapchainIndex), setID, 5, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(emissiveVelocityZ, frame.swapchainIndex), setID, 6, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, expectedLayout);

    vk::ImageLayout depthLayout = vk::ImageLayout::eDepthStencilReadOnlyOptimal;
    if(expectedLayout == vk::ImageLayout::eGeneral) {
        depthLayout = vk::ImageLayout::eGeneral;
    } else if(expectedLayout == vk::ImageLayout::eColorAttachmentOptimal) {
        depthLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
    }
    // 7 unused
    renderer.bindTexture(pipeline, frame, renderGraph.getTexture(depthStencil, frame.swapchainIndex), setID, 8, nullptr, vk::ImageAspectFlagBits::eDepth, vk::ImageViewType::e2D, 0, depthLayout);

    Render::Texture::Ref skyboxCubeMap = GetEngine().getSkyboxCubeMap();
    if(!skyboxCubeMap || GetEngine().getSkybox() == Carrot::Skybox::Type::None) {
        skyboxCubeMap = renderer.getBlackCubeMapTexture();
    }
    renderer.bindTexture(pipeline, frame, *skyboxCubeMap, setID, 9, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::eCube);

    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getLinearSampler(), setID, 10);
    renderer.bindSampler(pipeline, frame, renderer.getVulkanDriver().getNearestSampler(), setID, 11);
}
