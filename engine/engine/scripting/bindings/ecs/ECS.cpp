//
// Created by jglrxavpok on 19/05/2022.
//

#include "ECS.h"
#include "engine/ecs/components/Component.h"
#include "engine/ecs/systems/System.h"
#include "core/io/Logging.hpp"

namespace Carrot::ECS {
    void registerBindings(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
        auto ecsNamespace = carrotNamespace["ECS"].get_or_create<sol::table>();

        auto entityUsertype = ecsNamespace.new_usertype<Carrot::ECS::Entity>("Entity", sol::no_constructor,
                                                       "exists", &ECS::Entity::exists,
                                                       "addTag", &ECS::Entity::addTag,
                                                       "getTags", &ECS::Entity::getTags,
                                                       "parent", sol::property([](Carrot::ECS::Entity& e) { return e.getParent(); }, [](Carrot::ECS::Entity& e, std::optional<Carrot::ECS::Entity> parent) { return e.setParent(parent); }),
                                                       "name", sol::property([](Carrot::ECS::Entity& e) { return e.getName(); }, [](Carrot::ECS::Entity& e, std::string name) { e.updateName(name); }),
                                                       "world", sol::property([](ECS::Entity& e) { return e.getWorld(); }),
                                                       "id", sol::property([](ECS::Entity& e) { return e.getID(); })
        );

        getComponentLibrary().registerBindings(destination, entityUsertype);

    }

}