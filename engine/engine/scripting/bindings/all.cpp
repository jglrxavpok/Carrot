//
// Created by jglrxavpok on 16/09/2021.
//

#include <sol/sol.hpp>
#include "glm.hpp"
#include "vulkan/vulkan.hpp"
#include "ecs/ECS.h"
#include "math/bindings.h"
#include "engine/ecs/components/Component.h"
#include "engine/ecs/systems/System.h"
#include "engine/render/Sprite.h"
#include "engine/render/resources/Texture.h"
#include "engine/render/VulkanRenderer.h"
#include "engine/Engine.h"
#include "engine/vulkan/VulkanDriver.h"

namespace Carrot::Lua {
    void registerAllUsertypes(sol::state& destination) {
        registerGLMUsertypes(destination);
        registerVulkanUsertypes(destination);
        Carrot::Render::Context::registerUsertype(destination);
        Carrot::Engine::registerUsertype(destination);
        Carrot::VulkanDriver::registerUsertype(destination);
        Carrot::VulkanRenderer::registerUsertype(destination);
        Carrot::Render::Texture::registerUsertype(destination);
        Carrot::Render::Sprite::registerUsertype(destination);
        Carrot::Math::registerUsertypes(destination);

        Carrot::ECS::registerBindings(destination);
    }
}