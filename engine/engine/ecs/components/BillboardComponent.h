#pragma once

#include <engine/ecs/components/Component.h>

namespace Carrot::ECS {

    /**
     * Used to tag entities that should always face the camera
     */
    class BillboardComponent : public Carrot::ECS::IdentifiableComponent<BillboardComponent> {
    public:
        explicit BillboardComponent(Carrot::ECS::Entity entity);

        explicit BillboardComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity) : BillboardComponent(
                std::move(entity)) {
        };

        rapidjson::Value toJSON(rapidjson::Document& doc) const override {
            rapidjson::Value obj(rapidjson::kObjectType);
            return obj;
        }

        const char *const getName() const override {
            return "Billboard";
        }

        std::unique_ptr<Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;

    private:
    };
}

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::BillboardComponent>::getStringRepresentation() {
    return "Billboard";
}