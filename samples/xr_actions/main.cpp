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
#include "engine/io/actions/ActionSet.h"
#include "engine/io/actions/Action.hpp"

class SampleXRActions: public Carrot::CarrotGame {
public:
    SampleXRActions(Carrot::Engine& engine): Carrot::CarrotGame(engine)
    {
        selectClick.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::SimpleController, "/user/hand/right/input/select/click"));
        menuClick.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::SimpleController, "/user/hand/right/input/menu/click"));
        menuClick.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::IndexController, "/user/hand/right/input/trigger/value"));
        feedback.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::IndexController, "/user/hand/right/output/haptic"));

        aButtonTouch.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::IndexController, "/user/hand/right/input/a/touch"));
        aButtonClick.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::IndexController, "/user/hand/right/input/a/click"));

        trackpadX.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::IndexController, "/user/hand/right/input/trackpad/x"));
        trackpadY.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::IndexController, "/user/hand/right/input/trackpad/y"));

        rightHandPose.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::SimpleController, "/user/hand/right/input/aim/pose"));
        rightHandPose.suggestBinding(Carrot::IO::OpenXRBinding(Carrot::IO::IndexController, "/user/hand/right/input/aim/pose"));

        // mix actions for each set (to make sure you can mix input profiles properly)
        set1.add(selectClick);
        set1.add(aButtonTouch);
        set1.add(trackpadX);
        set1.add(feedback);

        set2.add(menuClick);
        set2.add(aButtonClick);
        set2.add(trackpadY);
        set2.add(rightHandPose);

        set1.activate();
        set2.activate();
    }

    void onFrame(Carrot::Render::Context renderContext) override {
        if(ImGui::Begin("Inputs")) {
            bool selected = false;

            bool isSetActive = false;

            isSetActive = set1.isActive();
            if(ImGui::Checkbox("Set 1", &isSetActive)) {
                if(!isSetActive) {
                    set1.deactivate();
                } else {
                    set1.activate();
                }
            }
            selected = selectClick.isPressed();
            ImGui::Checkbox(selectClick.getName().c_str(), &selected);

            selected = aButtonTouch.isPressed();
            ImGui::Checkbox(aButtonTouch.getName().c_str(), &selected);

            ImGui::Text("%s : %f", trackpadX.getName().c_str(), trackpadX.getValue());

            ImGui::Separator();

            isSetActive = set2.isActive();
            if(ImGui::Checkbox("Set 2", &isSetActive)) {
                if(!isSetActive) {
                    set2.deactivate();
                } else {
                    set2.activate();
                }
            }
            selected = menuClick.isPressed();
            ImGui::Checkbox(menuClick.getName().c_str(), &selected);

            selected = aButtonClick.isPressed();
            ImGui::Checkbox(aButtonClick.getName().c_str(), &selected);

            ImGui::Text("%s : %f", trackpadY.getName().c_str(), trackpadY.getValue());

            ImGui::Text("Right Hand position: %f ; %f ; %f", rightHandPose.getValue().position.x, rightHandPose.getValue().position.y, rightHandPose.getValue().position.z);
            ImGui::Text("Right Hand orientation: %f ; %f ; %f ; %f", rightHandPose.getValue().orientation.x, rightHandPose.getValue().orientation.y, rightHandPose.getValue().orientation.z, rightHandPose.getValue().orientation.w);
        }
        ImGui::End();
    }

    void tick(double frameTime) override {
        static bool wasPressed = false;
        if(aButtonClick.isPressed() && !wasPressed) {
            feedback.vibrate(1000000000, 10, 0.5f);
        }
        wasPressed = aButtonClick.isPressed();
    }

private:
    Carrot::IO::ActionSet set1 { "test_set_1" };
    Carrot::IO::ActionSet set2 { "test_set_2" };

    Carrot::IO::BoolInputAction selectClick { "select_click" };
    Carrot::IO::BoolInputAction menuClick { "menu_click" };

    Carrot::IO::BoolInputAction aButtonTouch { "touch_button_a_valve_index" };
    Carrot::IO::BoolInputAction aButtonClick { "click_button_a_valve_index" };

    Carrot::IO::FloatInputAction trackpadX { "trackpad_x_valve_index" };
    Carrot::IO::FloatInputAction trackpadY { "trackpad_y_valve_index" };
    Carrot::IO::PoseInputAction rightHandPose { "right_hand" };
    Carrot::IO::VibrationOutputAction feedback { "feedback" };
};

void Carrot::Engine::initGame() {
    game = std::make_unique<SampleXRActions>(*this);
}

int main() {
    Carrot::Configuration config;
    config.applicationName = "OpenXR Actions Sample";
    config.raytracingSupport = Carrot::RaytracingSupport::NotSupported;
    config.runInVR = true;

    Carrot::Engine engine{config};
    engine.run();
    return 0;
}