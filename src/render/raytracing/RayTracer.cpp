//
// Created by jglrxavpok on 29/12/2020.
//

#include "RayTracer.h"
#include "render/Image.h"

Carrot::RayTracer::RayTracer(Carrot::Engine& engine): engine(engine) {
    // init raytracing
    auto properties = engine.getPhysicalDevice().getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    rayTracingProperties = properties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

    // init command buffers which will contain the raytracing calls
    commandBuffers = engine.getLogicalDevice().allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = engine.getGraphicsCommandPool(),
            .level = vk::CommandBufferLevel::eSecondary,
            .commandBufferCount = engine.getSwapchainImageCount(),
    });

    // output images
    for(size_t i = 0; i < engine.getSwapchainImageCount(); i++) {
        auto image = make_unique<Image>(engine,
                                        vk::Extent3D{engine.getSwapchainExtent().width, engine.getSwapchainExtent().height, 1},
                                        vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eColorAttachment,
                                        vk::Format::eR32G32B32A32Sfloat);

        auto view = image->createImageView(vk::Format::eR32G32B32A32Sfloat);
        lightingImages.emplace_back(move(image));
        lightingImageViews.emplace_back(move(view));
    }
}

// TODO: setup AS (bottom and top)
// TODO: Setup SBT

void Carrot::RayTracer::onSwapchainRecreation() {
    // TODO
}

vector<vk::CommandBuffer>& Carrot::RayTracer::getCommandBuffers() {
    return commandBuffers;
}

void Carrot::RayTracer::recordCommands(uint32_t frameIndex, vk::CommandBufferInheritanceInfo* inheritance) {
    auto& commands = commandBuffers[frameIndex];
    commands.begin(vk::CommandBufferBeginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eSimultaneousUse,
            .pInheritanceInfo = inheritance,
    });
    engine.updateViewportAndScissor(commands);
    // TODO: actually raytrace
    commands.end();
}

vector<vk::UniqueImageView>& Carrot::RayTracer::getLightingImageViews() {
    return lightingImageViews;
}

vector<const char*> Carrot::RayTracer::getRequiredDeviceExtensions() {
    return {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    };
}