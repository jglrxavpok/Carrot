//
// Created by jglrxavpok on 31/12/2021.
//

#include "Transform.h"
#include <engine/utils/conversions.h>
#include <glm/glm.hpp>
#include <core/utils/JSON.h>
#include <glm/gtx/matrix_decompose.hpp>

namespace Carrot::Math {
    Transform::Transform(const reactphysics3d::Transform& physics) {
        *this = physics;
    }

    Transform& Transform::operator=(const reactphysics3d::Transform& physics) {
        position = glmVecFromReactPhysics(physics.getPosition());
        rotation = glmQuatFromReactPhysics(physics.getOrientation());
        return *this;
    }

    Transform::operator reactphysics3d::Transform() const {
        reactphysics3d::Transform result;
        result.setPosition(reactPhysicsVecFromGlm(position));
        result.setOrientation(reactPhysicsQuatFromGlm(rotation));
        return result;
    }

    glm::mat4 Transform::toTransformMatrix(const Transform *parent) const {
        auto modelRotation = glm::toMat4(rotation);
        auto matrix = glm::translate(glm::mat4(1.0f), position) * modelRotation * glm::scale(glm::mat4(1.0f), scale);
        if(parent) {
            return parent->toTransformMatrix() * matrix;
        }
        return matrix;
    }

    void Transform::loadJSON(const rapidjson::Value& json) {
        position = JSON::read<3, float>(json["position"]);
        scale = JSON::read<3, float>(json["scale"]);
        auto rotVec = JSON::read<4, float>(json["rotation"]);
        rotation = glm::quat { rotVec.w, rotVec.x, rotVec.y, rotVec.z };
    }

    rapidjson::Value Transform::toJSON(rapidjson::Document::AllocatorType& allocator) const {
        rapidjson::Value obj(rapidjson::kObjectType);
        obj.AddMember("position", JSON::write<3, float>(position, allocator), allocator);
        obj.AddMember("scale", JSON::write<3, float>(scale, allocator), allocator);
        glm::vec4 rotationVec { rotation.x, rotation.y, rotation.z, rotation.w };
        obj.AddMember("rotation", JSON::write<4, float>(rotationVec, allocator), allocator);
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