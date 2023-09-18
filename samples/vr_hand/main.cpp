//
// Created by jglrxavpok on 18/08/2022.
//

#include <engine/Engine.h>
#include <engine/CarrotGame.h>
#include <engine/vr/Session.h>
#include <engine/vr/VRInterface.h>
#include <engine/vr/HandTracking.h>
#include <engine/render/Model.h>
#include <engine/render/animation/SkeletalModelRenderer.h>

class SampleVRHand: public Carrot::CarrotGame {
public:
    SampleVRHand(Carrot::Engine& engine): Carrot::CarrotGame(engine),
                                          //handModel(GetAssetServer().loadModel("resources/models/hand-for-vr2.fbx")),
                                          handModel(GetAssetServer().loadModel("resources/models/MRTK_HandCoach_FBX-Carrot.fbx")),
                                          //handModel(GetAssetServer().loadModel("resources/models/cube_tower.fbx")),
                                          rightHandRenderer(handModel)
    {
        verify(engine.getVRSession().getHandTracking().isSupported(), "This sample only works with hand-tracking-capable devices.");

        auto registerBone = [&](int handIndex, const std::string& name, const std::string& parentName, Carrot::VR::HandJointID id) {
            Carrot::Render::Bone* bone = rightHandRenderer.getSkeleton().findBone(name);
            Carrot::Render::Bone* parentBone = rightHandRenderer.getSkeleton().findBone(parentName);
            // TODO: parent bone
            if(bone) {
                bone2joint[name] = id;
                bones[name] = bone;
                hands[name] = handIndex;

                if(parentBone) {
                    parents[name] = bone2joint[parentName];
                    //parents[name] = parentBone;
                }
            }
        };

        /*registerBone("hand_R", "", Carrot::VR::HandJointID::Wrist);
        registerBone("thumb00_R", "hand_R", Carrot::VR::HandJointID::ThumbProximal);
        registerBone("thumb01_R", "thumb00_R", Carrot::VR::HandJointID::ThumbDistal);
        registerBone("thumb02_R", "thumb01_R", Carrot::VR::HandJointID::ThumbTip);

        registerBone("index00_R", "hand_R", Carrot::VR::HandJointID::IndexMetacarpal);
        registerBone("index01_R", "index00_R", Carrot::VR::HandJointID::IndexProximal);
        registerBone("index02_R", "index01_R", Carrot::VR::HandJointID::IndexIntermediate);
        registerBone("index03_R", "index02_R", Carrot::VR::HandJointID::IndexDistal);

        registerBone("middle00_R", "hand_R", Carrot::VR::HandJointID::MiddleMetacarpal);
        registerBone("middle01_R", "middle00_R", Carrot::VR::HandJointID::MiddleProximal);
        registerBone("middle02_R", "middle01_R", Carrot::VR::HandJointID::MiddleIntermediate);
        registerBone("middle03_R", "middle02_R", Carrot::VR::HandJointID::MiddleDistal);

        registerBone("ring00_R", "hand_R", Carrot::VR::HandJointID::RingMetacarpal);
        registerBone("ring01_R", "ring00_R", Carrot::VR::HandJointID::RingProximal);
        registerBone("ring02_R", "ring01_R", Carrot::VR::HandJointID::RingIntermediate);
        registerBone("ring03_R", "ring02_R", Carrot::VR::HandJointID::RingDistal);

        registerBone("pinky00_R", "hand_R", Carrot::VR::HandJointID::LittleMetacarpal);
        registerBone("pinky01_R", "pinky00_R", Carrot::VR::HandJointID::LittleProximal);
        registerBone("pinky02_R", "pinky01_R", Carrot::VR::HandJointID::LittleIntermediate);
        registerBone("pinky03_R", "pinky02_R", Carrot::VR::HandJointID::LittleDistal);*/

        registerBone(0, "WristL_JNT", "", Carrot::VR::HandJointID::Wrist);

        registerBone(0, "PointL_JNT", "WristL_JNT", Carrot::VR::HandJointID::IndexMetacarpal);
        registerBone(0, "PointL_JNT1", "PointL_JNT", Carrot::VR::HandJointID::IndexProximal);
        registerBone(0, "PointL_JNT2", "PointL_JNT1", Carrot::VR::HandJointID::IndexIntermediate);
        registerBone(0, "PointL_JNT3", "PointL_JNT2", Carrot::VR::HandJointID::IndexDistal);

        registerBone(0, "ThumbL_JNT", "WristL_JNT", Carrot::VR::HandJointID::ThumbMetacarpal);
        registerBone(0, "ThumbL_JNT1", "ThumbL_JNT", Carrot::VR::HandJointID::ThumbProximal);
        registerBone(0, "ThumbL_JNT2", "ThumbL_JNT1", Carrot::VR::HandJointID::ThumbDistal);


        registerBone(0, "MiddleL_JNT", "WristL_JNT", Carrot::VR::HandJointID::MiddleMetacarpal);
        registerBone(0, "MiddleL_JNT1", "MiddleL_JNT", Carrot::VR::HandJointID::MiddleProximal);
        registerBone(0, "MiddleL_JNT2", "MiddleL_JNT1", Carrot::VR::HandJointID::MiddleIntermediate);
        registerBone(0, "MiddleL_JNT3", "MiddleL_JNT2", Carrot::VR::HandJointID::MiddleDistal);

        registerBone(0, "RingL_JNT", "WristL_JNT", Carrot::VR::HandJointID::RingMetacarpal);
        registerBone(0, "RingL_JNT1", "RingL_JNT", Carrot::VR::HandJointID::RingProximal);
        registerBone(0, "RingL_JNT2", "RingL_JNT1", Carrot::VR::HandJointID::RingIntermediate);
        registerBone(0, "RingL_JNT3", "RingL_JNT2", Carrot::VR::HandJointID::RingDistal);

        registerBone(0, "PinkyL_JNT", "WristL_JNT", Carrot::VR::HandJointID::LittleMetacarpal);
        registerBone(0, "PinkyL_JNT1", "PinkyL_JNT", Carrot::VR::HandJointID::LittleProximal);
        registerBone(0, "PinkyL_JNT2", "PinkyL_JNT1", Carrot::VR::HandJointID::LittleIntermediate);
        registerBone(0, "PinkyL_JNT3", "PinkyL_JNT2", Carrot::VR::HandJointID::LittleDistal);

        registerBone(1, "WristR_JNT", "", Carrot::VR::HandJointID::Wrist);

        registerBone(1, "PointR_JNT", "WristR_JNT", Carrot::VR::HandJointID::IndexMetacarpal);
        registerBone(1, "PointR_JNT1", "PointR_JNT", Carrot::VR::HandJointID::IndexProximal);
        registerBone(1, "PointR_JNT2", "PointR_JNT1", Carrot::VR::HandJointID::IndexIntermediate);
        registerBone(1, "PointR_JNT3", "PointR_JNT2", Carrot::VR::HandJointID::IndexDistal);

        registerBone(1, "ThumbR_JNT", "WristR_JNT", Carrot::VR::HandJointID::ThumbMetacarpal);
        registerBone(1, "ThumbR_JNT1", "ThumbR_JNT", Carrot::VR::HandJointID::ThumbProximal);
        registerBone(1, "ThumbR_JNT2", "ThumbR_JNT1", Carrot::VR::HandJointID::ThumbDistal);


        registerBone(1, "MiddleR_JNT", "WristR_JNT", Carrot::VR::HandJointID::MiddleMetacarpal);
        registerBone(1, "MiddleR_JNT1", "MiddleR_JNT", Carrot::VR::HandJointID::MiddleProximal);
        registerBone(1, "MiddleR_JNT2", "MiddleR_JNT1", Carrot::VR::HandJointID::MiddleIntermediate);
        registerBone(1, "MiddleR_JNT3", "MiddleR_JNT2", Carrot::VR::HandJointID::MiddleDistal);

        registerBone(1, "RingR_JNT", "WristR_JNT", Carrot::VR::HandJointID::RingMetacarpal);
        registerBone(1, "RingR_JNT1", "RingR_JNT", Carrot::VR::HandJointID::RingProximal);
        registerBone(1, "RingR_JNT2", "RingR_JNT1", Carrot::VR::HandJointID::RingIntermediate);
        registerBone(1, "RingR_JNT3", "RingR_JNT2", Carrot::VR::HandJointID::RingDistal);

        registerBone(1, "PinkyR_JNT", "WristR_JNT", Carrot::VR::HandJointID::LittleMetacarpal);
        registerBone(1, "PinkyR_JNT1", "PinkyR_JNT", Carrot::VR::HandJointID::LittleProximal);
        registerBone(1, "PinkyR_JNT2", "PinkyR_JNT1", Carrot::VR::HandJointID::LittleIntermediate);
        registerBone(1, "PinkyR_JNT3", "PinkyR_JNT2", Carrot::VR::HandJointID::LittleDistal);
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        const Carrot::VR::HandTracking& handTracking = engine.getVRSession().getHandTracking();
        if(ImGui::Begin("Hand tracking debug")) {
            auto draw = [&](const Carrot::VR::Hand& hand) {
                bool tracking = hand.currentlyTracking;
                ImGui::Checkbox("Tracking", &tracking);

                auto& palmJoint = hand.joints[(int)Carrot::VR::HandJointID::Palm];
                ImGui::Text("Palm linear speed");
                ImGui::Text("%f m/s", palmJoint.linearVelocity.x);
                ImGui::Text("%f m/s", palmJoint.linearVelocity.y);
                ImGui::Text("%f m/s", palmJoint.linearVelocity.z);
            };

            if(ImGui::CollapsingHeader("Left hand")) {
                draw(handTracking.getLeftHand());
            }
            ImGui::Separator();
            if(ImGui::CollapsingHeader("Right hand")) {
                draw(handTracking.getRightHand());
            }
        }
        ImGui::End();


        auto& instanceData = rightHandRenderer.getInstanceData();
        float scale = 0.05f;

        glm::vec3 handTranslation = handTracking.getRightHand().joints[(int)Carrot::VR::HandJointID::Palm].position;
        std::swap(handTranslation.y, handTranslation.z);
        handTranslation.y *= -1.0f;

        glm::mat4 translation = glm::translate(glm::identity<glm::mat4>(), glm::vec3 { 0.0f, 1.0f, 0.0f } + handTranslation);
        glm::mat4 scaling = glm::scale(glm::identity<glm::mat4>(), glm::vec3 { scale, scale, scale});


        glm::mat4 modelCorrection = glm::rotate(glm::mat4(1.0f), glm::pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
        instanceData.transform = translation * scaling * modelCorrection;
        glm::mat4 correction =
                {
                        {0.0f, 0.0f, -1.0f, 0.0f },
                        {1.0f, 0.0f, 0.0f, 0.0f },
                        {0.0f, -1.0f, 0.0f, 0.0f },
                        {0.0f, 0.0f, 0.0f, 1.0f },
                };
        /*glm::mat4 correction =
                glm::mat4 {
            // +Y = -Z
                        {1.0f, 0.0f, 0.0f, 0.0f },
                        {0.0f, 1.0f, 0.0f, 0.0f },
                        {0.0f, 0.0f, 1.0f, 0.0f },
                        {0.0f, 0.0f, 0.0f, 1.0f },
                }
                *
                glm::mat4 {
                        {1.0f, 0.0f, 0.0f, 0.0f },
                        {0.0f, 1.0f, 0.0f, 0.0f },
                        {0.0f, 0.0f, 1.0f, 0.0f },
                        {0.0f, 0.0f, 0.0f, 1.0f },
                }
                ;*/
        glm::mat4 invCorrection = glm::inverse(correction);
        for(const auto& [boneName, joint] : bone2joint) {
            Carrot::Render::Bone* bone = bones[boneName];
            int handIndex = hands[boneName];
            glm::mat4 inverseParent = glm::identity<glm::mat4>();

            const Carrot::VR::Hand& hand = handIndex == 0 ? handTracking.getLeftHand() : handTracking.getRightHand();

            auto jointTransform = [&](Carrot::VR::HandJointID jointID) {
                return glm::mat4(hand.joints[(int)jointID].orientation) *
                        glm::translate(glm::mat4(1.0f), hand.joints[(int)jointID].position);
            };
            auto it = parents.find(boneName);
            if(it != parents.end()) {
                inverseParent = glm::inverse(jointTransform(it->second));
            }

            bone->transform = bone->originalTransform * correction * inverseParent * jointTransform(joint) * invCorrection;
        }

        rightHandRenderer.onFrame(renderContext);
    }

    void tick(double frameTime) override {

    }

private:
    Carrot::Model::Ref handModel;
    Carrot::Render::SkeletalModelRenderer rightHandRenderer;

    std::unordered_map<std::string, Carrot::VR::HandJointID> bone2joint;
    std::unordered_map<std::string, Carrot::Render::Bone*> bones;
    std::unordered_map<std::string, Carrot::VR::HandJointID> parents;
    std::unordered_map<std::string, int> hands; // 0: left, 1: right
};

void Carrot::Engine::initGame() {
    game = std::make_unique<SampleVRHand>(*this);
}

int main() {
    Carrot::Configuration config;
    config.applicationName = "VR Hand Sample";
    config.raytracingSupport = Carrot::RaytracingSupport::NotSupported;
    config.runInVR = true;

    Carrot::Engine engine{config};
    engine.run();
    return 0;
}