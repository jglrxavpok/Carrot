#pragma once

// This file exists because Tracy does not play nice with pre-recorded buffers
//
//
// or maybe I don't understand how it works, so let's reinvent the wheel.
#include "vulkan/includes.h"
#ifdef TRACY_ENABLE

#include <Tracy.hpp>
#include <client/TracyProfiler.hpp>
#include <iostream>
#include <vector>
#include <stack>

struct Scope {
    tracy::QueueGpuZoneBegin begin{};
    tracy::QueueGpuZoneEnd end{};
    std::vector<Scope> children{};
};

class TracyVulkanContext {
private:
    vk::PhysicalDevice& physicalDevice;
    vk::Device& device;
    vk::Queue& queue;
    vk::UniqueCommandPool commandPool{};
    vk::UniqueQueryPool queryPool{};
    uint64_t availableQueryCount = MaxQueryCount;
    uint64_t currentQueryID = 0;
    uint8_t tracyContextID = -1;

    uint64_t totalPerformedQueries = 0;

    std::stack<Scope> scopes{};

    void writeRecurse(const Scope& scope) const;

public:
    static constexpr uint32_t MaxQueryCount = 64*1024; //64k

    explicit TracyVulkanContext(vk::PhysicalDevice& physicalDevice, vk::Device& device, vk::Queue& queue, uint32_t queueFamilyIndex, vk::AllocationCallbacks* allocator = nullptr);

    void prepareForTracing(vk::CommandBuffer& commands);

    void collect();

    ~TracyVulkanContext() = default;

    uint64_t nextQueryID();

    vk::QueryPool& getQueryPool();

    uint8_t getTracyID() const;

    tracy::QueueGpuZoneBegin& newZoneBegin();

    tracy::QueueGpuZoneEnd& newZoneEnd();
};

class TracyVulkanScope {
private:
    TracyVulkanContext& tracyContext;
    vk::CommandBuffer& commands;
    std::string name;

public:
    explicit TracyVulkanScope(TracyVulkanContext& tracyContext, vk::CommandBuffer& commands, const std::string& name, const tracy::SourceLocationData* source);

    ~TracyVulkanScope();
};

#define TracyVulkanNamedZone(context, commandBuffer, name, varname) \
    static constexpr tracy::SourceLocationData TracyConcat(__tracy_gpu_source_location,__LINE__) { name, __FUNCTION__,  __FILE__, (uint32_t)__LINE__, 0 }; \
    TracyVulkanScope varname( context, commandBuffer, name, &TracyConcat(__tracy_gpu_source_location,__LINE__));

#define TracyVulkanZone(context, commandBuffer, name) TracyVulkanNamedZone(context, commandBuffer, name, ____tracy_vulkan_zone)
#define PrepareVulkanTracy(context, commandBuffer) context->prepareForTracing(commandBuffer)
#define TracyVulkanCollect(context) context->collect()

#else
struct TracyVulkanContext {
    explicit TracyVulkanContext(vk::PhysicalDevice& physicalDevice, vk::Device& device, vk::Queue& queue, uint32_t queueFamilyIndex, vk::AllocationCallbacks* allocator = nullptr) {}
};
#define TracyVulkanZone(context, commandBuffer, name)
#define TracyVulkanNamedZone(context, commandBuffer, name)
#define TracyVulkanContext(physicalDevice, device, queue, queueIndex) ((void*)0)
#define FrameMark
#define ZoneScoped
#define ZoneScopedN
#define ZoneNamedN(varname, name, active)
#define PrepareVulkanTracy(context, commandBuffer)
#define TracyVulkanCollect(context)
#endif