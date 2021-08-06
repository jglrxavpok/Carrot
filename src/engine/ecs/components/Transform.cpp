//
// Created by jglrxavpok on 26/02/2021.
//
#include "Transform.h"

glm::mat4 Carrot::Transform::toTransformMatrix() const {
    auto modelRotation = glm::rotate(rotation, 0.0f, glm::vec3(0,0,01));
    auto matrix = glm::translate(glm::mat4(1.0f), position) * glm::toMat4(modelRotation) * glm::scale(glm::mat4(1.0f), scale);
    if(parent) {
        return parent->toTransformMatrix() * matrix;
    }
    return matrix;
}