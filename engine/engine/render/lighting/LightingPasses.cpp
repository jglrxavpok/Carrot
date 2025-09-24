//
// Created by jglrxavpok on 09/12/2024.
//

#include "LightingPasses.h"

#include <engine/render/TextureRepository.h>

#include "engine/render/VulkanRenderer.h"
#include "engine/render/RenderGraph.h"
#include "engine/render/RenderContext.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/utils/Profiling.h"
#include "engine/Engine.h"

namespace Carrot::Render {

    // keep in sync with hash-grid.include.glsl
    static constexpr std::uint64_t HashGridCellsPerBucket = 8;
    static constexpr std::uint64_t HashGridBucketCount = 1024*256;
    static constexpr std::uint64_t HashGridTotalCellCount = HashGridBucketCount*HashGridCellsPerBucket;

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
            glm::vec3 cameraPos;
            float rayLength;
        };

        struct Constants {
            std::uint32_t cellsPerBucket;
            std::uint32_t bucketCount;
        };

        struct Pointers {
            vk::DeviceAddress grids[2]; // 0: previous frame, 1: current frame
        };

        // offset into hash grid buffer where hash cells start
        static constexpr std::uint32_t DataOffset = 0;

        // from hash-grid.include.glsl
        static constexpr std::uint32_t SizeOfHashCell =
            sizeof(HashCellKey)
            + sizeof(std::uint32_t)
            + sizeof(glm::ivec3)
            + sizeof(std::uint32_t)
        ;
        static constexpr std::uint32_t LastTouchedFrameOffset = DataOffset + SizeOfHashCell * HashGridTotalCellCount;

        static Carrot::Render::PassData::HashGridResources createResources(Carrot::Render::GraphBuilder& graph) {
            Carrot::Render::PassData::HashGridResources r;

            const std::size_t hashGridSize = computeSizeOf(HashGridBucketCount, HashGridCellsPerBucket);
            r.hashGrid = graph.createBuffer("GI probes hashmap", hashGridSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc, false/*we want to keep the header*/);
            r.constants = graph.createBuffer("GI probes constants", sizeof(Constants), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, false/*filled once*/);
            r.gridPointers = graph.createBuffer("GI probes grid pointers", sizeof(Pointers), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, false/*filled once*/);

            graph.reuseResourceAcrossFrames(r.hashGrid, 1);
            graph.reuseResourceAcrossFrames(r.gridPointers, 1);
            return r;
        }

        static void prepareBuffers(const Carrot::Render::Graph& graph, const Carrot::Render::PassData::HashGridResources& r) {
            // only a single one for both frame
            {
                Constants constants{};
                constants.bucketCount = HashGridBucketCount;
                constants.cellsPerBucket = HashGridCellsPerBucket;
                graph.getBuffer(r.constants, 0).view.uploadForFrame(&constants, sizeof(constants));
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
            renderContext.renderer.bindBuffer(pipeline, renderContext, graph.getBuffer(r.constants, renderContext.frameNumber).view, setID, 0);
            renderContext.renderer.bindBuffer(pipeline, renderContext, graph.getBuffer(r.gridPointers, renderContext.frameNumber).view, setID, 1);
        }

        static Carrot::Render::PassData::HashGridResources write(Carrot::Render::GraphBuilder& graph, const Carrot::Render::PassData::HashGridResources& r) {
            Carrot::Render::PassData::HashGridResources out;
            out.constants = graph.write(r.constants, {}, {}, {}, {});
            out.gridPointers = graph.write(r.gridPointers, {}, {}, {}, {});
            out.hashGrid = graph.write(r.hashGrid, {}, {}, {}, {});
            return out;
        }

        static std::size_t computeSizeOf(std::size_t bucketCount, std::size_t cellsPerBucket) {
            const std::size_t totalCellCount = bucketCount * cellsPerBucket;
            return totalCellCount * SizeOfHashCell + totalCellCount * sizeof(std::uint32_t);
        }
    };

    PassData::Lighting addLightingPasses(const Carrot::Render::PassData::GBuffer& opaqueData,
                                                                                               Carrot::Render::GraphBuilder& graph,
                                                                                               const Render::TextureSize& framebufferSize) {
        using namespace Carrot::Render;
        vk::ClearValue clearColor = vk::ClearColorValue(std::array{0.0f,0.0f,0.0f,0.0f});


        struct GIData {
            PassData::HashGridResources hashGrid;
        };

        auto& reuseWorldSpaceGICells = graph.addPass<GIData>("reuse-gi",
            [&](GraphBuilder& graph, Pass<GIData>& pass, GIData& data) {
                pass.rasterized = false;

                data.hashGrid = HashGrid::createResources(graph);
            },
            [](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Reuse GI cells");
                if (frame.frameNumber == 0) {
                    return; // nothing to do
                }

                // copy last frame's data
                auto& buffer = pass.getGraph().getBuffer(data.hashGrid.hashGrid, frame.frameNumber);
                auto& lastFrameBuffer = pass.getGraph().getBuffer(data.hashGrid.hashGrid, frame.frameNumber-1);
                vk::BufferCopy region {
                    .srcOffset = lastFrameBuffer.view.getStart() + HashGrid::DataOffset,
                    .dstOffset = buffer.view.getStart() + HashGrid::DataOffset,
                    .size = HashGrid::SizeOfHashCell * HashGridTotalCellCount
                };

                // FIXME wtf
//                WaitDeviceIdle();
                cmds.copyBuffer(lastFrameBuffer.view.getVulkanBuffer(), buffer.view.getVulkanBuffer(), {region});

                cmds.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eAllCommands, static_cast<vk::DependencyFlags>(0),
                    {}, {
                        vk::BufferMemoryBarrier {
                            .srcAccessMask = vk::AccessFlagBits::eTransferWrite,
                            .dstAccessMask = vk::AccessFlagBits::eMemoryRead,
                            .buffer = buffer.view.getVulkanBuffer(),
                            .offset = buffer.view.getStart() + HashGrid::DataOffset,
                            .size = HashGrid::SizeOfHashCell * HashGridTotalCellCount,
                        }
                    }, {});
            });

        reuseWorldSpaceGICells.setSwapchainRecreation([](const CompiledPass& pass, const GIData& data) {
            HashGrid::prepareBuffers(pass.getGraph(), data.hashGrid);
        });

        auto addDenoisingResources = [&](Render::GraphBuilder& graph, const char* name, vk::Format format, PassData::Denoising& data) {
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
             graph.getVulkanDriver().getResourceRepository().getTextureUsages(data.historyLength.rootID) |= vk::ImageUsageFlagBits::eTransferDst;
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
             graph.reuseResourceAcrossFrames(data.historyLength, 1);
             data.iterationCount = 3;
         };
        auto applyDenoising = [framebufferSize](
            const CompiledPass& pass,
            const Carrot::Render::Context& frame,
            const char* denoisePipelineName,
            const PassData::GBuffer& gBuffer,

            // may differ from gbuffer to account for bounces
            const FrameResource& positionsTex,
            const FrameResource& normalsTangentsTex,

            const PassData::Denoising& data,
            std::uint8_t index,

            bool isAO, // has different neighbor clamping
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
                if(framebufferSize.type == Render::TextureSize::Type::SwapchainProportional) {
                    block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                    block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
                } else {
                    block.frameWidth = framebufferSize.width;
                    block.frameHeight = framebufferSize.height;
                }

               auto temporalDenoisePipeline = renderer.getOrCreatePipeline("lighting/temporal-denoise", (std::uint64_t)&pass + index);
               auto spatialDenoisePipelines = std::array {
                   renderer.getOrCreatePipeline(denoisePipelineName, (std::uint64_t)&pass + 0 + index*3),
                   renderer.getOrCreatePipeline(denoisePipelineName, (std::uint64_t)&pass + 1 + index*3),
                   renderer.getOrCreatePipeline(denoisePipelineName, (std::uint64_t)&pass + 2 + index*3),
               };

               std::array pImages = std::array {
                   &pass.getGraph().getTexture(data.pingPong[0], frame.frameNumber),
                   &pass.getGraph().getTexture(data.pingPong[1], frame.frameNumber)
               };

               gBuffer.bindInputs(*temporalDenoisePipeline, frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);
                renderer.bindTexture(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(positionsTex, frame.frameNumber), 0, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, positionsTex.layout);
                renderer.bindTexture(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(normalsTangentsTex, frame.frameNumber), 0, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, normalsTangentsTex.layout);

               renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.noisy, frame.frameNumber), 2, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
               renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.samples, frame.frameNumber), 2, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
               renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.historyLength, frame.frameNumber), 2, 3, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                renderer.bindTexture(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(positionsTex, frame.getPreviousFrameNumber()), 2, 5, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, positionsTex.layout);
                renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.historyLength, frame.getPreviousFrameNumber()), 2, 4, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

            // TODO: should use result of temporal supersampling or spatial denoise?
                renderer.bindStorageImage(*temporalDenoisePipeline, frame, pass.getGraph().getTexture(data.pingPong[(data.iterationCount+1)%2], frame.getPreviousFrameNumber()), 2, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

               gBuffer.bindInputs(*spatialDenoisePipelines[0], frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);
               gBuffer.bindInputs(*spatialDenoisePipelines[1], frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);
               gBuffer.bindInputs(*spatialDenoisePipelines[2], frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);

                for (auto& pSpatialDenoisePipeline : spatialDenoisePipelines) {
                    renderer.bindTexture(*pSpatialDenoisePipeline, frame, pass.getGraph().getTexture(positionsTex, frame.frameNumber), 0, 1, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, positionsTex.layout);
                    renderer.bindTexture(*pSpatialDenoisePipeline, frame, pass.getGraph().getTexture(normalsTangentsTex, frame.frameNumber), 0, 2, nullptr, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, normalsTangentsTex.layout);
                }

               renderer.bindStorageImage(*spatialDenoisePipelines[0], frame, pass.getGraph().getTexture(data.samples, frame.frameNumber), 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
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

                   GPUZoneColored(GetEngine().tracyCtx[frame.frameIndex], cmds, "Denoising passes", glm::vec4(0,0,1,1));

                   struct Block {
                       std::uint32_t iterationIndex;
                   } denoiseBlock;

                   // first iteration
                   {
                       renderer.pushConstants("push", *temporalDenoisePipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, isAO ? (u32)1 : (u32)0);
                       temporalDenoisePipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                       cmds.dispatch(dispatchX, dispatchY, 1);
                   }
                   {
                       denoiseBlock.iterationIndex = 0;
                       renderer.pushConstantBlock("push", *spatialDenoisePipelines[0], frame, vk::ShaderStageFlagBits::eCompute, cmds, denoiseBlock);
                       spatialDenoisePipelines[0]->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                       cmds.dispatch(dispatchX, dispatchY, 1);
                   }
                   for(std::uint8_t i = 0; i < data.iterationCount-1; i++) {
                       auto& pipeline = *spatialDenoisePipelines[i % 2 +1];
                       denoiseBlock.iterationIndex = i;
                       renderer.pushConstantBlock("push", pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, denoiseBlock);
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


        auto& decayGICells = graph.addPass<GIData>("decay-gi",
            [&](GraphBuilder& graph, Pass<GIData>& pass, GIData& data) {
                pass.rasterized = false;
                data.hashGrid = HashGrid::write(graph, reuseWorldSpaceGICells.getData().hashGrid);
            },
            [](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Decay GI cells");

                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/decay-cells", (std::uint64_t)&pass);

                const BufferAllocation& hashGridBuffer = pass.getGraph().getBuffer(data.hashGrid.hashGrid, frame.frameNumber);
                BufferView pCells = hashGridBuffer.view.subView(HashGrid::DataOffset, HashGrid::SizeOfHashCell * HashGridTotalCellCount);
                BufferView pLastTouchedFrame = hashGridBuffer.view.subView(HashGrid::LastTouchedFrameOffset);
                frame.renderer.pushConstants("push", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds,
                    (std::uint32_t)HashGridTotalCellCount, (std::uint32_t)frame.frameNumber, pLastTouchedFrame.getDeviceAddress(), pCells.getDeviceAddress());

                HashGrid::bind(data.hashGrid, pass.getGraph(), frame, *pipeline, 0);
                pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const std::size_t localSize = 256;
                const std::size_t groupX = (HashGridTotalCellCount + localSize-1) / localSize;
                cmds.dispatch(groupX, 1, 1);
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
            if(framebufferSize.type == Render::TextureSize::Type::SwapchainProportional) {
                block.frameWidth = framebufferSize.width * frame.pViewport->getWidth();
                block.frameHeight = framebufferSize.height * frame.pViewport->getHeight();
            } else {
                block.frameWidth = framebufferSize.width;
                block.frameHeight = framebufferSize.height;
            }
        };

        struct GIUpdateData {
            GIData gi;
            FrameResource screenProbes;
            FrameResource spawnedProbes;
            FrameResource emptyProbes;
            FrameResource reprojectedProbes;
            FrameResource spawnedRays;
            PassData::GBuffer gbuffer;

            void writeProbeData(Render::GraphBuilder& graph, const GIUpdateData& other) {
                screenProbes = graph.write(other.screenProbes, {}, {});
                spawnedProbes = graph.write(other.spawnedProbes, {}, {});
                emptyProbes = graph.write(other.emptyProbes, {}, {});
                reprojectedProbes = graph.write(other.reprojectedProbes, {}, {});
                spawnedRays = graph.write(other.spawnedRays, {}, {});
            }
        };

        static constexpr i32 ProbeScreenSize = 8; // how many pixels a screen probe covers in one direction
        struct ScreenProbe {
            glm::ivec3 radiance;
            glm::vec3 worldPos;
            glm::vec3 normal;
            glm::ivec2 bestPixel;
            u32 sampleCount;
        };
        struct SpawnedRay {
            std::uint32_t probeIndex;
        };

        auto bindBaseGIUpdateInputs = [preparePushConstant](PushConstantRT& block, const GIUpdateData& data, const Render::Graph& graph, const Render::Context& frame, Carrot::Pipeline& pipeline, vk::CommandBuffer& cmds) {
            auto& renderer = frame.renderer;
            preparePushConstant(block, frame);

            bool useRaytracingVersion = GetCapabilities().supportsRaytracing;
            Carrot::AccelerationStructure* pTLAS = nullptr;
            if (pipeline.getPushConstant("push").offset != (u32)-1) {
                if(useRaytracingVersion) {
                    pTLAS = frame.renderer.getASBuilder().getTopLevelAS(frame);
                    block.hasTLAS = pTLAS != nullptr;
                    renderer.pushConstantBlock<PushConstantRT>("push", pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
                } else {
                    renderer.pushConstantBlock<PushConstantNoRT>("push", pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
                }
            }
            bool needSceneInfo = true;
            if(useRaytracingVersion) {
               renderer.bindTexture(pipeline, frame, *renderer.getMaterialSystem().getBlueNoiseTextures()[frame.frameIndex % Render::BlueNoiseTextureCount]->texture, 6, 1, nullptr);
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

            Render::Texture::Ref skyboxCubeMap = GetEngine().getSkyboxCubeMap();
            if(!skyboxCubeMap || GetEngine().getSkybox() == Carrot::Skybox::Type::None) {
                skyboxCubeMap = renderer.getBlackCubeMapTexture();
            }

            HashGrid::bind(data.gi.hashGrid, graph, frame, pipeline, 0);
            data.gbuffer.bindInputs(pipeline, frame, graph, 4, vk::ImageLayout::eGeneral);
        };

        static constexpr i32 MaxRaysPerProbe = 20;

        auto bindGIRayBuffers = [](Carrot::Pipeline& pipeline, const GIUpdateData& data, const Render::Graph& graph, const Render::Context& context) {
            context.renderer.bindBuffer(pipeline, context, graph.getBuffer(data.screenProbes, context.frameNumber).view, 1, 0);
            context.renderer.bindBuffer(pipeline, context, graph.getBuffer(data.screenProbes, context.frameNumber-1).view, 1, 1);
            context.renderer.bindBuffer(pipeline, context, graph.getBuffer(data.spawnedProbes, context.frameNumber).view, 1, 2);
            context.renderer.bindBuffer(pipeline, context, graph.getBuffer(data.emptyProbes, context.frameNumber).view, 1, 3);
            context.renderer.bindBuffer(pipeline, context, graph.getBuffer(data.reprojectedProbes, context.frameNumber).view, 1, 4);
            context.renderer.bindBuffer(pipeline, context, graph.getBuffer(data.spawnedRays, context.frameNumber).view, 1, 5);
        };

        auto spawnScreenProbes = graph.addPass<GIUpdateData>("spawn-screen-probes",
            [&](GraphBuilder& graph, Pass<GIUpdateData>& pass, GIUpdateData& data) {
                pass.rasterized = false;

                auto getProbeCount = [](const glm::ivec2& viewportSize) {
                    const i32 probesCountX = (viewportSize.x+ProbeScreenSize-1) / ProbeScreenSize;
                    const i32 probesCountY = (viewportSize.y+ProbeScreenSize-1) / ProbeScreenSize;
                    const i32 probeCount = probesCountX * probesCountY;
                    return probeCount;
                };

                data.gi.hashGrid = HashGrid::write(graph, decayGICells.getData().hashGrid);
                data.screenProbes = graph.createBuffer("screen-probes", [=](const glm::ivec2& v) { return getProbeCount(v) * sizeof(ScreenProbe); }, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, true);
                data.spawnedProbes = graph.createBuffer("spawned-probes", [=](const glm::ivec2& v) { return getProbeCount(v) * sizeof(ScreenProbe) + sizeof(u32); }, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, true);
                ComputedBufferSize probeListSize = [=](const glm::ivec2& v) { return getProbeCount(v) * sizeof(u32) + sizeof(u32); };
                data.emptyProbes = graph.createBuffer("empty-probes", probeListSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, true);
                data.reprojectedProbes = graph.createBuffer("reprojected-probes", probeListSize, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, true);
                data.spawnedRays = graph.createBuffer("spawned-rays", [=](const glm::ivec2& v) { return sizeof(u32) + getProbeCount(v) * MaxRaysPerProbe * sizeof(SpawnedRay); }, vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, true);
                graph.reuseResourceAcrossFrames(data.screenProbes, 1); // need previous frame

                data.gbuffer.readFrom(graph, opaqueData, vk::ImageLayout::eGeneral);
            },
            [preparePushConstant, bindBaseGIUpdateInputs, bindGIRayBuffers](const Render::CompiledPass& pass, const Render::Context& frame, const GIUpdateData& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Spawn GI screen probes");

                if (frame.frameNumber == 0) {
                    return;
                }

                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/spawn-screen-probes", (i64)&pass);

                PushConstantRT block;
                bindBaseGIUpdateInputs(block, data, pass.getGraph(), frame, *pipeline, cmds);
                bindGIRayBuffers(*pipeline, data, pass.getGraph(), frame);
                pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const std::size_t groupX = (block.frameWidth+ProbeScreenSize-1)/ProbeScreenSize;
                const std::size_t groupY = (block.frameHeight+ProbeScreenSize-1)/ProbeScreenSize;
                cmds.dispatch(groupX, groupY, 1);
            });

        auto addDispatchPass = [&](const Render::Pass<GIUpdateData>& previous, const std::string& pipelineName, i64 countMultiplier) {
            return graph.addPass<GIUpdateData>(Carrot::sprintf("gi-%s", pipelineName.c_str()),
            [&](GraphBuilder& graph, Pass<GIUpdateData>& pass, GIUpdateData& data) {
                pass.rasterized = false;
                data.writeProbeData(graph, previous.getData());
                data.gbuffer.readFrom(graph, previous.getData().gbuffer, vk::ImageLayout::eGeneral);
                data.gi.hashGrid = HashGrid::write(graph, previous.getData().gi.hashGrid);
            },
            [countMultiplier, bindBaseGIUpdateInputs, bindGIRayBuffers, framebufferSize, preparePushConstant, pipelineFullName = Carrot::sprintf("lighting/gi/%s", pipelineName.c_str())]
            (const Render::CompiledPass& pass, const Render::Context& frame, const GIUpdateData& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "GI pass");

                auto pipeline = frame.renderer.getOrCreatePipeline(pipelineFullName, (std::uint64_t)&pass);

                PushConstantRT block;
                bindBaseGIUpdateInputs(block, data, pass.getGraph(), frame, *pipeline, cmds);
                bindGIRayBuffers(*pipeline, data, pass.getGraph(), frame);
                pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const i64 probesCountX = (block.frameWidth+ProbeScreenSize-1) / ProbeScreenSize;
                const i64 probesCountY = (block.frameHeight+ProbeScreenSize-1) / ProbeScreenSize;
                const i64 spawnedRayCount = countMultiplier * probesCountX * probesCountY;
                const i64 groups = (spawnedRayCount + 31) / 32;
                cmds.dispatch(groups, 1, 1);
            });
        };

        auto reorderSpawnedRays = addDispatchPass(spawnScreenProbes, "reorder-rays", 1);
        auto spawnGIRays = addDispatchPass(reorderSpawnedRays, "spawn-rays", 1);
        auto traceGIRays = addDispatchPass(spawnGIRays, "trace-rays", MaxRaysPerProbe);

        auto& lightingPass = graph.addPass<Carrot::Render::PassData::LightingResources>("lighting",
                                                                 [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::LightingResources>& pass, Carrot::Render::PassData::LightingResources& resolveData)
               {
                    pass.rasterized = false;
                    resolveData.gBuffer.readFrom(graph, traceGIRays.getData().gbuffer, vk::ImageLayout::eShaderReadOnlyOptimal);

                    addDenoisingResources(graph, "Ambient Occlusion", vk::Format::eR8Unorm, resolveData.ambientOcclusion);
                    addDenoisingResources(graph, "Direct Lighting", vk::Format::eR32G32B32A32Sfloat, resolveData.directLighting);
                    addDenoisingResources(graph, "Reflections", vk::Format::eR32G32B32A32Sfloat, resolveData.reflections);
                    resolveData.reflectionsFirstBounceViewPositions = graph.createStorageTarget(
                        "Reflections first bounce positions",
                        vk::Format::eR32G32B32A32Sfloat,
                        framebufferSize,
                        vk::ImageLayout::eGeneral
                        );
                    resolveData.reflectionsFirstBounceViewNormalsTangents = graph.createStorageTarget(
                        "Reflections first bounce normals",
                        vk::Format::eR32G32B32A32Sfloat,
                        framebufferSize,
                        vk::ImageLayout::eGeneral);

                    resolveData.hashGrid = HashGrid::write(graph, decayGICells.getData().hashGrid);
               },
               [framebufferSize, applyDenoising](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::LightingResources& data, vk::CommandBuffer& cmds) {
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
                       auto& outputImage = renderGraph.getTexture(output, frame.frameNumber);
                       renderer.bindStorageImage(pipeline, frame, outputImage, 5, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

                       if(useRaytracingVersion) {
                           renderer.bindTexture(pipeline, frame, *renderer.getMaterialSystem().getBlueNoiseTextures()[frame.frameIndex % Render::BlueNoiseTextureCount]->texture, 6, 1, nullptr);
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
                       GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "RT passes");
                       const std::uint8_t localSizeX = 32;
                       const std::uint8_t localSizeY = 32;
                       std::size_t dispatchX = (block.frameWidth + (localSizeX-1)) / localSizeX;
                       std::size_t dispatchY = (block.frameHeight + (localSizeY-1)) / localSizeY;
                       setupPipeline(data.directLighting.noisy, *directLightingPipeline, false, useRaytracingVersion ? 7 : 6);
                       directLightingPipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                       cmds.dispatch(dispatchX, dispatchY, 1);

                       setupPipeline(data.ambientOcclusion.noisy, *aoPipeline, false, -1);
                       aoPipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);
                       cmds.dispatch(dispatchX, dispatchY, 1);

                       setupPipeline(data.reflections.noisy, *reflectionsPipeline, true, useRaytracingVersion ? 7 : 6);
                       // setup first bounce textures
                       {
                            auto& outputPos = pass.getGraph().getTexture(data.reflectionsFirstBounceViewPositions, frame.frameIndex);
                            auto& outputNormalsTangents = pass.getGraph().getTexture(data.reflectionsFirstBounceViewNormalsTangents, frame.frameIndex);
                            renderer.bindStorageImage(*reflectionsPipeline, frame, outputPos, 5, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                            renderer.bindStorageImage(*reflectionsPipeline, frame, outputNormalsTangents, 5, 2, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                       }
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

                   applyDenoising(pass, frame, "lighting/denoise-ao", data.gBuffer, data.gBuffer.positions, data.gBuffer.viewSpaceNormalTangents, data.ambientOcclusion, 0, true, cmds);
                   applyDenoising(pass, frame, "lighting/denoise-direct", data.gBuffer, data.gBuffer.positions, data.gBuffer.viewSpaceNormalTangents, data.directLighting, 1, false, cmds);
                   applyDenoising(pass, frame, "lighting/denoise-direct", data.gBuffer, data.reflectionsFirstBounceViewPositions, data.reflectionsFirstBounceViewNormalsTangents, data.reflections, 2, false, cmds);
               },
               [&](const CompiledPass& pass, const PassData::LightingResources& data) {
               }
        );

        lightingPass.setSwapchainRecreation([&](const CompiledPass& pass, const PassData::LightingResources& data) {
            GetVulkanDriver().performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
                for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
                    auto clear = [&](const PassData::Denoising& data) {
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
                    clear(data.ambientOcclusion);
                    clear(data.directLighting);
                }
            });

        });

        // TODO: disableable
        struct GIDebug {
            PassData::GBuffer gbuffer;
            GIData gi;
            FrameResource output;

            FrameResource screenProbes;
            FrameResource spawnedProbes;
            FrameResource emptyProbes;
            FrameResource reprojectedProbes;
        };
        auto& debugGICells = graph.addPass<GIDebug>("debug-gi",
            [&](GraphBuilder& graph, Pass<GIDebug>& pass, GIDebug& data) {
                pass.rasterized = false;
                data.gbuffer.readFrom(graph, lightingPass.getData().gBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.gi.hashGrid = HashGrid::write(graph, traceGIRays.getData().gi.hashGrid);
                data.output = graph.createStorageTarget("gi-debug", vk::Format::eR8G8B8A8Unorm, framebufferSize, vk::ImageLayout::eGeneral);
                data.screenProbes = graph.write(traceGIRays.getData().screenProbes, {}, {});
                data.spawnedProbes = graph.write(traceGIRays.getData().spawnedProbes, {}, {});
                data.emptyProbes = graph.write(traceGIRays.getData().emptyProbes, {}, {});
                data.reprojectedProbes = graph.write(traceGIRays.getData().reprojectedProbes, {}, {});
            },
            [preparePushConstant](const Render::CompiledPass& pass, const Render::Context& frame, const GIDebug& data, vk::CommandBuffer& cmds) {
                GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Debug GI cells");

                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/debug-gi", (std::uint64_t)&pass);
                auto& outputTexture = pass.getGraph().getTexture(data.output, frame.frameIndex);

                PushConstantRT block;
                preparePushConstant(block, frame);
                frame.renderer.pushConstantBlock<PushConstantNoRT>("entryPointParams", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);

                frame.renderer.bindStorageImage(*pipeline, frame, outputTexture, 0, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                frame.renderer.bindBuffer(*pipeline, frame, pass.getGraph().getBuffer(data.screenProbes, frame.frameNumber).view, 0, 1);
                pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const std::size_t localSize = 32;
                const std::size_t groupX = (outputTexture.getSize().width + localSize-1) / localSize;
                const std::size_t groupY = (outputTexture.getSize().height + localSize-1) / localSize;
                cmds.dispatch(groupX, groupY, 1);
            });

        struct GIFinal {
            PassData::GBuffer gbuffer;
            FrameResource screenProbesInput;
            PassData::Denoising output;
        };
        auto& getGIResults = graph.addPass<GIFinal>("gi",
            [&](GraphBuilder& graph, Pass<GIFinal>& pass, GIFinal& data) {
                pass.rasterized = false;
                data.gbuffer.readFrom(graph, lightingPass.getData().gBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.screenProbesInput = graph.read(traceGIRays.getData().screenProbes, {});
                addDenoisingResources(graph, "gi", vk::Format::eR8G8B8A8Unorm, data.output);
                data.output.iterationCount = 6;
            },
            [preparePushConstant, applyDenoising](const Render::CompiledPass& pass, const Render::Context& frame, const GIFinal& data, vk::CommandBuffer& cmds) {
                {
                    GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Final GI");

                    auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/apply-gi", (std::uint64_t)&pass);
                    auto& outputTexture = pass.getGraph().getTexture(data.output.noisy, frame.frameIndex);

                    PushConstantRT block;
                    preparePushConstant(block, frame);
                    frame.renderer.pushConstantBlock<PushConstantNoRT>("entryPointParams", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds, block);
                    frame.renderer.bindStorageImage(*pipeline, frame, outputTexture, 0, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    frame.renderer.bindBuffer(*pipeline, frame, pass.getGraph().getBuffer(data.screenProbesInput, frame.frameNumber).view, 0, 1);
                    pipeline->bind(RenderingPipelineCreateInfo{}, frame, cmds, vk::PipelineBindPoint::eCompute);

                    const std::size_t localSize = 32;
                    const std::size_t groupX = (outputTexture.getSize().width + localSize-1) / localSize;
                    const std::size_t groupY = (outputTexture.getSize().height + localSize-1) / localSize;
                    cmds.dispatch(groupX, groupY, 1);
                }

                {
                    GPUZone(GetEngine().tracyCtx[frame.frameIndex], cmds, "Denoise GI");
                    applyDenoising(pass, frame, "lighting/denoise-direct", data.gbuffer, data.gbuffer.positions, data.gbuffer.viewSpaceNormalTangents, data.output, 2, false, cmds);
                }
            });

        PassData::Lighting data {
            .ambientOcclusion = lightingPass.getData().ambientOcclusion.pingPong[(lightingPass.getData().ambientOcclusion.iterationCount+1) % 2],
            .direct = lightingPass.getData().directLighting.pingPong[(lightingPass.getData().directLighting.iterationCount+1) % 2],
            .gi = getGIResults.getData().output.pingPong[(getGIResults.getData().output.iterationCount+1) % 2],
            .reflections = lightingPass.getData().reflections.pingPong[(lightingPass.getData().reflections.iterationCount+1) % 2],
            .gBuffer = lightingPass.getData().gBuffer,
        };
        return data;
    }
}