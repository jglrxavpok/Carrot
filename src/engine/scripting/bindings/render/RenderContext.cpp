//
// Created by jglrxavpok on 14/09/2021.
//
#include <sol/sol.hpp>
#include "engine/render/RenderContext.h"
#include "engine/render/VulkanRenderer.h"
#include <functional>

namespace Carrot::Render {
    void Context::registerUsertype(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
        auto renderNamespace = carrotNamespace["Render"].get_or_create<sol::table>();

        renderNamespace.new_usertype<Carrot::Render::Context>("Context", sol::no_constructor,
                                                              // TODO: more members, doing bare minimum right now
                                                              "eye", &Context::eye,
                                                              "frameCount", &Context::frameCount,
                                                              "lastSwapchainIndex", &Context::lastSwapchainIndex,
                                                              "renderer", sol::property([](Carrot::Render::Context& c) {
                                                                  return std::ref(c.renderer);
                                                              }),
                                                              "swapchainIndex", &Context::swapchainIndex
                                                              );
    }
}