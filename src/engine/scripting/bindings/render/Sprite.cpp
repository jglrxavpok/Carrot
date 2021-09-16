//
// Created by jglrxavpok on 14/09/2021.
//

#include <sol/sol.hpp>
#include <utility>
#include "engine/render/Sprite.h"

namespace Carrot::Render {
    void Sprite::registerUsertype(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
        auto renderNamespace = carrotNamespace["Render"].get_or_create<sol::table>();

        renderNamespace.new_usertype<Carrot::Render::Sprite>("Sprite",
                                                             sol::meta_function::construct,
                                                             sol::factories(
                                                                     [](VulkanRenderer& renderer, Carrot::Render::Texture::Ref texture) {
                                                                         return Sprite(renderer, std::move(texture));
                                                                     },
                                                                     [](VulkanRenderer& renderer, Carrot::Render::Texture::Ref texture, Carrot::Math::Rect2Df textureRegion) {
                                                                         return Sprite(renderer, std::move(texture), textureRegion);
                                                                     }
                                                             ),
                                                             "tick", &Sprite::tick,
                                                             "onFrame", &Sprite::onFrame,
                                                             "soloGBufferRender", &Sprite::soloGBufferRender,
                                                             "size", &Sprite::size,
                                                             "rotation", &Sprite::rotation,
                                                             "position", &Sprite::position,
                                                             "getRenderer", &Sprite::getRenderer
                                                             );
    }
}