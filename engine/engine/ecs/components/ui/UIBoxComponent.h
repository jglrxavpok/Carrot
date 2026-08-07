//
// Created by jglrxavpok on 22/06/2026.
//

#pragma once

#include <engine/ecs/components/ComponentReflection.h>
#include <engine/render/AsyncResource.hpp>
#include <engine/render/resources/Texture.h>

namespace Carrot::UI {
    struct Range {
        float min = 0;
        float max = std::numeric_limits<float>::infinity();
    };
    // Grow to maximum size
    struct Grow {
        Range range;
    };
    // Shrink to fit contents
    struct Fit {
        Range range;
    };
    // Fixed size, dictated by scale of entity owning the component
    struct Fixed {};
    // Percent of parent
    struct Percent {
        float ratio;
    };

    using SizeType = std::variant<Grow, Fit, Fixed, Percent>;

    enum class SizeTypeEnum {
        Grow,
        Fit,
        Fixed,
        Percent
    };

    // check that order in variant match enum
    static_assert(std::is_same_v<std::remove_reference_t<decltype(std::get<static_cast<std::size_t>(SizeTypeEnum::Grow)>(std::declval<SizeType>()))>, Grow>);
    static_assert(std::is_same_v<std::remove_reference_t<decltype(std::get<static_cast<std::size_t>(SizeTypeEnum::Fit)>(std::declval<SizeType>()))>, Fit>);
    static_assert(std::is_same_v<std::remove_reference_t<decltype(std::get<static_cast<std::size_t>(SizeTypeEnum::Fixed)>(std::declval<SizeType>()))>, Fixed>);
    static_assert(std::is_same_v<std::remove_reference_t<decltype(std::get<static_cast<std::size_t>(SizeTypeEnum::Percent)>(std::declval<SizeType>()))>, Percent>);

    struct UIBoxComponent: public ECS::ReflectionComponent<UIBoxComponent> {
        SizeType width = Grow{};
        SizeType height = Grow{};
        glm::vec4 color = {1,1,1,1};
        glm::uvec4 padding = {0,0,0,0};
        u16 childGap = 0;
        AsyncTextureResource image;

        // TODO: rounding

        using ReflectionComponent::ReflectionComponent;
        UIBoxComponent(const Carrot::DocumentElement& json, Carrot::ECS::Entity entity);
        Carrot::DocumentElement serialise() const override;
        std::unique_ptr<Component> duplicate(const Carrot::ECS::Entity& newOwner) const override;
    };
} // Carrot::UI

ADD_COMPONENT_ID(Carrot::UI, UIBox)