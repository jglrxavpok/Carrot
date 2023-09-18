//
// Created by jglrxavpok on 19/05/2021.
//

#pragma once

#include <stdexcept>
#include <core/Macros.h>

namespace Carrot {
    class Engine;
    class ResourceAllocator;
    class VulkanRenderer;
    class VulkanDriver;
    struct Configuration;
    struct Capabilities;
    class TaskScheduler;
    class SceneManager;
    class AssetServer;

    namespace Audio {
        class AudioManager;
    }

    namespace Scripting {
        class CSharpBindings;
        class ScriptingEngine;
    }

    namespace Physics {
        class PhysicsSystem;
    }

}

Carrot::Engine& GetEngine();
Carrot::VulkanRenderer& GetRenderer();
Carrot::VulkanDriver& GetVulkanDriver();
Carrot::ResourceAllocator& GetResourceAllocator();
vk::Device& GetVulkanDevice();
void WaitDeviceIdle();
const Carrot::Capabilities& GetCapabilities();
const Carrot::Configuration& GetConfiguration();
Carrot::TaskScheduler& GetTaskScheduler();
Carrot::Scripting::ScriptingEngine& GetCSharpScripting();
Carrot::Scripting::CSharpBindings& GetCSharpBindings();
Carrot::SceneManager& GetSceneManager();
Carrot::Audio::AudioManager& GetAudioManager();
Carrot::AssetServer& GetAssetServer();

#undef GetVFS
#define GetVFS() GetEngine().getVFS()

Carrot::Physics::PhysicsSystem& GetPhysics();