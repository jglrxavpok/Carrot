//
// Created by jglrxavpok on 14/09/2021.
//

#include <sol/sol.hpp>
#include "engine/utils/Macros.h"

namespace Carrot::Lua {
    void registerVulkanUsertypes(sol::state& destination) {
        auto vkNamespace = destination["vk"].get_or_create<sol::table>();
        vkNamespace.new_usertype<vk::RenderPass>("RenderPass", sol::no_constructor);

        vkNamespace.new_usertype<vk::CommandBuffer>("CommandBuffer",
                                                    sol::no_constructor
            // TODO: add bindings
        );

        vkNamespace.new_usertype<vk::Extent2D>("Extent2D",
                                               sol::call_constructor,
                                               sol::factories(
                                                       [](std::uint32_t w, std::uint32_t h) {
                                                           return vk::Extent2D{ .width = w, .height = h };
                                                       }
                                               ),
                                               "width", &vk::Extent2D::width,
                                               "height", &vk::Extent2D::height
                                               );

        vkNamespace.new_usertype<vk::Extent3D>("Extent3D",
                                               sol::call_constructor,
                                               sol::factories(
                                                       [](std::uint32_t w, std::uint32_t h, std::uint32_t d) {
                                                           return vk::Extent3D{ .width = w, .height = h, .depth = d };
                                                       }
                                               ),
                                               "width", &vk::Extent3D::width,
                                               "height", &vk::Extent3D::height,
                                               "depth", &vk::Extent3D::depth
        );
    }
}