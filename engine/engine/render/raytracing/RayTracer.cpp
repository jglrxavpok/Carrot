//
// Created by jglrxavpok on 29/12/2020.
//

#include <engine/render/shaders/ShaderStages.h>
#include "RayTracer.h"
#include "engine/render/resources/Image.h"
#include "engine/render/CameraBufferObject.h"
#include "engine/render/TextureRepository.h"
#include "ASBuilder.h"
#include "SceneElement.h"
#include <iostream>

#include <engine/Capabilities.h>

#include <engine/render/VulkanRenderer.h>
#include <engine/vulkan/VulkanDriver.h>
#include <engine/Engine.h>

constexpr int maxInstances = 301;

static constexpr float ResolutionScale = 0.5f;

Carrot::RayTracer::RayTracer(Carrot::VulkanRenderer& renderer): renderer(renderer) {
    available = GetCapabilities().supportsRaytracing;

    if(!available)
        return;
    // init raytracing
    auto properties = renderer.getVulkanDriver().getPhysicalDevice().getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
    rayTracingProperties = properties.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
}

void Carrot::RayTracer::onFrame(const Carrot::Render::Context& renderContext) {
    if(!available)
        return;
    if(renderContext.pViewport != &renderer.getEngine().getMainViewport())
        return;
    ZoneScoped;
    renderer.getASBuilder().startFrame();
}

std::vector<const char*> Carrot::RayTracer::getRequiredDeviceExtensions() {
    return {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    };
}

void Carrot::RayTracer::onSwapchainImageCountChange(std::size_t newCount) {}

void Carrot::RayTracer::onSwapchainSizeChange(Window& window, int newWidth, int newHeight) {
}
