//
// Created by jglrxavpok on 30/03/2025.
//

#pragma once

#include <functional>
#include <core/async/Locks.h>
#include <core/containers/Vector.hpp>
#include <eventpp/callbacklist.h>

#include <Jolt/Physics/Body/BodyID.h>

namespace JPH {
    class Body;
    class SharedMutex;
}

namespace Carrot::Physics {
    struct BodyAccessWrite {
        BodyAccessWrite(const JPH::BodyID& bodyID);
        ~BodyAccessWrite();

        JPH::Body* operator->();
        operator bool();

        JPH::Body* get();

    private:
        JPH::SharedMutex* mutex = nullptr;
        JPH::Body* body = nullptr;
    };

    struct BodyAccessRead {
        BodyAccessRead(const JPH::BodyID& bodyID);
        ~BodyAccessRead();

        const JPH::Body* operator->();
        operator bool();

        const JPH::Body* get();

    private:
        JPH::SharedMutex* mutex = nullptr;
        JPH::Body* body = nullptr;
    };

    class BaseBody {
    public:
        /// Adds a contact listener to this rigidbody.
        /// If 'mainThreadOnly' is true, the callback will be called from postPhysics, on the Main thread.
        /// If 'mainThreadOnly' is false, the callback will be called on the physics thread detecting the contact
        void addContactListener(const std::function<void(const JPH::BodyID&)>& listener, bool mainThreadOnly);

        /// Registers a ContactAdded event for dispatch in the next dispatchEventsPostPhysicsMainThread
        void addPostPhysicsContactAddedEvent(JPH::BodyID otherBody);

        /// Calls contact listeners registered on main thread, for each contact added to this rigidbody (therefore may call no callbacks at all)
        void dispatchEventsPostPhysicsMainThread();

    public: // called by physics code
        void onContactAdded(const JPH::BodyID& other);

        virtual ~BaseBody() {};

    private:
        Async::SpinLock contactAddedListsAccess;
        Async::SpinLock contactAddedSinceLastPostPhysicsAccess;
        eventpp::CallbackList<void(JPH::BodyID)> contactAddedCallbacks;
        eventpp::CallbackList<void(JPH::BodyID)> contactAddedCallbacksMainThread;
        Carrot::Vector<JPH::BodyID> contactAddedSinceLastPostPhysics;

    };
}
