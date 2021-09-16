//
// Created by jglrxavpok on 14/09/2021.
//

#include <sol/sol.hpp>
#include "engine/render/resources/Mesh.h"
#include "engine/render/resources/Texture.h"

namespace sol {
    template <>
    struct is_automagical<Carrot::Render::Texture::Ref> : std::false_type {};
}

namespace Carrot::Render {
    void Texture::registerUsertype(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
        auto renderNamespace = carrotNamespace["Render"].get_or_create<sol::table>();

        renderNamespace.new_usertype<Texture>("Texture",
                                              sol::no_constructor, // TODO: define constructors for Lua
                                              "getSize", &Texture::getSize
                                              );

        auto textureNamespace = renderNamespace["Texture"].get_or_create<sol::table>();
        textureNamespace.new_usertype<Texture::Ref>("Ref",
                                                    sol::no_constructor,
                                                    "get",
                                                    [](Texture::Ref& ref) {
                                                        return std::ref(*ref);
                                                    }
                                                    );
    }
}