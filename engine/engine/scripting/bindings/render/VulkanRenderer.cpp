//
// Created by jglrxavpok on 14/09/2021.
//
#include <sol/sol.hpp>
#include "engine/render/resources/Mesh.h"
#include "engine/render/GBuffer.h"
#include "engine/render/raytracing/ASBuilder.h"
#include "engine/render/raytracing/RayTracer.h"
#include "engine/render/VulkanRenderer.h"

namespace Carrot {
    void VulkanRenderer::registerUsertype(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
    }
}