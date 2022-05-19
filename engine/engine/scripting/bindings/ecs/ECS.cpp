//
// Created by jglrxavpok on 19/05/2022.
//

#include "ECS.h"
#include "engine/ecs/components/Component.h"
#include "engine/ecs/systems/System.h"
#include "core/io/Logging.hpp"

#pragma optimize("", off)

namespace Carrot::ECS {
    class ComponentWrapper {
    public:
        ComponentWrapper(Component* component): component(component) {}

        bool isPresent() const {
            return component != nullptr;
        }

        Component* get() const {
            return component;
        }

    private:
        Component* component;
    };

    struct ComponentsWrapper {
        Entity entity;

        explicit ComponentsWrapper(Entity e): entity(e) {}
    };

    void registerBindings(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
        auto ecsNamespace = carrotNamespace["ECS"].get_or_create<sol::table>();

        ecsNamespace.new_usertype<ComponentWrapper>("ComponentWrapper", sol::no_constructor,
                                                                            "present", sol::property([](ComponentWrapper& w) { return w.isPresent(); }),
                                                                            "value", sol::property([](ComponentWrapper& w) { return w.get(); })
        );

        auto componentListWrapper = ecsNamespace.new_usertype<ComponentsWrapper>("ComponentsWrapper", sol::no_constructor);
        for(const auto& id : getComponentLibrary().getAllIDs()) {
            auto compID = Carrot::getIDFromName(id);
            verify(compID.has_value(), Carrot::sprintf("Missing mapping between id and name for name: %s", id.c_str()));

            componentListWrapper.set(id, sol::property([compID](ComponentsWrapper& w) {
                auto compRef = w.entity.getComponent(compID.value());
                Component* componentPtr = compRef.hasValue() ? compRef.asPtr() : nullptr;
                return ComponentWrapper(componentPtr);
            }));
        }

        ecsNamespace.new_usertype<Carrot::ECS::Entity>("Entity", sol::no_constructor,
                                                       "exists", &ECS::Entity::exists,
                                                       "addTag", &ECS::Entity::addTag,
                                                       "getTags", &ECS::Entity::getTags,
                                                       "parent", sol::property([](Carrot::ECS::Entity& e) { return e.getParent(); }, [](Carrot::ECS::Entity& e, std::optional<Carrot::ECS::Entity> parent) { return e.setParent(parent); }),
                                                       "name", sol::property([](Carrot::ECS::Entity& e) { return e.getName(); }, [](Carrot::ECS::Entity& e, std::string name) { e.updateName(name); }),
                                                       "world", sol::property([](ECS::Entity& e) { return e.getWorld(); }),
                                                       "id", sol::property([](ECS::Entity& e) { return e.getID(); }),
                                                       "components", sol::property([](ECS::Entity& entity) { return ComponentsWrapper(entity); })

        );
    }
}