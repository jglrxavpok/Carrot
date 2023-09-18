//
// Created by jglrxavpok on 30/08/2023.
//

#include <engine/utils/Macros.h>
#include <engine/Engine.h>
#include <engine/physics/PhysicsSystem.h>

Carrot::Engine& GetEngine() {
    return Carrot::Engine::getInstance();
}

Carrot::VulkanRenderer& GetRenderer() {
    return GetEngine().getRenderer();
}

Carrot::VulkanDriver& GetVulkanDriver() {
    return GetEngine().getVulkanDriver();
}

Carrot::ResourceAllocator& GetResourceAllocator() {
    return GetEngine().getResourceAllocator();
}

vk::Device& GetVulkanDevice() {
    return GetVulkanDriver().getLogicalDevice();
}

void WaitDeviceIdle() {
    GetVulkanDriver().waitDeviceIdle();
}

const Carrot::Capabilities& GetCapabilities() {
    return GetEngine().getCapabilities();
}

const Carrot::Configuration& GetConfiguration() {
    return GetEngine().getConfiguration();
}

Carrot::TaskScheduler& GetTaskScheduler() {
    return GetEngine().getTaskScheduler();
}

Carrot::Scripting::ScriptingEngine& GetCSharpScripting() {
    return GetEngine().getCSScriptEngine();
}

Carrot::Scripting::CSharpBindings& GetCSharpBindings() {
    return GetEngine().getCSBindings();
}

Carrot::SceneManager& GetSceneManager() {
    return GetEngine().getSceneManager();
}

Carrot::Audio::AudioManager& GetAudioManager() {
    return GetEngine().getAudioManager();
}

Carrot::AssetServer& GetAssetServer() {
    return GetEngine().getAssetServer();
}

Carrot::Physics::PhysicsSystem& GetPhysics() {
    return Carrot::Physics::PhysicsSystem::getInstance();
}