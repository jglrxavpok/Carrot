//
// Created by jglrxavpok on 23/05/2026.
//

#include <atomic>
#include <engine/render/resources/UIDObject.h>

static std::atomic<u64> counter;

namespace Carrot::Render {
    UIDObject::UIDObject() {
        this->uid = counter++;
    }

    u64 UIDObject::getUID() const {
        return uid;
    }
}