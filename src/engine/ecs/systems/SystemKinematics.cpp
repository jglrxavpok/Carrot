//
// Created by jglrxavpok on 06/03/2021.
//

#include "SystemKinematics.h"

void Carrot::SystemKinematics::tick(double dt) {
    forEachEntity([&](Entity_Ptr ent, Transform& transform, Kinematics& kinematics) {
        transform.position += kinematics.velocity * static_cast<float>(dt);
    });
}