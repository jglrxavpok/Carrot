//
// Created by jglrxavpok on 09/12/2024.
//

#include "LightingPasses.h"

#include <core/math/BasicFunctions.h>
#include <engine/render/TextureRepository.h>

#include "engine/render/VulkanRenderer.h"
#include "engine/render/RenderGraph.h"
#include "engine/render/RenderContext.h"
#include "engine/render/raytracing/RaytracingScene.h"
#include "engine/utils/Profiling.h"
#include "engine/Engine.h"

namespace Carrot::Render {

    enum class NeighborClampingType {
        eNone,
        e5x5,
    };

    // keep in sync with gi.slang
    static constexpr std::uint64_t HashGridCellsPerBucket = 32;
    static constexpr std::uint64_t HashGridBucketCount = 1024*256;
    static constexpr std::uint64_t HashGridTotalCellCount = HashGridBucketCount*HashGridCellsPerBucket;

    struct HashGrid {
        struct HashCellKey {
            glm::vec3 hitPosition;
            glm::vec3 direction;
            glm::vec3 cameraPos;
            float rayLength;
        };

        struct StorageHeader {
            vk::DeviceAddress keys;
            vk::DeviceAddress hash2;
            vk::DeviceAddress radiance;
            vk::DeviceAddress sampleCounts;
            vk::DeviceAddress radianceAccumulation;
            vk::DeviceAddress sampleCountAccumulation;
            vk::DeviceAddress lastTouchedFrames;
        };

        // from gi.slang
        static constexpr std::uint32_t SizeOfHashCell =
            sizeof(HashCellKey)
            + sizeof(std::uint32_t)
            + sizeof(glm::ivec3)
            + sizeof(std::uint32_t)
            + sizeof(glm::ivec3)
            + sizeof(std::uint32_t)
        ;
        static constexpr u32 SizeOfHashCellWithLastTouchedFrame = SizeOfHashCell + sizeof(u32);

        static Carrot::Render::PassData::HashGridResources createResources(Carrot::Render::GraphBuilder& graph) {
            Carrot::Render::PassData::HashGridResources r;

            const std::size_t hashGridSize = computeSizeOf(HashGridBucketCount, HashGridCellsPerBucket);
            r.hashGrid = graph.createBuffer("GI probes hashmap", hashGridSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc, false/*we want to keep the header*/);
            return r;
        }

        static void prepareBuffers(const Carrot::Render::Graph& graph, const Carrot::Render::PassData::HashGridResources& r) {
            BufferView hashGridBuffer = graph.getBuffer(r.hashGrid, 0).view;
            StorageHeader header{};
            header.keys = hashGridBuffer.getDeviceAddress() + sizeof(StorageHeader);
            header.hash2 = header.keys + sizeof(HashCellKey) * HashGridTotalCellCount;
            header.radiance = header.hash2 + sizeof(u32) * HashGridTotalCellCount;
            header.sampleCounts = header.radiance + sizeof(glm::vec3) * HashGridTotalCellCount;
            header.radianceAccumulation = header.sampleCounts + sizeof(u32) * HashGridTotalCellCount;
            header.sampleCountAccumulation = header.radianceAccumulation + sizeof(i32) * 3 * HashGridTotalCellCount;
            header.lastTouchedFrames = header.sampleCountAccumulation + sizeof(u32) * HashGridTotalCellCount;

            assert(header.lastTouchedFrames + sizeof(u32) * HashGridTotalCellCount == hashGridBuffer.getDeviceAddress() + hashGridBuffer.getSize());

            hashGridBuffer.uploadForFrame(&header, sizeof(header));
        }

        static void bind(const Carrot::Render::PassData::HashGridResources& r, const Carrot::Render::Graph& graph, const Carrot::Render::Context& renderContext, Carrot::Pipeline& pipeline, std::size_t setID) {
            pipeline.setStorageBuffer(renderContext, "hashGridData", graph.getBuffer(r.hashGrid, renderContext.frameNumber).view);
        }

        static Carrot::Render::PassData::HashGridResources write(Carrot::Render::GraphBuilder& graph, const Carrot::Render::PassData::HashGridResources& r) {
            Carrot::Render::PassData::HashGridResources out;
            out.hashGrid = graph.write(r.hashGrid, {}, {}, {}, {});
            return out;
        }

        static std::size_t computeSizeOf(std::size_t bucketCount, std::size_t cellsPerBucket) {
            const std::size_t totalCellCount = bucketCount * cellsPerBucket;
            return totalCellCount * SizeOfHashCellWithLastTouchedFrame + sizeof(StorageHeader);
        }
    };

    PassData::Lighting addLightingPasses(const Carrot::Render::PassData::GBuffer& opaqueData,
                                                                                               Carrot::Render::GraphBuilder& graph,
                                                                                               const Render::TextureSize& framebufferSize) {
        using namespace Carrot::Render;

        struct GIData {
            PassData::HashGridResources hashGrid;
        };

        auto addTemporalDenoisingResources = [&](Render::GraphBuilder& graph, const char* name, vk::Format format, PassData::TemporalDenoising& data) {
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
             graph.getVulkanDriver().getEngine().getResourceRepository().getTextureUsages(data.historyLength.rootID) |= vk::ImageUsageFlagBits::eTransferDst;

            // used in temporal algorithms
             graph.reuseResourceAcrossFrames(data.historyLength, 1);
             graph.reuseResourceAcrossFrames(data.samples, 1);
         };
        auto addSpatialDenoisingResources = [&](Render::GraphBuilder& graph, const char* name, vk::Format format, PassData::SpatialDenoising& data) {
             data.pingPong[0] = graph.createStorageTarget(Carrot::sprintf("%s (0)", name),
                                                             format,
                                                             framebufferSize,
                                                             vk::ImageLayout::eGeneral);
             data.pingPong[1] = graph.createStorageTarget(Carrot::sprintf("%s (1)", name),
                                                             format,
                                                             framebufferSize,
                                                             vk::ImageLayout::eGeneral);

            // used in temporal algorithms
             graph.reuseResourceAcrossFrames(data.pingPong[0], 1);
             graph.reuseResourceAcrossFrames(data.pingPong[1], 1);
             data.iterationCount = 3;
         };
        auto applyTemporalDenoising = [framebufferSize](const CompiledPass& pass,
            const Carrot::Render::Context& frame,
            const PassData::GBuffer& gBuffer,

            // may differ from gbuffer to account for bounces
            const FrameResource& positionsTex,
            const FrameResource& normalsTangentsTex,

            const PassData::TemporalDenoising& data,
            std::uint8_t index,

            vk::ImageLayout gBufferLayout,

            NeighborClampingType neighborClampingType, // has different neighbor clamping
            vk::CommandBuffer& cmds) {
            auto& renderer = GetRenderer();

            // used for randomness
            struct PushConstantNoRT {
                std::uint32_t frameCount;
                std::uint32_t frameWidth;
                std::uint32_t frameHeight;
            };
            struct PushConstantRT : PushConstantNoRT {
                bool hasTLAS = false;
                // used only if RT is supported by GPU. Shader compilation will strip this member in shaders where it is unused!
            } block;

            block.frameCount = renderer.getFrameCount();
            if (framebufferSize.type == Render::TextureSize::Type::ViewportProportional) {
                block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
            } else {
                block.frameWidth = framebufferSize.width;
                block.frameHeight = framebufferSize.height;
            }

            auto temporalDenoisePipeline = renderer.getOrCreatePipeline("lighting/temporal-denoise",
                                                                        (std::uint64_t)&pass + index);
            gBuffer.bindInputs(*temporalDenoisePipeline, frame, pass.getGraph(), 0,
                               gBufferLayout);
            renderer.bindTexture(*temporalDenoisePipeline, frame,
                                 pass.getGraph().getTexture(positionsTex, frame.frameNumber), 0, 1, nullptr,
                                 vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, positionsTex.layout);
            renderer.bindTexture(*temporalDenoisePipeline, frame,
                                 pass.getGraph().getTexture(normalsTangentsTex, frame.frameNumber), 0, 2, nullptr,
                                 vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, normalsTangentsTex.layout);

            renderer.bindStorageImage(*temporalDenoisePipeline, frame,
                                      pass.getGraph().getTexture(data.noisy, frame.frameNumber), 2, 0,
                                      vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0,
                                      vk::ImageLayout::eGeneral);
            renderer.bindStorageImage(*temporalDenoisePipeline, frame,
                                      pass.getGraph().getTexture(data.samples, frame.frameNumber), 2, 2,
                                      vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0,
                                      vk::ImageLayout::eGeneral);
            renderer.bindStorageImage(*temporalDenoisePipeline, frame,
                                      pass.getGraph().getTexture(data.historyLength, frame.frameNumber), 2, 3,
                                      vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0,
                                      vk::ImageLayout::eGeneral);

            renderer.bindTexture(*temporalDenoisePipeline, frame,
                                 pass.getGraph().getTexture(positionsTex, frame.getPreviousFrameNumber()), 2, 5,
                                 nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0,
                                 positionsTex.layout);
            renderer.bindStorageImage(*temporalDenoisePipeline, frame,
                                      pass.getGraph().getTexture(data.historyLength, frame.getPreviousFrameNumber()), 2,
                                      4, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0,
                                      vk::ImageLayout::eGeneral);

            renderer.bindStorageImage(*temporalDenoisePipeline, frame,
                                      pass.getGraph().getTexture(data.samples, frame.getPreviousFrameNumber()), 2,
                                      1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0,
                                      vk::ImageLayout::eGeneral);

            {
                const std::size_t localSizeX = 16;
                const std::size_t localSizeY = 8;
                std::size_t dispatchX = (block.frameWidth + (localSizeX - 1)) / localSizeX;
                std::size_t dispatchY = (block.frameHeight + (localSizeY - 1)) / localSizeY;

                GPUZoneColored(GetEngine().tracyCtx[frame.frameIndex], cmds, "Temporal accumulation", glm::vec4(0,0,1,1));

                {
                    GPUZoneColored(GetEngine().tracyCtx[frame.frameIndex], cmds, "Temporal pass", glm::vec4(0,0,1,1));
                    renderer.pushConstants("push", *temporalDenoisePipeline, frame, vk::ShaderStageFlagBits::eCompute,
                                           cmds, (u32)neighborClampingType);
                    temporalDenoisePipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds,
                                                  vk::PipelineBindPoint::eCompute);
                    cmds.dispatch(dispatchX, dispatchY, 1);
                }
            }
        };
        auto applySpatialDenoising = [framebufferSize](
            const CompiledPass& pass,
            const Carrot::Render::Context& frame,
            const char* denoisePipelineName,
            const PassData::GBuffer& gBuffer,

            // may differ from gbuffer to account for bounces
            const FrameResource& positionsTex,
            const FrameResource& normalsTangentsTex,

            const PassData::SpatialDenoising& data,
            std::uint8_t index,

            vk::CommandBuffer& cmds) {
               auto& renderer = GetRenderer();

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
                if(framebufferSize.type == Render::TextureSize::Type::ViewportProportional) {
                    block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                    block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
                } else {
                    block.frameWidth = framebufferSize.width;
                    block.frameHeight = framebufferSize.height;
                }

               auto spatialDenoisePipelines = std::array {
                   renderer.getOrCreatePipeline(denoisePipelineName, (std::uint64_t)&pass + 0 + index*3),
                   renderer.getOrCreatePipeline(denoisePipelineName, (std::uint64_t)&pass + 1 + index*3),
                   renderer.getOrCreatePipeline(denoisePipelineName, (std::uint64_t)&pass + 2 + index*3),
               };

               std::array pImages = std::array {
                   &pass.getGraph().getTexture(data.pingPong[0], frame.frameNumber),
                   &pass.getGraph().getTexture(data.pingPong[1], frame.frameNumber)
               };

                for (auto& pSpatialDenoisePipeline : spatialDenoisePipelines) {
                    gBuffer.bindInputs(*pSpatialDenoisePipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eGeneral);
                    renderer.bindTexture(*pSpatialDenoisePipeline, frame, pass.getGraph().getTexture(positionsTex, frame.frameNumber), 0, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, positionsTex.layout);
                    renderer.bindTexture(*pSpatialDenoisePipeline, frame, pass.getGraph().getTexture(normalsTangentsTex, frame.frameNumber), 0, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, normalsTangentsTex.layout);
                }

               renderer.bindStorageImage(*spatialDenoisePipelines[0], frame, pass.getGraph().getTexture(data.noisy, frame.frameNumber), 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
               renderer.bindStorageImage(*spatialDenoisePipelines[0], frame, *pImages[0], 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

               renderer.bindStorageImage(*spatialDenoisePipelines[1], frame, *pImages[0], 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
               renderer.bindStorageImage(*spatialDenoisePipelines[1], frame, *pImages[1], 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

               renderer.bindStorageImage(*spatialDenoisePipelines[2], frame, *pImages[1], 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
               renderer.bindStorageImage(*spatialDenoisePipelines[2], frame, *pImages[0], 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

               {
                   const std::size_t localSizeX = 16;
                   const std::size_t localSizeY = 8;
                   std::size_t dispatchX = (block.frameWidth + (localSizeX-1)) / localSizeX;
                   std::size_t dispatchY = (block.frameHeight + (localSizeY-1)) / localSizeY;

                   GPUZoneColored(GetEngine().tracyCtx[frame.frameIndex], cmds, "Spatial denoise", glm::vec4(0,0,1,1));

                   struct Block {
                       std::uint32_t iterationIndex;
                   } denoiseBlock;

                   // first iteration
                   {
                       GPUZoneColored(GetEngine().tracyCtx[frame.frameIndex], cmds, "Spatial pass 0", glm::vec4(0,0,1,1));
                       denoiseBlock.iterationIndex = 0;
                       renderer.pushConstantBlock("entryPointParams", *spatialDenoisePipelines[0], frame, vk::ShaderStageFlagBits::eCompute, cmds, denoiseBlock);
                       spatialDenoisePipelines[0]->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                       cmds.dispatch(dispatchX, dispatchY, 1);
                   }
                   for(std::uint8_t i = 0; i < data.iterationCount-1; i++) {
                       GPUZoneColored(GetEngine().tracyCtx[frame.frameIndex], cmds, "Spatial pass i", glm::vec4(0,0,1,1));
                       auto& pipeline = *spatialDenoisePipelines[i % 2 +1];
                       denoiseBlock.iterationIndex = i;
                       renderer.pushConstantBlock("entryPointParams", pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, denoiseBlock);
                       pipeline.bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
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
                       GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Barrier between a-trous filter iterations");
                       cmds.pipelineBarrier2KHR(dependencyInfo);
                   }
               }
            };

        auto& lightingPass = graph.addPass<Carrot::Render::PassData::LightingResources>("lighting",
                                                                 [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::LightingResources>& pass, Carrot::Render::PassData::LightingResources& resolveData)
               {
                    pass.rasterized = false;
                    resolveData.gBuffer.writeTo(graph, opaqueData, vk::ImageLayout::eGeneral);

                    addTemporalDenoisingResources(graph, "Ambient Occlusion", vk::Format::eR8Unorm, resolveData.ambientOcclusionTemporal);
                    addSpatialDenoisingResources(graph, "Ambient Occlusion", vk::Format::eR8Unorm, resolveData.ambientOcclusionSpatial);
                    resolveData.ambientOcclusionSpatial.noisy = resolveData.ambientOcclusionTemporal.samples;
                    resolveData.directLighting = graph.createStorageTarget("Direct Lighting", vk::Format::eR32G32B32A32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                    resolveData.reflections = graph.createStorageTarget("Reflections", vk::Format::eR32G32B32A32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
               },
               [framebufferSize, applyTemporalDenoising, applySpatialDenoising](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::LightingResources& data, vk::CommandBuffer& cmds) {
                   ZoneScopedN("CPU RenderGraph lighting");
                   GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "lighting");
                   bool useRaytracingVersion = GetCapabilities().supportsRaytracing;

                   auto& renderer = frame.renderer;
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
                   if(framebufferSize.type == Render::TextureSize::Type::ViewportProportional) {
                       block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                       block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
                   } else {
                       block.frameWidth = framebufferSize.width;
                       block.frameHeight = framebufferSize.height;
                   }

                   auto setupPipeline = [&](const FrameResource& output, Carrot::Pipeline& pipeline, bool needSceneInfo, bool isRWGBuffer, const char* pushConstantName) {
                       Carrot::AccelerationStructure* pTLAS = nullptr;
                       if(useRaytracingVersion) {
                           pTLAS = frame.renderer.getRaytracingScene().getTopLevelAS(frame);
                           block.hasTLAS = pTLAS != nullptr;
                           renderer.pushConstantBlock<PushConstantRT>(pushConstantName, pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
                       } else {
                           renderer.pushConstantBlock<PushConstantNoRT>(pushConstantName, pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
                       }

                       // GBuffer inputs
                       if (isRWGBuffer) {
                           data.gBuffer.bindRW(pipeline, frame, pass.getGraph(), 0);
                       } else {
                           data.gBuffer.bindInputs(pipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eGeneral);
                       }

                       // outputs
                       auto& renderGraph = pass.getGraph();
                       auto& outputImage = renderGraph.getTexture(output, frame.frameNumber);
                       renderer.bindStorageImage(pipeline, frame, outputImage, 5, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                       if(useRaytracingVersion) {
                           renderer.bindTexture(pipeline, frame, *renderer.getMaterialSystem().getBlueNoiseTextures()[frame.frameNumber % Render::BlueNoiseTextureCount]->texture, 6, 1, nullptr);
                           if(pTLAS) {
                               renderer.bindAccelerationStructure(pipeline, frame, *pTLAS, 6, 0);
                               if(needSceneInfo) {
                                   renderer.bindBuffer(pipeline, frame, renderer.getRaytracingScene().getGeometriesBuffer(frame), 6, 2);
                                   renderer.bindBuffer(pipeline, frame, renderer.getRaytracingScene().getInstancesBuffer(frame), 6, 3);
                               }
                           } else {
                               renderer.unbindAccelerationStructure(pipeline, frame, 6, 0);
                               if(needSceneInfo) {
                                   renderer.unbindBuffer(pipeline, frame, 6, 2);
                                   renderer.unbindBuffer(pipeline, frame, 6, 3);
                               }
                           }
                       }
                   };

                   {
                       GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "RT passes");
                       const std::uint8_t localSizeX = 8;
                       const std::uint8_t localSizeY = 8;
                       std::size_t dispatchX = (block.frameWidth + (localSizeX-1)) / localSizeX;
                       std::size_t dispatchY = (block.frameHeight + (localSizeY-1)) / localSizeY;
                       setupPipeline(data.directLighting, *directLightingPipeline, false, false, "entryPointParams");
                       directLightingPipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                       cmds.dispatch(dispatchX, dispatchY, 1);

                       setupPipeline(data.ambientOcclusionTemporal.noisy, *aoPipeline, false, false, "entryPointParams");
                       aoPipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                       cmds.dispatch(dispatchX, dispatchY, 1);

                       setupPipeline(data.reflections, *reflectionsPipeline, true, true, "entryPointParams");
                       reflectionsPipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
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
                       GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Barrier before denoising");
                       cmds.pipelineBarrier2KHR(dependencyInfo);
                   }

                   applyTemporalDenoising(pass, frame, data.gBuffer, data.gBuffer.positions, data.gBuffer.viewSpaceNormalTangents, data.ambientOcclusionTemporal, 0, vk::ImageLayout::eGeneral, NeighborClampingType::e5x5, cmds);
                   applySpatialDenoising(pass, frame, "lighting/denoise-ao", data.gBuffer, data.gBuffer.positions, data.gBuffer.viewSpaceNormalTangents, data.ambientOcclusionSpatial, 0, cmds);
               }
        );

        lightingPass.setSwapchainRecreation([&](const CompiledPass& pass, const PassData::LightingResources& data) {
            GetVulkanDriver().performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
                for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                    auto clear = [&](const PassData::TemporalDenoising& data) {
                        auto& texture = pass.getGraph().getTexture(data.historyLength, i);
                        texture.assumeLayout(vk::ImageLayout::eUndefined);
                        texture.transitionInline(cmds, vk::ImageLayout::eGeneral);
                        cmds.clearColorImage(texture.getVulkanImage(), vk::ImageLayout::eGeneral, vk::ClearColorValue{std::array {0.0f, 0.0f, 0.0f, 0.0f} },
                            vk::ImageSubresourceRange {
                                .aspectMask = vk::ImageAspectFlagBits::eColor,
                                .baseMipLevel = 0,
                                .levelCount = 1,
                                .baseArrayLayer = 0,
                                .layerCount = 1,
                            });
                    };
                    clear(data.ambientOcclusionTemporal);
                }
            });

        });

        auto& decayGICells = graph.addPass<GIData>("decay-gi",
            [&](GraphBuilder& graph, Pass<GIData>& pass, GIData& data) {
                pass.rasterized = false;
                data.hashGrid = HashGrid::createResources(graph);
            },
            [](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Decay GI cells");

                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/decay-cells", (std::uint64_t)&pass);

                const BufferAllocation& hashGridBuffer = pass.getGraph().getBuffer(data.hashGrid.hashGrid, frame.frameNumber);
                frame.renderer.pushConstants("entryPointParams", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds,
                    (std::uint32_t)HashGridTotalCellCount, (std::uint32_t)frame.frameNumber);

                HashGrid::bind(data.hashGrid, pass.getGraph(), frame, *pipeline, 0);
                pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const std::size_t localSize = 32;
                const std::size_t groupX = (HashGridTotalCellCount + localSize-1) / localSize;
                cmds.dispatch(groupX, 1, 1);
            });

        decayGICells.setSwapchainRecreation([](const CompiledPass& pass, const GIData& data) {
            HashGrid::prepareBuffers(pass.getGraph(), data.hashGrid);
        });

        // used for randomness
        struct PushConstantNoRT {
            std::uint32_t frameCount;
            std::uint32_t frameWidth;
            std::uint32_t frameHeight;
        };
        struct PushConstantRT: PushConstantNoRT {
            VkBool32 hasTLAS = false; // used only if RT is supported by GPU. Shader compilation will strip this member in shaders where it is unused!
        };

        auto preparePushConstant = [framebufferSize](PushConstantNoRT& block, const Carrot::Render::Context& frame) {
            block.frameCount = GetRenderer().getFrameCount();
            if(framebufferSize.type == Render::TextureSize::Type::ViewportProportional) {
                block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
            } else {
                block.frameWidth = framebufferSize.width;
                block.frameHeight = framebufferSize.height;
            }
        };

        struct GIUpdateData {
            GIData gi;
            PassData::GBuffer gbuffer;
        };

        auto bindBaseGIUpdateInputs = [preparePushConstant](PushConstantRT& block, bool needRaytracing, const GIUpdateData& data, const Render::Graph& graph, const Render::Context& frame, Carrot::Pipeline& pipeline, vk::CommandBuffer& cmds) {
            auto& renderer = frame.renderer;
            preparePushConstant(block, frame);

            bool useRaytracingVersion = GetCapabilities().supportsRaytracing;
            Carrot::AccelerationStructure* pTLAS = nullptr;
            if (pipeline.getPushConstant("entryPointParams").offset != (u32)-1) {
                if(useRaytracingVersion) {
                    pTLAS = frame.renderer.getRaytracingScene().getTopLevelAS(frame);
                    block.hasTLAS = pTLAS != nullptr;
                    renderer.pushConstantBlock<PushConstantRT>("entryPointParams", pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
                } else {
                    renderer.pushConstantBlock<PushConstantNoRT>("entryPointParams", pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
                }
            }
            bool needSceneInfo = true;
            if(useRaytracingVersion && needRaytracing) {
               renderer.bindTexture(pipeline, frame, *renderer.getMaterialSystem().getBlueNoiseTextures()[frame.frameNumber % Render::BlueNoiseTextureCount]->texture, 7, 1, nullptr);
               if(pTLAS) {
                   renderer.bindAccelerationStructure(pipeline, frame, *pTLAS, 7, 0);
                   if(needSceneInfo) {
                       renderer.bindBuffer(pipeline, frame, renderer.getRaytracingScene().getGeometriesBuffer(frame), 7, 2);
                       renderer.bindBuffer(pipeline, frame, renderer.getRaytracingScene().getInstancesBuffer(frame), 7, 3);
                   }
               } else {
                   renderer.unbindAccelerationStructure(pipeline, frame, 7, 0);
                   if(needSceneInfo) {
                       renderer.unbindBuffer(pipeline, frame, 7, 2);
                       renderer.unbindBuffer(pipeline, frame, 7, 3);
                   }
               }
            }

            Render::Texture::Ref skyboxCubeMap = GetEngine().getSkyboxCubeMap();
            if(!skyboxCubeMap || GetEngine().getSkybox() == Carrot::Skybox::Type::None) {
                skyboxCubeMap = renderer.getBlackCubeMapTexture();
            }

            HashGrid::bind(data.gi.hashGrid, graph, frame, pipeline, 0);
            data.gbuffer.bindInputs(pipeline, frame, graph, 4, vk::ImageLayout::eGeneral);
            data.gbuffer.bindLastFrameInputs(pipeline, frame, graph, 5, vk::ImageLayout::eGeneral);
        };

        auto fillSurfaceCache = graph.addPass<GIUpdateData>("fill-surface-cache",
            [&lightingPass, &decayGICells](GraphBuilder& graph, Pass<GIUpdateData>& pass, GIUpdateData& data) {
                pass.rasterized = false;

                data.gi.hashGrid = HashGrid::write(graph, decayGICells.getData().hashGrid);

                data.gbuffer.readFrom(graph, lightingPass.getData().gBuffer, vk::ImageLayout::eGeneral);
                data.gbuffer.positions = graph.read(lightingPass.getData().gBuffer.positions, vk::ImageLayout::eGeneral);
                data.gbuffer.viewSpaceNormalTangents = graph.read(lightingPass.getData().gBuffer.viewSpaceNormalTangents, vk::ImageLayout::eGeneral);
            },
            [bindBaseGIUpdateInputs](const Render::CompiledPass& pass, const Context& frame, const GIUpdateData& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "fill surface cache");

                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/fill-surface-cache", (std::uint64_t)&pass);

                PushConstantRT block;
                bindBaseGIUpdateInputs(block, true, data, pass.getGraph(), frame, *pipeline, cmds);
                pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);

                constexpr i32 cacheDownscaleFactor = 2;
                const i64 resolutionX = (block.frameWidth + cacheDownscaleFactor - 1) / cacheDownscaleFactor;
                const i64 resolutionY = (block.frameHeight + cacheDownscaleFactor - 1) / cacheDownscaleFactor;

                // 8x4 recommended by AMD https://gpuopen.com/learn/rdna-performance-guide/
                const i64 groupCountX = (resolutionX + 8 - 1) / 8;
                const i64 groupCountY = (resolutionY + 4 - 1) / 4;
                cmds.dispatch(groupCountX, groupCountY, 1);
            });
        auto accumulateSurfaceCache = graph.addPass<GIUpdateData>("accumulate-surface-cache",
            [&lightingPass, &decayGICells](GraphBuilder& graph, Pass<GIUpdateData>& pass, GIUpdateData& data) {
                pass.rasterized = false;

                data.gi.hashGrid = HashGrid::write(graph, decayGICells.getData().hashGrid);

                data.gbuffer.readFrom(graph, lightingPass.getData().gBuffer, vk::ImageLayout::eGeneral);
                data.gbuffer.positions = graph.read(lightingPass.getData().gBuffer.positions, vk::ImageLayout::eGeneral);
                data.gbuffer.viewSpaceNormalTangents = graph.read(lightingPass.getData().gBuffer.viewSpaceNormalTangents, vk::ImageLayout::eGeneral);
            },
            [bindBaseGIUpdateInputs](const Render::CompiledPass& pass, const Context& frame, const GIUpdateData& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "accumulate surface cache");

                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/accumulate-radiance", (std::uint64_t)&pass);

                PushConstantRT block;
                bindBaseGIUpdateInputs(block, false, data, pass.getGraph(), frame, *pipeline, cmds);
                pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const i64 groups = (HashGridTotalCellCount + 31) / 32;
                cmds.dispatch(groups, 1, 1);
            });

        struct GIResult {
            PassData::HashGridResources hashGrid;
            PassData::GBuffer gbuffer;
            FrameResource output;
        };
        auto getGIResults = graph.addPass<GIResult>("compute-gi",
            [&accumulateSurfaceCache, &framebufferSize](GraphBuilder& graph, Pass<GIResult>& pass, GIResult& data) {
                pass.rasterized = false;

                data.gbuffer.readFrom(graph, accumulateSurfaceCache.getData().gbuffer, vk::ImageLayout::eGeneral);
                data.hashGrid = HashGrid::write(graph, accumulateSurfaceCache.getData().gi.hashGrid);

                data.output = graph.createStorageTarget("gi", vk::Format::eR32G32B32A32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
            },
            [bindBaseGIUpdateInputs](const Render::CompiledPass& pass, const Context& frame, const GIResult& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "compute GI");

                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/compute-gi", (std::uint64_t)&pass);

                PushConstantRT block;
                GIUpdateData fakeData;
                fakeData.gi.hashGrid = data.hashGrid;
                fakeData.gbuffer = data.gbuffer;
                bindBaseGIUpdateInputs(block, true, fakeData, pass.getGraph(), frame, *pipeline, cmds);

                frame.renderer.pushConstantBlock<PushConstantNoRT>("entryPointParams", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);

                auto& texture = pass.getGraph().getTexture(data.output, frame.frameNumber);
                pipeline->setStorageImage(frame, "io.output", texture, vk::ImageLayout::eGeneral);
                pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const i64 groupCountX = (block.frameWidth + 8 - 1) / 8;
                const i64 groupCountY = (block.frameHeight + 8 - 1) / 8;
                cmds.dispatch(groupCountX, groupCountY, 1);
            });

        struct PremergeData {
            PassData::GBuffer gBuffer;
            FrameResource premergedLighting;
            FrameResource directLighting;
            FrameResource gi;
            FrameResource reflections;
        };
        auto& premergeLighting = graph.addPass<PremergeData>("premerge-lighting",
            [&getGIResults, &lightingPass](GraphBuilder& graph, Pass<PremergeData>& pass, PremergeData& data) {
                pass.rasterized = false;

                data.directLighting = graph.read(lightingPass.getData().directLighting, vk::ImageLayout::eGeneral);
                data.reflections = graph.read(lightingPass.getData().reflections, vk::ImageLayout::eGeneral);
                data.gi = graph.read(getGIResults.getData().output, vk::ImageLayout::eGeneral);

                data.premergedLighting = graph.createStorageTarget("Premerged lighting", vk::Format::eR32G32B32A32Sfloat, {}, vk::ImageLayout::eGeneral);

                data.gBuffer.readFrom(graph, getGIResults.getData().gbuffer, vk::ImageLayout::eGeneral);
            }, [](const Render::CompiledPass& pass, const Context& frame, const PremergeData& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Premerge lighting");

                auto pipeline = frame.renderer.getOrCreateRenderPassSpecificPipeline("post-process/premerge-lighting", pass);

                auto& directLighting = pass.getGraph().getTexture(data.directLighting, frame.frameIndex);
                auto& reflections = pass.getGraph().getTexture(data.reflections, frame.frameIndex);
                auto& gi = pass.getGraph().getTexture(data.gi, frame.frameIndex);
                auto& output = pass.getGraph().getTexture(data.premergedLighting, frame.frameIndex);

                pipeline->setStorageImage(frame, "io.directLighting", directLighting, vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "io.reflections", reflections, vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "io.gi", gi, vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "io.output", output, vk::ImageLayout::eGeneral);
                data.gBuffer.bindInputs(*pipeline, frame, pass.getGraph(), 1, vk::ImageLayout::eGeneral);


                pipeline->bind(pass, frame, cmds, vk::PipelineBindPoint::eCompute);

                constexpr u32 blockW = 32;
                constexpr u32 blockH = 32;
                const u32 blockCountX = (output.getSize().width + blockW - 1) / blockW;
                const u32 blockCountY = (output.getSize().height + blockH - 1) / blockH;
                cmds.dispatch(blockCountX, blockCountY, 1);
            });

        struct FireflyRejection {
            FrameResource imageFullOfBugs;
            FrameResource output;
        };
        // use variance of final denoised image to reject firefly
        auto& fireflyRejection = graph.addPass<FireflyRejection>("firefly-rejection",
            [&](GraphBuilder& graph, Pass<FireflyRejection>& pass, FireflyRejection& data) {
                data.imageFullOfBugs = graph.read(premergeLighting.getData().premergedLighting, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.output = graph.createStorageTarget("lighting-with-rejected-fireflies", vk::Format::eR32G32B32A32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                pass.rasterized = false;
            },
            [](const CompiledPass& pass, const Context& frame, const FireflyRejection& data, vk::CommandBuffer& cmds) {
                auto pipeline = frame.renderer.getOrCreatePipelineFullPath("resources/pipelines/compute/firefly-rejection.pipeline", (std::uint64_t)&pass);
                auto& outputTexture = pass.getGraph().getTexture(data.output, frame.frameNumber);
                pipeline->setSampledImage(frame,"entryPointParams.input", pass.getGraph().getTexture(data.imageFullOfBugs, frame.frameNumber));
                pipeline->setStorageImage(frame,"entryPointParams.output", outputTexture, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                const auto& extent = outputTexture.getSize();
                const std::uint8_t localSize = 8;
                std::size_t dispatchX = (extent.width + (localSize-1)) / localSize;
                std::size_t dispatchY = (extent.height + (localSize-1)) / localSize;

                pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                cmds.dispatch(dispatchX, dispatchY, 1);
            });

        constexpr static i32 SVGFLocalSize = 8;

        struct SVGFTemporal {
            PassData::GBuffer gBuffer;
            FrameResource integratedColor; // right after temporal accumulation
            FrameResource colorHistory; // after first wave of spatial filter
            FrameResource momentsHistory;
            FrameResource currentFrameColor;
        };
        auto& svgfTemporal = graph.addPass<SVGFTemporal>("svgf-temporal",
            [&](GraphBuilder& graph, Pass<SVGFTemporal>& pass, SVGFTemporal& data) {
                data.gBuffer.readFrom(graph, premergeLighting.getData().gBuffer, vk::ImageLayout::eGeneral);
                data.integratedColor = graph.createStorageTarget("integrated color", vk::Format::eR32G32B32A32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                data.colorHistory = graph.createStorageTarget("color history", vk::Format::eR32G32B32A32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                data.momentsHistory = graph.createStorageTarget("moments history", vk::Format::eR32G32B32A32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                data.currentFrameColor = graph.read(fireflyRejection.getData().output, vk::ImageLayout::eGeneral);

                graph.reuseResourceAcrossFrames(data.colorHistory, 1);
                graph.reuseResourceAcrossFrames(data.momentsHistory, 1);
                graph.getVulkanDriver().getEngine().getResourceRepository().getTextureUsages(data.colorHistory.rootID) |= vk::ImageUsageFlagBits::eTransferDst;
                pass.rasterized = false;
            },
            [](const CompiledPass& pass, const Context& frame, const SVGFTemporal& data, vk::CommandBuffer& cmds) {
                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/svgf/temporal", (u64)&pass);
                auto& output = pass.getGraph().getTexture(data.integratedColor, frame.frameNumber); // colorHistory is written to by first wave of spatial denoise
                // 0 is camera
                data.gBuffer.bindLastFrameInputs(*pipeline, frame, pass.getGraph(), 1, vk::ImageLayout::eGeneral); // previous frame
                data.gBuffer.bindInputs(*pipeline, frame, pass.getGraph(), 2, vk::ImageLayout::eGeneral); // current frame

                pipeline->setStorageImage(frame, "currentFrameColor", pass.getGraph().getTexture(data.currentFrameColor, frame.frameNumber), vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "colorHistory", pass.getGraph().getTexture(data.colorHistory, frame.getPreviousFrameNumber()), vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "momentsHistory", pass.getGraph().getTexture(data.momentsHistory, frame.getPreviousFrameNumber()), vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "colorOutput", output, vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "momentsOutput", pass.getGraph().getTexture(data.momentsHistory, frame.frameNumber), vk::ImageLayout::eGeneral);

                pipeline->bind(pass, frame, cmds, vk::PipelineBindPoint::eCompute);

                const i32 groupCountX = Math::alignUp((i32)output.getSize().width, SVGFLocalSize) / SVGFLocalSize;
                const i32 groupCountY = Math::alignUp((i32)output.getSize().height, SVGFLocalSize) / SVGFLocalSize;
                cmds.dispatch(groupCountX, groupCountY, 1);
            });

        struct SVGFVariance {
            PassData::GBuffer gBuffer;
            FrameResource currentFrameColor;
            FrameResource momentsHistory;
            FrameResource varianceOutput;
        };

        auto& svgfVarianceEstimation = graph.addPass<SVGFVariance>("svgf-variance-estimation",
            [&](GraphBuilder& graph, Pass<SVGFVariance>& pass, SVGFVariance& data) {
                data.gBuffer.readFrom(graph, svgfTemporal.getData().gBuffer, vk::ImageLayout::eGeneral);
                data.varianceOutput = graph.createStorageTarget("variance output", vk::Format::eR32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                data.currentFrameColor = graph.read(svgfTemporal.getData().integratedColor, vk::ImageLayout::eGeneral);
                data.momentsHistory = graph.read(svgfTemporal.getData().momentsHistory, vk::ImageLayout::eGeneral);
                pass.rasterized = false;
            },
            [](const CompiledPass& pass, const Context& frame, const SVGFVariance& data, vk::CommandBuffer& cmds) {
                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/svgf/variance-estimation", (u64)&pass);
                auto& output = pass.getGraph().getTexture(data.varianceOutput, frame.frameNumber);

                // 0 is camera
                data.gBuffer.bindLastFrameInputs(*pipeline, frame, pass.getGraph(), 1, vk::ImageLayout::eGeneral); // previous frame
                data.gBuffer.bindInputs(*pipeline, frame, pass.getGraph(), 2, vk::ImageLayout::eGeneral); // current framess

                pipeline->setStorageImage(frame, "currentFrameColor", pass.getGraph().getTexture(data.currentFrameColor, frame.frameNumber), vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "momentsHistory", pass.getGraph().getTexture(data.momentsHistory, frame.frameNumber), vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "varianceOutput", output, vk::ImageLayout::eGeneral);

                pipeline->bind(pass, frame, cmds, vk::PipelineBindPoint::eCompute);

                const i32 groupCountX = Math::alignUp((i32)output.getSize().width, SVGFLocalSize) / SVGFLocalSize;
                const i32 groupCountY = Math::alignUp((i32)output.getSize().height, SVGFLocalSize) / SVGFLocalSize;
                cmds.dispatch(groupCountX, groupCountY, 1);
            });

        struct SVGFIteration {
            PassData::GBuffer gBuffer;
            FrameResource previousPassColor;
            FrameResource currentPassColorOutput;
            FrameResource previousPassVariance;
            FrameResource updatedVariance;
        };

        auto& svgfFirstIteration = graph.addPass<SVGFIteration>("svgf-iteration-1",
            [&](GraphBuilder& graph, Pass<SVGFIteration>& pass, SVGFIteration& data) {
                data.gBuffer.readFrom(graph, svgfVarianceEstimation.getData().gBuffer, vk::ImageLayout::eGeneral);
                data.previousPassColor = graph.read(svgfVarianceEstimation.getData().currentFrameColor, vk::ImageLayout::eGeneral);
                data.currentPassColorOutput = graph.createStorageTarget("filtered color 1", vk::Format::eR32G32B32A32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                data.previousPassVariance = graph.read(svgfVarianceEstimation.getData().varianceOutput, vk::ImageLayout::eGeneral);
                data.updatedVariance = graph.createStorageTarget("updated variance 1", vk::Format::eR32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                pass.rasterized = false;

                graph.getVulkanDriver().getEngine().getResourceRepository().getTextureUsages(data.currentPassColorOutput.rootID) |= vk::ImageUsageFlagBits::eTransferSrc;
            },
            [](const CompiledPass& pass, const Context& frame, const SVGFIteration& data, vk::CommandBuffer& cmds) {
                auto pipeline = frame.renderer.getOrCreateRenderPassSpecificPipeline("lighting/svgf/wavelet-filter", pass);
                auto& output = pass.getGraph().getTexture(data.currentPassColorOutput, frame.frameNumber);

                // 0 is camera
                data.gBuffer.bindLastFrameInputs(*pipeline, frame, pass.getGraph(), 1, vk::ImageLayout::eGeneral); // previous frame
                data.gBuffer.bindInputs(*pipeline, frame, pass.getGraph(), 2, vk::ImageLayout::eGeneral); // current frame

                pipeline->setStorageImage(frame, "currentFrameColor", pass.getGraph().getTexture(data.previousPassColor, frame.frameNumber), vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "varianceInput", pass.getGraph().getTexture(data.previousPassVariance, frame.frameNumber), vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "colorOutput", output, vk::ImageLayout::eGeneral);
                pipeline->setStorageImage(frame, "varianceOutput", pass.getGraph().getTexture(data.updatedVariance, frame.frameNumber), vk::ImageLayout::eGeneral);

                frame.renderer.pushConstants("entryPointParams", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, (u32)0);

                pipeline->bind(pass, frame, cmds, vk::PipelineBindPoint::eCompute);

                const i32 groupCountX = Math::alignUp((i32)output.getSize().width, SVGFLocalSize) / SVGFLocalSize;
                const i32 groupCountY = Math::alignUp((i32)output.getSize().height, SVGFLocalSize) / SVGFLocalSize;
                cmds.dispatch(groupCountX, groupCountY, 1);
            });

        struct SVGFCopy {
            FrameResource input;
            FrameResource output;
        };
        auto& svgfCopyFirstIterationToHistory = graph.addPass<SVGFCopy>("copy first iteration to history",
            [&](GraphBuilder& graph, Pass<SVGFCopy>& pass, SVGFCopy& data) {
                data.input = graph.read(svgfFirstIteration.getData().currentPassColorOutput, vk::ImageLayout::eGeneral);
                data.output = graph.write(svgfTemporal.getData().colorHistory, vk::AttachmentLoadOp::eDontCare, vk::ImageLayout::eGeneral);
                pass.rasterized = false;
            }, [](const CompiledPass& pass, const Context& frame, const SVGFCopy& data, vk::CommandBuffer& cmds) {
                const auto& input = pass.getGraph().getTexture(data.input, frame.frameNumber);
                const auto& output = pass.getGraph().getTexture(data.output, frame.frameNumber);

                cmds.copyImage(input.getVulkanImage(), vk::ImageLayout::eGeneral, output.getVulkanImage(), vk::ImageLayout::eGeneral, vk::ImageCopy {
                    .srcSubresource = vk::ImageSubresourceLayers {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .layerCount = 1,
                    },
                    .dstSubresource = vk::ImageSubresourceLayers {
                        .aspectMask = vk::ImageAspectFlagBits::eColor,
                        .layerCount = 1,
                    },
                    .extent = output.getSize(),
                });
            });

        auto addWaveletIteration = [&](int iterationIndex, const SVGFIteration& previousPass) -> Pass<SVGFIteration>& {
            return graph.addPass<SVGFIteration>(Carrot::sprintf("wavelet iteration %d", iterationIndex),
                [&](GraphBuilder& graph, Pass<SVGFIteration>& pass, SVGFIteration& data) {
                    data.gBuffer.readFrom(graph, previousPass.gBuffer, vk::ImageLayout::eGeneral);
                    data.previousPassColor = graph.read(previousPass.currentPassColorOutput, vk::ImageLayout::eGeneral);
                    data.currentPassColorOutput = graph.createStorageTarget("filtered color", vk::Format::eR32G32B32A32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                    data.previousPassVariance = graph.read(previousPass.updatedVariance, vk::ImageLayout::eGeneral);
                    data.updatedVariance = graph.createStorageTarget("updated variance", vk::Format::eR32Sfloat, framebufferSize, vk::ImageLayout::eGeneral);
                    pass.rasterized = false;
                },
                [iterationIndex](const CompiledPass& pass, const Context& frame, const SVGFIteration& data, vk::CommandBuffer& cmds) {
                    auto pipeline = frame.renderer.getOrCreatePipeline("lighting/svgf/wavelet-filter", (u64)&pass);
                    auto& output = pass.getGraph().getTexture(data.currentPassColorOutput, frame.frameNumber);

                    // 0 is camera
                    data.gBuffer.bindLastFrameInputs(*pipeline, frame, pass.getGraph(), 1, vk::ImageLayout::eGeneral); // previous frame
                    data.gBuffer.bindInputs(*pipeline, frame, pass.getGraph(), 2, vk::ImageLayout::eGeneral); // current frame

                    pipeline->setStorageImage(frame, "currentFrameColor", pass.getGraph().getTexture(data.previousPassColor, frame.frameNumber), vk::ImageLayout::eGeneral);
                    pipeline->setStorageImage(frame, "varianceInput", pass.getGraph().getTexture(data.previousPassVariance, frame.frameNumber), vk::ImageLayout::eGeneral);
                    pipeline->setStorageImage(frame, "colorOutput", output, vk::ImageLayout::eGeneral);
                    pipeline->setStorageImage(frame, "varianceOutput", pass.getGraph().getTexture(data.updatedVariance, frame.frameNumber), vk::ImageLayout::eGeneral);

                    frame.renderer.pushConstants("entryPointParams", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, (u32)iterationIndex);

                    pipeline->bind(pass, frame, cmds, vk::PipelineBindPoint::eCompute);

                    const i32 groupCountX = Math::alignUp((i32)output.getSize().width, SVGFLocalSize) / SVGFLocalSize;
                    const i32 groupCountY = Math::alignUp((i32)output.getSize().height, SVGFLocalSize) / SVGFLocalSize;
                    cmds.dispatch(groupCountX, groupCountY, 1);
                });
        };

        auto& iteration2 = addWaveletIteration(2, svgfFirstIteration.getData());
        auto& iteration3 = addWaveletIteration(3, iteration2.getData());
        auto& iteration4 = addWaveletIteration(4, iteration3.getData());
        auto& iteration5 = addWaveletIteration(5, iteration4.getData());

        PassData::Lighting data {
            .ambientOcclusion = lightingPass.getData().ambientOcclusionSpatial.pingPong[(lightingPass.getData().ambientOcclusionSpatial.iterationCount+1) % 2],
            .combinedLighting = iteration5.getData().currentPassColorOutput,
            .giDebug = lightingPass.getData().reflections, // whatever, just can't be empty TODO: remove
            .gBuffer = iteration5.getData().gBuffer,
        };
        return data;
    }
}