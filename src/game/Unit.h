//
// Created by jglrxavpok on 05/12/2020.
//

#pragma once

#include <render/InstanceData.h>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>


namespace Game {
    using namespace Carrot;

    class Unit {
    public:
        enum class Type {
            Red,
            Green,
            Blue,
        };


    private:
        AnimatedInstanceData& instanceData;
        glm::vec3 position{};
        glm::vec3 color{};
        glm::quat rotation{0.0f,0.0f,0.0f,1.0f};
        Unit::Type type;

        // TODO: test only
        glm::vec3 target{};

    public:
        explicit Unit(Unit::Type type, AnimatedInstanceData& instanceData);

        void teleport(const glm::vec3& newPos);
        void moveTo(const glm::vec3& targetPosition);
        void update(float dt);

        glm::mat4 getTransform() const;
        Type getType() const;
        glm::vec3 getPosition() const;
    };
}
