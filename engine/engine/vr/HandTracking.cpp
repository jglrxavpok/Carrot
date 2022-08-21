//
// Created by jglrxavpok on 20/08/2022.
//

#include "HandTracking.h"
#include "VRInterface.h"
#include "engine/utils/conversions.h"

namespace Carrot::VR {

    HandTracking::HandTracking(Interface& interface, Session& session): interface(interface), session(session) {
        supported = interface.supportsHandTracking();

        xr::HandTrackerCreateInfoEXT createInfo;
        createInfo.handJointSet = xr::HandJointSetEXT::Default;

        createInfo.hand = xr::HandEXT::Left;
        leftHandTracker = session.createHandTracker(createInfo);

        createInfo.hand = xr::HandEXT::Right;
        rightHandTracker = session.createHandTracker(createInfo);
    }

    bool HandTracking::isSupported() const {
        return supported;
    }

    const Hand& HandTracking::getLeftHand() const {
        return leftHand;
    }

    const Hand& HandTracking::getRightHand() const {
        return rightHand;
    }

    void HandTracking::locateJoints(const xr::Space& space, const xr::Time& time) {
        if(!supported) {
            return;
        }
        xr::HandJointsLocateInfoEXT locateInfo;
        locateInfo.baseSpace = space;
        locateInfo.time = time;

        auto updateHand = [&](xr::HandTrackerEXT& tracker, Hand& hand) {
            xr::HandJointLocationsEXT joints;
            xr::HandJointVelocitiesEXT velocities;
            joints.jointCount = XR_HAND_JOINT_COUNT_EXT;
            velocities.jointCount = XR_HAND_JOINT_COUNT_EXT;

            xr::HandJointLocationEXT jointStorage[XR_HAND_JOINT_COUNT_EXT];
            xr::HandJointVelocityEXT velocityStorage[XR_HAND_JOINT_COUNT_EXT];

            joints.jointLocations = jointStorage;
            velocities.jointVelocities = velocityStorage;

            joints.next = &velocities;
//            xr::Result result = tracker.locateHandJointsEXT(locateInfo, joints, session.getXRDispatch());
            // OpenXR HPP actually resets the 'joints' struct before calling OpenXR (via its put() method)
            xr::Result result = static_cast<xr::Result>(session.getXRDispatch().xrLocateHandJointsEXT(tracker.get(), locateInfo.get(), joints.put(false /*don't clear*/)));
            verify(result == xr::Result::Success, "Failed to locate hand joints");

            hand.currentlyTracking = !!joints.isActive;
            if(!hand.currentlyTracking) {
                return;
            }
            for(HandJointID jointID = HandJointID::First; jointID != HandJointID::Count; jointID = static_cast<HandJointID>(static_cast<int>(jointID)+1)) {
                int index = static_cast<int>(jointID);
                HandJoint& joint = hand.joints[index];
                const xr::HandJointLocationEXT& xrJoint = joints.jointLocations[index];
                const xr::HandJointVelocityEXT& xrVelocity = velocities.jointVelocities[index];
                joint.isPoseValid = !!(xrJoint.locationFlags & xr::SpaceLocationFlagBits::PositionValid) && !!(xrJoint.locationFlags & xr::SpaceLocationFlagBits::OrientationValid);
                joint.isRadiusValid = !!(xrJoint.locationFlags & xr::SpaceLocationFlagBits::PositionValid);
                joint.isLinearVelocityValid = !!(xrVelocity.velocityFlags & xr::SpaceVelocityFlagBits::LinearValid);
                joint.isAngularVelocityValid = !!(xrVelocity.velocityFlags & xr::SpaceVelocityFlagBits::AngularValid);

                joint.angularVelocity = Carrot::toGlm(xrVelocity.angularVelocity);
                joint.linearVelocity = Carrot::toGlm(xrVelocity.linearVelocity);

                glm::mat4 rotation = glm::rotate(glm::identity<glm::mat4>(), -glm::pi<float>()/2.0f, glm::vec3(1.0f, 0.0f, 0.0f));
                joint.pose = rotation * Carrot::toGlm(xrJoint.pose);
                joint.radius = xrJoint.radius;
            }
        };

        updateHand(*leftHandTracker, leftHand);
        updateHand(*rightHandTracker, rightHand);
    }
} // Carrot::VR