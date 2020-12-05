//
// Created by jglrxavpok on 05/12/2020.
//

#include <glm/gtc/matrix_transform.hpp>
#include "Unit.h"

Carrot::Unit::Unit(Unit::Type type, Carrot::InstanceData& instanceData): instanceData(instanceData), type(type) {
    switch (type) {
        case Type::Blue:
            color = {0,0,1};
            break;
        case Type::Red:
            color = {1,0,0};
            break;
        case Type::Green:
            color = {0,1,0};
            break;
    }
}

void Carrot::Unit::update(float dt) {
    glm::vec3 direction = target-position;
    if(direction.length() > 0.01) {
        auto normalizedDirection = glm::vec3(direction.x / direction.length(), direction.y / direction.length(), direction.z / direction.length());
        float angle = atan2(normalizedDirection.y, normalizedDirection.x);
        rotation = glm::angleAxis(angle, glm::vec3(0,0,1));
        position += normalizedDirection * dt;
    }
    // TODO: move randomly

    instanceData.color = { color.r, color.g, color.b, 1.0f };
    auto modelRotation = glm::rotate(rotation, -glm::pi<float>()/2, glm::vec3(0,0,01));
    instanceData.transform = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(modelRotation);
}

void Carrot::Unit::teleport(const glm::vec3& newPos) {
    position = newPos;
}

Carrot::Unit::Type Carrot::Unit::getType() const {
    return type;
}

void Carrot::Unit::moveTo(const glm::vec3& targetPosition) {
    target = targetPosition;
}

glm::vec3 Carrot::Unit::getPosition() const {
    return position;
}
