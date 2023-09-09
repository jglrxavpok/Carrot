#pragma once

#include <engine/ecs/components/Component.h>

namespace Carrot::ECS {

    class SoundListenerComponent : public Carrot::ECS::IdentifiableComponent<SoundListenerComponent> {
    public:
        bool active = true;

        explicit SoundListenerComponent(Carrot::ECS::Entity entity);

        explicit SoundListenerComponent(const rapidjson::Value& json, Carrot::ECS::Entity entity);

        rapidjson::Value toJSON(rapidjson::Document& doc) const override {
            rapidjson::Value obj(rapidjson::kObjectType);
            obj.AddMember("active", active, doc.GetAllocator());
            return obj;
        }

        const char *const getName() const override {
            return "SoundListener";
        }

        std::unique_ptr<Carrot::ECS::Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;

    private:
    };
}

template<>
inline const char *Carrot::Identifiable<Carrot::ECS::SoundListenerComponent>::getStringRepresentation() {
    return "SoundListener";
}