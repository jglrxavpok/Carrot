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
                                          handModel(engine.getRenderer().getOrCreateModel("resources/models/hand-for-vr.fbx")),
                                          rightHandRenderer(handModel)
    {
        verify(engine.getVRSession().getHandTracking().isSupported(), "This sample only works with hand-tracking-capable devices.");

        auto registerBone = [&](const std::string& name, const std::string& parentName, Carrot::VR::HandJointID id) {
            Carrot::Render::Bone* bone = rightHandRenderer.getSkeleton().findBone(name);
            Carrot::Render::Bone* parentBone = rightHandRenderer.getSkeleton().findBone(parentName);
            // TODO: parent bone
            if(bone) {
                bone2joint[name] = id;
                bones[name] = bone;

                if(parentBone) {
                    parents[name] = bone2joint[parentName];
                }
            }
        };

        registerBone("hand_R", "", Carrot::VR::HandJointID::Palm);
        registerBone("thumb01_R", "hand_R", Carrot::VR::HandJointID::ThumbProximal);
        registerBone("thumb02_R", "thumb01_R", Carrot::VR::HandJointID::ThumbDistal);
        registerBone("thumb03_R", "thumb02_R", Carrot::VR::HandJointID::ThumbTip);

        registerBone("index00_R", "hand_R", Carrot::VR::HandJointID::IndexProximal);
        registerBone("index01_R", "index00_R", Carrot::VR::HandJointID::IndexIntermediate);
        registerBone("index02_R", "index01_R", Carrot::VR::HandJointID::IndexDistal);
        registerBone("index03_R", "index02_R", Carrot::VR::HandJointID::IndexTip);

        registerBone("middle00_R", "hand_R", Carrot::VR::HandJointID::MiddleProximal);
        registerBone("middle01_R", "middle00_R", Carrot::VR::HandJointID::MiddleIntermediate);
        registerBone("middle02_R", "middle01_R", Carrot::VR::HandJointID::MiddleDistal);
        registerBone("middle03_R", "middle02_R", Carrot::VR::HandJointID::MiddleTip);

        registerBone("ring00_R", "hand_R", Carrot::VR::HandJointID::RingProximal);
        registerBone("ring01_R", "ring00_R", Carrot::VR::HandJointID::RingIntermediate);
        registerBone("ring02_R", "ring01_R", Carrot::VR::HandJointID::RingDistal);
        registerBone("ring03_R", "ring02_R", Carrot::VR::HandJointID::RingTip);

        registerBone("pinky00_R", "hand_R", Carrot::VR::HandJointID::LittleProximal);
        registerBone("pinky01_R", "pinky00_R", Carrot::VR::HandJointID::LittleIntermediate);
        registerBone("pinky02_R", "pinky01_R", Carrot::VR::HandJointID::LittleDistal);
        registerBone("pinky03_R", "pinky02_R", Carrot::VR::HandJointID::LittleTip);
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
        glm::mat4 translation = glm::translate(glm::identity<glm::mat4>(), glm::vec3 { 0.0f, 1.0f, 0.0f });
        glm::mat4 scaling = glm::scale(glm::identity<glm::mat4>(), glm::vec3 { scale, scale, scale});
        instanceData.transform = translation * scaling;

        glm::mat4 correction = glm::rotate(glm::identity<glm::mat4>(), glm::pi<float>()/2.0f, glm::vec3(0.0f, 1.0f, 0.0f));
        for(const auto& [boneName, joint] : bone2joint) {
            Carrot::Render::Bone* bone = bones[boneName];
            glm::mat4 inverseParent = glm::identity<glm::mat4>();

            auto it = parents.find(boneName);
            if(it != parents.end()) {
                inverseParent = glm::inverse(correction * handTracking.getRightHand().joints[(int)it->second].pose);
            }

            bone->transform = inverseParent * correction * handTracking.getRightHand().joints[(int)joint].pose;
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