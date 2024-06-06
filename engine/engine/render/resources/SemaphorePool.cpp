//
// Created by jglrxavpok on 06/06/2024.
//

#include "SemaphorePool.h"

#include <engine/utils/Macros.h>

namespace Carrot::Render {
    SemaphorePool::SemaphorePool() {}

    void SemaphorePool::reset() {
        Async::LockGuard g { lock };
        cursor = 0;
    }

    vk::Semaphore SemaphorePool::get() {
        Async::LockGuard g { lock };

        // no more blocks, allocate a new one
        if(cursor >= blocks.size() * Block::Size) {
            auto& block = blocks.emplaceBack();
            for(std::size_t i = 0; i < Block::Size; i++) {
                block.semaphores[i] = GetVulkanDevice().createSemaphoreUnique(vk::SemaphoreCreateInfo{});
            }
        }
        const vk::Semaphore r = *(blocks[cursor / Block::Size].semaphores[cursor % Block::Size]);
        cursor++;
        return r;
    }


} // Carrot::Render