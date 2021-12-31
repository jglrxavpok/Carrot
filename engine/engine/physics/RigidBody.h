//
// Created by jglrxavpok on 31/12/2021.
//

#pragma once

#include <reactphysics3d/reactphysics3d.h>
#include <engine/math/Transform.h>

namespace Carrot::Physics {
    class RigidBody {
    public:
        explicit RigidBody();

        RigidBody(const RigidBody& toCopy);
        RigidBody(RigidBody&& toMove);

        RigidBody& operator=(const RigidBody& toCopy);
        RigidBody& operator=(RigidBody&& toMove);

        ~RigidBody();

    public:
        Carrot::Math::Transform getTransform() const;
        /// Forces the given transform. Only use for static bodies, kinematic and dynamic bodies may have trouble adapting to the new transform
        void setTransform(const Carrot::Math::Transform& transform);

    private:
        rp3d::RigidBody* body = nullptr;
    };
}
