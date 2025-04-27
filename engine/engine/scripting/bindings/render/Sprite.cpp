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

    }
}