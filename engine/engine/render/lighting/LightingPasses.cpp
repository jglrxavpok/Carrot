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

    static constexpr std::uint64_t HashGridCellsPerBucket = 8;
    static constexpr std::uint64_t HashGridBucketCount = 1024*256*4;
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

        struct HashCell {
            HashCellKey key;
            std::uint32_t hash2;
            glm::ivec3 value;
            std::uint32_t sampleCount;
        };

        struct Header {
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
            r.constants = graph.createBuffer("GI probes constants", sizeof(Constants), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, false/*filled once*/);
            r.gridPointers = graph.createBuffer("GI probes grid pointers", sizeof(Pointers), vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst, false/*filled once*/);

            graph.reuseBufferAcrossFrames(r.hashGrid, 1);
            graph.reuseBufferAcrossFrames(r.gridPointers, 1);
            return r;
        }

        static void prepareBuffers(const Carrot::Render::Graph& graph, const Carrot::Render::PassData::HashGridResources& r) {
            HashGrid::Header header{};

            // only a single one for both frame
            {
                Constants constants{};
                constants.bucketCount = HashGridBucketCount;
                constants.cellsPerBucket = HashGridCellsPerBucket;
                graph.getBuffer(r.constants, 0).view.uploadForFrame(&constants, sizeof(constants));
            }

            for(int i = 0; i < 2/* history length: current frame and previous frame */; i++) {
                Carrot::BufferView gridBuffer = graph.getBuffer(r.hashGrid, i).view;

                std::size_t cursor = sizeof(Header);
                header.pCells = gridBuffer.subView(cursor, sizeof(HashGrid::HashCell) * HashGridTotalCellCount).getDeviceAddress();

                cursor += sizeof(HashGrid::HashCell) * HashGridTotalCellCount;
                header.pLastTouchedFrame = gridBuffer.subView(cursor, sizeof(std::uint32_t) * HashGridTotalCellCount).getDeviceAddress();
                cursor += sizeof(std::uint32_t) * HashGridTotalCellCount;

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

    PassData::Lighting addLightingPasses(const Carrot::Render::PassData::GBuffer& opaqueData,
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
            [](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Clear GI cells");

                // TODO: replace with call to vkCmdFillBuffer?
                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/clear-grid", (std::uint64_t)&pass);

                frame.renderer.pushConstants("", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds,
                    static_cast<std::uint32_t>(HashGridTotalCellCount));

                HashGrid::bind(data.hashGrid, pass.getGraph(), frame, *pipeline, 0);
                pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const std::size_t localSize = 256;
                const std::size_t groupX = (HashGridTotalCellCount + localSize-1) / localSize;
                cmds.dispatch(groupX, 1, 1);
            });

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
             graph.getVulkanDriver().getResourceRepository().getTextureUsages(data.historyLength.rootID) |= vk::ImageUsageFlagBits::eTransferDst;
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
        auto applyDenoising = [framebufferSize](
            const CompiledPass& pass,
            const Carrot::Render::Context& frame,
            const char* denoisePipelineName,
            const PassData::GBuffer& gBuffer,
            const PassData::Denoising& data,
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
               gBuffer.bindInputs(*spatialDenoisePipelines[2], frame, pass.getGraph(), 0, vk::ImageLayout::eShaderReadOnlyOptimal);

               renderer.bindStorageImage(*spatialDenoisePipelines[0], frame, pass.getGraph().getTexture(data.samples, frame.swapchainIndex), 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
               renderer.bindStorageImage(*spatialDenoisePipelines[0], frame, *pImages[0], 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

               renderer.bindStorageImage(*spatialDenoisePipelines[1], frame, *pImages[1], 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
               renderer.bindStorageImage(*spatialDenoisePipelines[1], frame, *pImages[0], 1, 1, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);

            // TODO: find out why spatialDenoisePipelines[2] yields a black screen (which should be the correct code?)
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

        auto& decayGICells = graph.addPass<GIData>("decay-gi",
            [&](GraphBuilder& graph, Pass<GIData>& pass, GIData& data) {
                pass.rasterized = false;
                data.hashGrid = HashGrid::write(graph, clearGICells.getData().hashGrid);
            },
            [](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Decay GI cells");

                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/decay-cells", (std::uint64_t)&pass);

                frame.renderer.pushConstants("push", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds,
                    (std::uint32_t)HashGridTotalCellCount, (std::uint32_t)frame.frameCount);

                HashGrid::bind(data.hashGrid, pass.getGraph(), frame, *pipeline, 0);
                pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const std::size_t localSize = 256;
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
            PassData::GBuffer gbuffer;
        };
        auto updateGICells = graph.addPass<GIUpdateData>(Carrot::sprintf("update-gi"),
            [&](GraphBuilder& graph, Pass<GIUpdateData>& pass, GIUpdateData& data) {
                pass.rasterized = false;
                data.gbuffer.readFrom(graph, opaqueData, vk::ImageLayout::eGeneral);
                data.gi.hashGrid = HashGrid::write(graph, decayGICells.getData().hashGrid);
            },
            [framebufferSize, preparePushConstant](const Render::CompiledPass& pass, const Render::Context& frame, const GIUpdateData& data, vk::CommandBuffer& cmds) {
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Update GI cells");

                auto& renderer = frame.renderer;
                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/update-cells", (std::uint64_t)&pass);

                PushConstantRT block;
                preparePushConstant(block, frame);

                bool useRaytracingVersion = GetCapabilities().supportsRaytracing;
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

                HashGrid::bind(data.gi.hashGrid, pass.getGraph(), frame, *pipeline, 0);
                data.gbuffer.bindInputs(*pipeline, frame, pass.getGraph(), 4, vk::ImageLayout::eGeneral);
                pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const std::size_t localSize = 32;
                const std::size_t groupX = (block.frameWidth + localSize-1) / localSize;
                const std::size_t groupY = (block.frameHeight + localSize-1) / localSize;
                cmds.dispatch(groupX, groupY, 1);
            });

        auto& reuseGICells = graph.addPass<GIData>("reuse-gi",
            [&](GraphBuilder& graph, Pass<GIData>& pass, GIData& data) {
                pass.rasterized = false;
                data.hashGrid = HashGrid::write(graph, updateGICells.getData().gi.hashGrid);
            },
            [](const Render::CompiledPass& pass, const Render::Context& frame, const GIData& data, vk::CommandBuffer& cmds) {
                TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Reuse GI cells");

                auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/reuse-gi", (std::uint64_t)&pass);

                frame.renderer.pushConstants("push", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds,
                    (std::uint32_t)HashGridTotalCellCount, (std::uint32_t)frame.frameCount);

                HashGrid::bind(data.hashGrid, pass.getGraph(), frame, *pipeline, 0);
                pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

                const std::size_t localSize = 256;
                const std::size_t groupX = (HashGridTotalCellCount + localSize-1) / localSize;
                cmds.dispatch(groupX, 1, 1);
            });

        // TODO: move after GI to benefit from GI results even in frames where camera moves
        auto& lightingPass = graph.addPass<Carrot::Render::PassData::LightingResources>("lighting",
                                                                 [&](GraphBuilder& graph, Pass<Carrot::Render::PassData::LightingResources>& pass, Carrot::Render::PassData::LightingResources& resolveData)
               {
                    pass.rasterized = false;
                    pass.prerecordable = false; //TODO: since it depends on Lighting descriptor sets which may change, it is not 100% pre-recordable now
                    // TODO (or it should be re-recorded when changes happen)
                    resolveData.gBuffer.readFrom(graph, updateGICells.getData().gbuffer, vk::ImageLayout::eShaderReadOnlyOptimal);

                    addDenoisingResources("Ambient Occlusion", vk::Format::eR8Unorm, resolveData.ambientOcclusion);
                    addDenoisingResources("Direct Lighting", vk::Format::eR32G32B32A32Sfloat, resolveData.directLighting);

                    resolveData.reflectionsNoisy = graph.createStorageTarget("Reflections (noisy)",
                                                                    vk::Format::eR32G32B32A32Sfloat,
                                                                    framebufferSize,
                                                                    vk::ImageLayout::eGeneral);

                    resolveData.hashGrid = HashGrid::write(graph, reuseGICells.getData().hashGrid);
               },
               [framebufferSize, applyDenoising](const Render::CompiledPass& pass, const Render::Context& frame, const Carrot::Render::PassData::LightingResources& data, vk::CommandBuffer& cmds) {
                   ZoneScopedN("CPU RenderGraph lighting");
                   TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "lighting");
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

                   applyDenoising(pass, frame, "lighting/denoise-ao", data.gBuffer, data.ambientOcclusion, 0, cmds);
                   applyDenoising(pass, frame, "lighting/denoise-direct", data.gBuffer, data.directLighting, 1, cmds);
               },
               [&](const CompiledPass& pass, const PassData::LightingResources& data) {
               }
        );

        lightingPass.setSwapchainRecreation([&](const CompiledPass& pass, const PassData::LightingResources& data) {
            GetVulkanDriver().performSingleTimeGraphicsCommands([&](vk::CommandBuffer& cmds) {
                for(int i = 0; i < GetEngine().getSwapchainImageCount(); i++) {
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
        };
        auto& debugGICells = graph.addPass<GIDebug>("debug-gi",
            [&](GraphBuilder& graph, Pass<GIDebug>& pass, GIDebug& data) {
                data.gbuffer.readFrom(graph, lightingPass.getData().gBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.gi.hashGrid = HashGrid::write(graph, reuseGICells.getData().hashGrid);
                data.output = graph.createStorageTarget("gi-debug", vk::Format::eR8G8B8A8Unorm, framebufferSize, vk::ImageLayout::eGeneral);
            },
            [](const Render::CompiledPass& pass, const Render::Context& frame, const GIDebug& data, vk::CommandBuffer& cmds) {
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

        struct GIFinal {
            PassData::GBuffer gbuffer;
            GIData gi;
            PassData::Denoising output;
        };
        auto& getGIResults = graph.addPass<GIFinal>("gi",
            [&](GraphBuilder& graph, Pass<GIFinal>& pass, GIFinal& data) {
                data.gbuffer.readFrom(graph, lightingPass.getData().gBuffer, vk::ImageLayout::eShaderReadOnlyOptimal);
                data.gi.hashGrid = HashGrid::write(graph, reuseGICells.getData().hashGrid);
                addDenoisingResources("gi", vk::Format::eR8G8B8A8Unorm, data.output);
                data.output.iterationCount = 1;
            },
            [preparePushConstant, applyDenoising](const Render::CompiledPass& pass, const Render::Context& frame, const GIFinal& data, vk::CommandBuffer& cmds) {
                {
                    TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Final GI");

                    auto pipeline = frame.renderer.getOrCreatePipeline("lighting/gi/apply-gi", (std::uint64_t)&pass);
                    auto& outputTexture = pass.getGraph().getTexture(data.output.noisy, frame.swapchainIndex);

                    frame.renderer.pushConstants("push", *pipeline, frame, vk::ShaderStageFlagBits::eCompute, cmds,
                        frame.renderer.getFrameCount());
                    HashGrid::bind(data.gi.hashGrid, pass.getGraph(), frame, *pipeline, 0);
                    frame.renderer.bindStorageImage(*pipeline, frame, outputTexture, 1, 0, vk::ImageAspectFlagBits::eColor, vk::ImageViewType::e2D, 0, vk::ImageLayout::eGeneral);
                    data.gbuffer.bindInputs(*pipeline, frame, pass.getGraph(), 2, vk::ImageLayout::eShaderReadOnlyOptimal);
                    pipeline->bind({}, frame, cmds, vk::PipelineBindPoint::eCompute);

                    const std::size_t localSize = 32;
                    const std::size_t groupX = (outputTexture.getSize().width + localSize-1) / localSize;
                    const std::size_t groupY = (outputTexture.getSize().height + localSize-1) / localSize;
                    cmds.dispatch(groupX, groupY, 1);
                }

                {
                    TracyVkZone(GetEngine().tracyCtx[frame.swapchainIndex], cmds, "Denoise GI");
                    applyDenoising(pass, frame, "lighting/denoise-direct", data.gbuffer, data.output, 2, cmds);
                }
            });

        PassData::Lighting data {
            .ambientOcclusion = lightingPass.getData().ambientOcclusion.pingPong[(lightingPass.getData().ambientOcclusion.iterationCount+1) % 2],
            .direct = lightingPass.getData().directLighting.pingPong[(lightingPass.getData().directLighting.iterationCount+1) % 2],
            .gi = getGIResults.getData().output.pingPong[(getGIResults.getData().output.iterationCount+1) % 2],
            //.gi = getGIResults.getData().output.noisy,
            .reflections = lightingPass.getData().reflectionsNoisy,
        };
        return data;
    }
}