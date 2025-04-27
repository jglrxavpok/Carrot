//
// Created by jglrxavpok on 27/04/25.
//

#pragma once

template<typename TResource>
class DeferredDestruction {
public:
    explicit DeferredDestruction(std::string name, TResource&& toDestroy, std::size_t framesBeforeDestruction):
        countdown(framesBeforeDestruction),
        resourceName(std::move(name))
    {
        resource = std::move(toDestroy);
    };
    explicit DeferredDestruction(const std::string& name, TResource&& toDestroy): DeferredDestruction(name, std::move(toDestroy), MAX_FRAMES_IN_FLIGHT) {};

    bool isReadyForDestruction() const {
        return countdown == 0;
    }

    void tickDown() {
        if(countdown > 0)
            countdown--;
    }

private:
    std::string resourceName; // destroyed AFTER the resource, helps debug use-after-free
public:
    TResource resource{};
private:
    std::size_t countdown = 0; /* number of frames before destruction (swapchain image count by default)*/
};