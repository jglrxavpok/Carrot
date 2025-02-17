//
// Created by jglrxavpok on 31/12/2021.
//

#include "Transform.h"

#include <core/io/DocumentHelpers.h>
#include <engine/utils/conversions.h>
#include <glm/glm.hpp>
#include <core/utils/JSON.h>
#include <glm/gtx/matrix_decompose.hpp>

namespace Carrot::Math {
    glm::mat4 Transform::toTransformMatrix(const Transform *parent) const {
        auto modelRotation = glm::toMat4(rotation);
        auto matrix = glm::translate(glm::mat4(1.0f), position) * modelRotation * glm::scale(glm::mat4(1.0f), scale);
        if(parent) {
            return parent->toTransformMatrix() * matrix;
        }
        return matrix;
    }

    void Transform::deserialise(const Carrot::DocumentElement& doc) {
        position = DocumentHelpers::read<3, float>(doc["position"]);
        scale = DocumentHelpers::read<3, float>(doc["scale"]);
        auto rotVec = DocumentHelpers::read<4, float>(doc["rotation"]);
        rotation = glm::quat { rotVec.w, rotVec.x, rotVec.y, rotVec.z };
    }

    Carrot::DocumentElement Transform::serialise() const {
        Carrot::DocumentElement obj;
        obj["position"] = DocumentHelpers::write<3, float>(position);
        obj["scale"] = DocumentHelpers::write<3, float>(scale);
        glm::vec4 rotationVec { rotation.x, rotation.y, rotation.z, rotation.w };
        obj["rotation"] = DocumentHelpers::write<4, float>(rotationVec);
        return obj;
    }

    void Transform::fromMatrix(const glm::mat4& matrix) {
        glm::vec3 translation;
        glm::vec3 skew;
        glm::vec4 perspective;
        glm::decompose(matrix, scale, rotation, position, skew, perspective);
        this->rotation = glm::conjugate(rotation);
    }

    Transform Transform::operator*(const Transform& other) const {
        glm::mat4 finalTransform = toTransformMatrix() * other.toTransformMatrix();
        Transform result;
        result.fromMatrix(finalTransform);
        return result;
    }
}