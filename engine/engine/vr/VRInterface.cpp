//
// Created by jglrxavpok on 13/07/2021.
//
#ifdef ENABLE_VR
#include "VRInterface.h"
#include "engine/utils/Macros.h"
#include "engine/io/Logging.hpp"
#include "engine/utils/stringmanip.h"

namespace Carrot::VR {

    static XrBool32 xrDebugCallback(XrDebugUtilsMessageSeverityFlagsEXT              messageSeverity,
                                XrDebugUtilsMessageTypeFlagsEXT                  messageTypes,
                                const XrDebugUtilsMessengerCallbackDataEXT*      callbackData,
                                void*                                            userData) {
        if(messageSeverity == XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            Carrot::Log::error("OpenXR error: %s (function is %s)", callbackData->message, callbackData->functionName);
        } else if(messageSeverity == XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
            Carrot::Log::warn("OpenXR warning: %s (function is %s)", callbackData->message, callbackData->functionName);
        } else {
            Carrot::Log::info("OpenXR info: %s (function is %s)", callbackData->message, callbackData->functionName);
        }

        Carrot::Log::flush();
        return false;
    }

    Interface::Interface(Engine& engine): engine(engine) {
        std::vector<const char*> apiLayers {
                //"XR_APILAYER_LUNARG_core_validation"
        };
        std::vector<const char*> extensions {
                XR_EXT_DEBUG_UTILS_EXTENSION_NAME,
                XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME
        };

        xr::DebugUtilsMessengerCreateInfoEXT debugCreate = {
                xr::DebugUtilsMessengerCreateInfoEXT { xr::DebugUtilsMessageSeverityFlagBitsEXT::AllBits,
                        //xr::DebugUtilsMessageSeverityFlagBitsEXT::Error | xr::DebugUtilsMessageSeverityFlagBitsEXT::Warning | xr::DebugUtilsMessageSeverityFlagBitsEXT::Info,
                                                       xr::DebugUtilsMessageTypeFlagBitsEXT::AllBits,
                                                       &xrDebugCallback,
                                                       this }
        };
        xr::InstanceCreateInfo createInfo;
        createInfo.createFlags = xr::InstanceCreateFlagBits::None;
        createInfo.enabledApiLayerCount = apiLayers.size();
        createInfo.enabledApiLayerNames = apiLayers.data();
        createInfo.enabledExtensionCount = extensions.size();
        createInfo.enabledExtensionNames = extensions.data();
        createInfo.applicationInfo.engineVersion = engine.getConfiguration().engineVersion;
        strcpy_s(createInfo.applicationInfo.engineName, sizeof(createInfo.applicationInfo.engineName), engine.getConfiguration().engineName);
        createInfo.applicationInfo.applicationVersion = engine.getConfiguration().applicationVersion;
        strcpy_s(createInfo.applicationInfo.applicationName, sizeof(createInfo.applicationInfo.applicationName), engine.getConfiguration().applicationName.c_str());
        createInfo.applicationInfo.apiVersion = xr::Version(engine.getConfiguration().openXRApiVersion);

        createInfo.next = &debugCreate;
        xrInstance = xr::createInstanceUnique(createInfo);

        messenger = getXRInstance().createDebugUtilsMessengerUniqueEXT(debugCreate, getXRDispatch());

        systemID = getXRInstance().getSystem(xr::SystemGetInfo(
                xr::FormFactor::HeadMountedDisplay
        ));
    }

    std::unique_ptr<Session> Interface::createSession() {
        return std::make_unique<Session>(*this);
    }

    xr::SystemId Interface::getXRSystemID() {
        return systemID;
    }

    xr::Instance& Interface::getXRInstance() {
        return *xrInstance;
    }

    bool Interface::hasHMD() {
        try {
            DISCARD(getXRSystemID());
            return true;
        } catch (xr::exceptions::FormFactorUnsupportedError&) {
            return false;
        } catch (xr::exceptions::FormFactorUnavailableError&) {
            return false;
        }
    }

    xr::DispatchLoaderDynamic& Interface::getXRDispatch() {
        //static xr::DispatchLoaderDynamic dispatch(getXRInstance());
        static xr::DispatchLoaderDynamic dispatch = xr::DispatchLoaderDynamic::createFullyPopulated(getXRInstance(), &xrGetInstanceProcAddr);
        return dispatch;
    }

    void Interface::pollEvents() {
        xr::EventDataBuffer event;
        xr::Result result;
        do {
            result = getXRInstance().pollEvent(event);
            if(result == xr::Result::Success) {
                handleEvent(event);
            }
        } while(result == xr::Result::Success);
    }

    void Interface::handleEvent(xr::EventDataBuffer event) {
        #define eventAs(type) (*reinterpret_cast<type *>(&event))
        switch(event.type) {
            case xr::StructureType::EventDataSessionStateChanged: {
                auto& sessionStateChanged = eventAs(xr::EventDataSessionStateChanged);
                auto* session = sessions[sessionStateChanged.session];
                assert(session);
                session->stateChanged(sessionStateChanged.time, sessionStateChanged.state);
            } break;

            default:
                // TODO
                break;
        }
        #undef eventAs
    }

    void Interface::registerSession(Session *session) {
        assert(session != nullptr);
        sessions[session->getXRSession()] = session;
    }

    void Interface::unregisterSession(Session *session) {
        assert(session == sessions[session->getXRSession()]);
        sessions[session->getXRSession()] = nullptr;
    }

    vk::UniqueInstance Interface::createVulkanInstance(const vk::InstanceCreateInfo& createInfo, const vk::AllocationCallbacks* allocationCallbacks) {
        vk::Result result;
        vk::Instance instance;
        xr::VulkanInstanceCreateInfoKHR instanceCreate;
        instanceCreate.systemId = systemID;
        instanceCreate.vulkanAllocator = reinterpret_cast<const VkAllocationCallbacks *>(allocationCallbacks);
        instanceCreate.vulkanCreateInfo = reinterpret_cast<const VkInstanceCreateInfo*>(&createInfo);
        instanceCreate.createFlags = xr::VulkanInstanceCreateFlagBitsKHR::None;
        instanceCreate.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
        xrInstance->createVulkanInstanceKHR(instanceCreate, reinterpret_cast<VkInstance *>(&instance),
                                            reinterpret_cast<VkResult *>(&result), getXRDispatch());

        if(result != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create VR-compatible Vulkan instance");
        }

        return std::move(vk::UniqueInstance(instance));
    }

    vk::UniqueDevice Interface::createVulkanDevice(const vk::DeviceCreateInfo& createInfo, const vk::AllocationCallbacks* allocationCallbacks) {
        vk::Result result;
        vk::Device device;

        xr::VulkanDeviceCreateInfoKHR deviceCreate;
        deviceCreate.pfnGetInstanceProcAddr = &vkGetInstanceProcAddr;
        deviceCreate.createFlags = xr::VulkanDeviceCreateFlagBitsKHR::None;
        deviceCreate.vulkanCreateInfo = reinterpret_cast<const VkDeviceCreateInfo*>(&createInfo);
        deviceCreate.systemId = systemID;
        deviceCreate.vulkanAllocator = reinterpret_cast<const VkAllocationCallbacks*>(allocationCallbacks);
        deviceCreate.vulkanPhysicalDevice = getEngine().getVulkanDriver().getPhysicalDevice();

        xrInstance->createVulkanDeviceKHR(deviceCreate, reinterpret_cast<VkDevice *>(&device), reinterpret_cast<VkResult*>(&result), getXRDispatch());

        if(result != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to create VR-compatible Vulkan device");
        }

        return vk::UniqueDevice(device);
    }

    vk::PhysicalDevice Interface::getVulkanPhysicalDevice() {
        xr::VulkanGraphicsDeviceGetInfoKHR getInfo;
        getInfo.systemId = systemID;
        getInfo.vulkanInstance = engine.getVulkanDriver().getInstance();
        return xrInstance->getVulkanGraphicsDevice2KHR(getInfo, getXRDispatch());
    }
}
#endif