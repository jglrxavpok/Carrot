#include "CustomTracyVulkan.h"

#ifdef TRACY_ENABLE
void TracyVulkanContext::writeRecurse(const Scope& scope) const {
    // write zones
    for(const auto& child : scope.children) {
        auto item = tracy::Profiler::QueueSerial();
        tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuZoneBeginSerial );
        tracy::MemWrite( &item->gpuZoneBegin.cpuTime, child.begin.cpuTime );
        tracy::MemWrite( &item->gpuZoneBegin.srcloc, child.begin.srcloc );
        tracy::MemWrite( &item->gpuZoneBegin.thread, child.begin.thread );
        tracy::MemWrite( &item->gpuZoneBegin.queryId, totalPerformedQueries+child.begin.queryId );
        tracy::MemWrite( &item->gpuZoneBegin.context, child.begin.context );
        tracy::Profiler::QueueSerialFinish();

        writeRecurse(child);

        auto item2 = tracy::Profiler::QueueSerial();
        tracy::MemWrite( &item2->hdr.type, tracy::QueueType::GpuZoneEndSerial );
        tracy::MemWrite( &item2->gpuZoneEnd.cpuTime, child.end.cpuTime );
        tracy::MemWrite( &item2->gpuZoneEnd.thread, child.end.thread );
        tracy::MemWrite( &item2->gpuZoneEnd.queryId, totalPerformedQueries+child.end.queryId );
        tracy::MemWrite( &item2->gpuZoneEnd.context, child.end.context );
        tracy::Profiler::QueueSerialFinish();
    }
}


TracyVulkanContext::TracyVulkanContext(vk::PhysicalDevice& physicalDevice, vk::Device& device, vk::Queue& queue, uint32_t queueFamilyIndex, vk::AllocationCallbacks* allocator):
        physicalDevice(physicalDevice), device(device), queue(queue), tracyContextID(tracy::GetGpuCtxCounter().fetch_add( 1, std::memory_order_relaxed )) {
    const float gpuPeriod = physicalDevice.getProperties().limits.timestampPeriod;

    commandPool = device.createCommandPoolUnique(vk::CommandPoolCreateInfo {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queueFamilyIndex,
    }, allocator);

    // create query pool
    queryPool = device.createQueryPoolUnique(vk::QueryPoolCreateInfo {
            .queryType = vk::QueryType::eTimestamp,
            .queryCount = MaxQueryCount,
    }, allocator);

    auto cmd = device.allocateCommandBuffers(vk::CommandBufferAllocateInfo {
            .commandPool = *commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
    })[0];

    // reset query pool
    cmd.begin(vk::CommandBufferBeginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });
    cmd.resetQueryPool(*queryPool, 0, availableQueryCount);
    cmd.end();
    queue.submit(vk::SubmitInfo {
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
    }, nullptr);
    queue.waitIdle();

    // get GPU time to query #0
    cmd.begin(vk::CommandBufferBeginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });
    cmd.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, *queryPool, 0);
    cmd.end();
    queue.submit(vk::SubmitInfo {
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
    }, nullptr);
    queue.waitIdle();

    // read CPU&GPU Time
    int64_t cpuTime = tracy::Profiler::GetTime();
    int64_t gpuTime = device.getQueryPoolResult<int64_t>(*queryPool, 0, 1, 0, vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);

    // reset query pool
    cmd.begin(vk::CommandBufferBeginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit
    });
    cmd.resetQueryPool(*queryPool, 0, availableQueryCount);
    cmd.end();
    queue.submit(vk::SubmitInfo {
            .commandBufferCount = 1,
            .pCommandBuffers = &cmd,
    }, nullptr);
    queue.waitIdle();

    // no longer need command buffer, free it
    device.freeCommandBuffers(*commandPool, cmd);

    // send timing information to Tracy
    auto item = tracy::Profiler::QueueSerial();
    tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuNewContext );
    tracy::MemWrite( &item->gpuNewContext.cpuTime, cpuTime );
    tracy::MemWrite( &item->gpuNewContext.gpuTime, gpuTime );
    memset( &item->gpuNewContext.thread, 0, sizeof( item->gpuNewContext.thread ) );
    tracy::MemWrite( &item->gpuNewContext.period, gpuPeriod );
    tracy::MemWrite( &item->gpuNewContext.context, tracyContextID );
    tracy::MemWrite( &item->gpuNewContext.flags, 0 );
    tracy::MemWrite( &item->gpuNewContext.type, tracy::GpuContextType::Vulkan );
    // finish sending
    tracy::Profiler::QueueSerialFinish();
}

void TracyVulkanContext::prepareForTracing(vk::CommandBuffer& commands) {
    commands.resetQueryPool(*queryPool, 0, availableQueryCount);
    currentQueryID = 0;
    scopes = {};
    scopes.push({});
}

void TracyVulkanContext::collect() {
    ZoneScoped;
    // read from query pool
    auto [returnCode, results] = device.getQueryPoolResults<int64_t>(*queryPool, 0, currentQueryID, availableQueryCount*sizeof(int64_t), sizeof(int64_t), vk::QueryResultFlagBits::e64);
    if(returnCode == vk::Result::eNotReady) {
        // not ready yet
        return;
    }

    writeRecurse(scopes.top());

    for(size_t index = 0; index < currentQueryID; index++) {
        // send to Tracy
        auto item = tracy::Profiler::QueueSerial();
        tracy::MemWrite( &item->hdr.type, tracy::QueueType::GpuTime );
        tracy::MemWrite( &item->gpuTime.gpuTime, results[index] );
        tracy::MemWrite( &item->gpuTime.queryId, static_cast<uint16_t>(totalPerformedQueries+index));
        tracy::MemWrite( &item->gpuTime.context, tracyContextID );
        tracy::Profiler::QueueSerialFinish();
    }

    totalPerformedQueries += currentQueryID;
}


uint64_t TracyVulkanContext::nextQueryID() {
    return currentQueryID++;
}

vk::QueryPool& TracyVulkanContext::getQueryPool() {
    return *queryPool;
}

uint8_t TracyVulkanContext::getTracyID() const {
    return tracyContextID;
}

tracy::QueueGpuZoneBegin& TracyVulkanContext::newZoneBegin() {
    scopes.push({});
    return scopes.top().begin;
}

tracy::QueueGpuZoneEnd& TracyVulkanContext::newZoneEnd() {
    auto copyTop = scopes.top();
    scopes.pop();
    auto& children = scopes.top().children;
    children.push_back(copyTop);
    return children[children.size()-1].end;
}

TracyVulkanScope::TracyVulkanScope(TracyVulkanContext& tracyContext, vk::CommandBuffer& commands, const std::string& name, const tracy::SourceLocationData* source):
        tracyContext(tracyContext), commands(commands), name(name)
{
    uint64_t queryID = tracyContext.nextQueryID();
    // write timestamp to command buffer
    commands.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, tracyContext.getQueryPool(), queryID);

    auto& item = tracyContext.newZoneBegin();
    // tell Tracy about new gpu zone
    tracy::MemWrite( &item.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item.srcloc, (uint64_t)source );
    tracy::MemWrite( &item.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item.queryId, uint16_t( queryID ) );
    tracy::MemWrite( &item.context, tracyContext.getTracyID() );
}

TracyVulkanScope::~TracyVulkanScope() {
    uint64_t queryID = tracyContext.nextQueryID();
    // write timestamp to command buffer
    commands.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, tracyContext.getQueryPool(), queryID);

    // tell Tracy about end of gpu zone
    auto& item = tracyContext.newZoneEnd();
    tracy::MemWrite( &item.cpuTime, tracy::Profiler::GetTime() );
    tracy::MemWrite( &item.thread, tracy::GetThreadHandle() );
    tracy::MemWrite( &item.queryId, uint16_t( queryID ) );
    tracy::MemWrite( &item.context, tracyContext.getTracyID() );
}

#endif