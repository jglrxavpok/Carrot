//
// Created by jglrxavpok on 30/03/2025.
//

#include "PhysicsBody.h"

#include <engine/physics/PhysicsSystem.h>
#include <engine/utils/Macros.h>

namespace Carrot::Physics {
    BodyAccessWrite::BodyAccessWrite(const JPH::BodyID& bodyID) {
        if(!bodyID.IsInvalid()) {
            mutex = GetPhysics().lockWriteBody(bodyID);
            body = GetPhysics().lockedGetBody(bodyID);
            verify(body != nullptr, "Invalid bodyID");
        }
    }

    BodyAccessWrite::~BodyAccessWrite() {
        if(mutex) {
            GetPhysics().unlockWriteBody(mutex);
        }
    }

    JPH::Body* BodyAccessWrite::operator->() {
        return body;
    }

    BodyAccessWrite::operator bool() {
        return body != nullptr;
    }

    JPH::Body* BodyAccessWrite::get() {
        return body;
    }

    BodyAccessRead::BodyAccessRead(const JPH::BodyID& bodyID) {
        if(!bodyID.IsInvalid()) {
            mutex = GetPhysics().lockReadBody(bodyID);
            body = GetPhysics().lockedGetBody(bodyID);
            verify(body != nullptr, "Invalid bodyID");
        }
    }

    BodyAccessRead::~BodyAccessRead() {
        if(mutex) {
            GetPhysics().unlockReadBody(mutex);
        }
    }

    const JPH::Body* BodyAccessRead::operator->() {
        return body;
    }

    BodyAccessRead::operator bool() {
        return body != nullptr;
    }

    const JPH::Body* BodyAccessRead::get() {
        return body;
    }

    void BaseBody::onContactAdded(const JPH::BodyID& other) {
        Async::LockGuard g { contactAddedListsAccess };
        contactAddedCallbacks(other);
    }

    void BaseBody::addContactListener(const std::function<void(const JPH::BodyID&)>& listener, bool mainThreadOnly) {
        Async::LockGuard g { contactAddedListsAccess };
        if (mainThreadOnly) {
            Async::LockGuard g2 { contactAddedSinceLastPostPhysicsAccess };
            contactAddedCallbacksMainThread.append(listener);
        } else {
            contactAddedCallbacks.append(listener);
        }
    }

    void BaseBody::addPostPhysicsContactAddedEvent(JPH::BodyID otherBody) {
        if (contactAddedCallbacksMainThread.empty()) {
            return;
        }

        Async::LockGuard g { contactAddedSinceLastPostPhysicsAccess };
        contactAddedSinceLastPostPhysics.pushBack(otherBody);
    }

    void BaseBody::dispatchEventsPostPhysicsMainThread() {
        // FIXME: remove once scripting works in multithreading
        Async::LockGuard g { contactAddedSinceLastPostPhysicsAccess };
        for (auto& otherBody : contactAddedSinceLastPostPhysics) {
            contactAddedCallbacksMainThread(otherBody);
        }
        contactAddedSinceLastPostPhysics.clear();
    }

}
