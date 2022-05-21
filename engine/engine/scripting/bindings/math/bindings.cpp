//
// Created by jglrxavpok on 21/05/2022.
//

#include "bindings.h"
#include "engine/math/Transform.h"

namespace Carrot::Math {
    void registerUsertypes(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
        auto renderNamespace = carrotNamespace["Math"].get_or_create<sol::table>();

        renderNamespace.new_usertype<Carrot::Math::Transform>("Transform", sol::default_constructor,
                                                              "position", &Transform::position,
                                                              "rotation", &Transform::rotation,
                                                              "scale", &Transform::scale,
                                                              "toTransformMatrix", sol::overload(
                                                                      [](Transform& t) {
                                                                          return t.toTransformMatrix();
                                                                      },
                                                                      [](Transform& t, const Transform* parent) {
                                                                          return t.toTransformMatrix(parent);
                                                                      })
        );

    }
}