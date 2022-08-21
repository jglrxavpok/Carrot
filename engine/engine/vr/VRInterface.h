//
// Created by jglrxavpok on 13/07/2021.
//

#pragma once

#include <memory>
#include "engine/vr/includes.h"
#include "engine/vr/Session.h"
#include "engine/Engine.h"

namespace Carrot::VR {
    //! Interface to communicate with OpenXR. Carrot does not expose all of OpenXR directly (contrary to Vulkan)
    class Interface {
    public:
        explicit Interface(Carrot::Engine& engine);

    public:
        bool hasHMD();

        //! Is locating individual joints of hands available ?
        bool supportsHandTracking() const;

        Carrot::Engine& getEngine() { return engine; }

    public: // Create VR-compatible Vulkan objects
        /// Creates a VR-compatible Vulkan instance
        vk::UniqueInstance createVulkanInstance(const vk::InstanceCreateInfo& createInfo, const vk::AllocationCallbacks* allocationCallbacks = nullptr);
        vk::UniqueDevice createVulkanDevice(const vk::DeviceCreateInfo& createInfo, const vk::AllocationCallbacks* allocationCallbacks = nullptr);
        vk::PhysicalDevice getVulkanPhysicalDevice();

    public:
        std::unique_ptr<Session> createSession();
        void pollEvents();

    private: // OpenXR interfacing. Contrary to Vulkan, Carrot limits what is available to the games: the reasoning is that games should not interface with the VR device directly.
        xr::Instance& getXRInstance();
        xr::SystemId getXRSystemID();
        xr::DispatchLoaderDynamic& getXRDispatch();

    private:
        void handleEvent(xr::EventDataBuffer buffer);

        void registerSession(Session* session);
        void unregisterSession(Session* session);

    private:
        Carrot::Engine& engine;
        xr::UniqueInstance xrInstance{};
        xr::UniqueDynamicDebugUtilsMessengerEXT messenger{};
        xr::SystemId systemID;
        xr::SystemProperties systemProperties;
        xr::SystemHandTrackingPropertiesEXT handTrackingProperties;
        std::unordered_map<xr::Session::RawHandleType, Session*> sessions;

        friend class Session;
    };

}