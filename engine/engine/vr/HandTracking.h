//
// Created by jglrxavpok on 20/08/2022.
//

#pragma once

#include <core/utils/Lookup.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Carrot::VR {
    class Interface;
    class Session;

    enum class HandJointID {
        First = 0,

        Palm = First,
        Wrist = 1,
        ThumbMetacarpal = 2,
        ThumbProximal = 3,
        ThumbDistal = 4,
        ThumbTip = 5,
        IndexMetacarpal = 6,
        IndexProximal = 7,
        IndexIntermediate = 8,
        IndexDistal = 9,
        IndexTip = 10,
        MiddleMetacarpal = 11,
        MiddleProximal = 12,
        MiddleIntermediate = 13,
        MiddleDistal = 14,
        MiddleTip = 15,
        RingMetacarpal = 16,
        RingProximal = 17,
        RingIntermediate = 18,
        RingDistal = 19,
        RingTip = 20,
        LittleMetacarpal = 21,
        LittleProximal = 22,
        LittleIntermediate = 23,
        LittleDistal = 24,
        LittleTip = 25,

        Count,
    };

    static_assert(static_cast<int>(HandJointID::Count) == XR_HAND_JOINT_COUNT_EXT);

    //! Joint, represented in local space (see OpenXR documentation)
    struct HandJoint {
        bool isPoseValid = false;
        bool isRadiusValid = false;
        bool isLinearVelocityValid = false;
        bool isAngularVelocityValid = false;

        //! Position of this joint. Undefined if 'isPoseValid' is false
        glm::vec3 position{0.0f};

        //! Orientation of this joint. Undefined if 'isPoseValid' is false
        glm::quat orientation = glm::identity<glm::quat>();

        //! Size of this joint. Undefined if 'isRadiusValid' is false
        float radius = 0.0f;

        //! Linear velocity (in m/s) of this joint. Undefined if 'isLinearVelocityValid' is false
        glm::vec3 linearVelocity{0.0f};

        //! Angular velocity (in rad/s) of this joint. Undefined if 'isAngularVelocityValid' is false
        glm::vec3 angularVelocity{0.0f};
    };

    //! Holds the list of joint for a given hand
    struct Hand {
        HandJoint joints[static_cast<int>(HandJointID::Count)];
        bool currentlyTracking = false;
    };

    //! Interface to determine hand joint locations, if supported.
    //! If hand tracking is not supported by the current OpenXR session, isSupported() returns false, and the various
    //! methods will return default values (as if the hand is in a default pose)
    class HandTracking {
    public:
        //! Create a new HandTracking object. Should not be used by games, use VR::Session::getHandTracking()
        HandTracking(Interface& interface, Session& session);

        //! Returns true iif the current session supports hand tracking
        bool isSupported() const;

        const Hand& getLeftHand() const;
        const Hand& getRightHand() const;

    public:
        void locateJoints(const xr::Space& space, const xr::Time& time);

    private:
        VR::Interface& interface;
        VR::Session& session;

        bool supported = false;
        xr::UniqueDynamicHandTrackerEXT leftHandTracker;
        xr::UniqueDynamicHandTrackerEXT rightHandTracker;

        Hand leftHand;
        Hand rightHand;
    };
} // Carrot::VR
