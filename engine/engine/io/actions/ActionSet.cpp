//
// Created by jglrxavpok on 25/07/2021.
//
#include "ActionSet.h"
#include <core/Macros.h>
#include "engine/Engine.h"
#include "engine/vr/Session.h"
#include "core/io/Logging.hpp"
#include "engine/utils/conversions.h"


namespace Carrot::IO {

    std::vector<ActionSet*>& ActionSet::getSetList() {
        static std::vector<ActionSet*> list;
        return list;
    }

    ActionSet::ActionSet(std::string_view name): name(name) {
        getSetList().push_back(this);

        if(GetConfiguration().runInVR) {
            xrActionSet = GetEngine().getVRSession().createActionSet(name);
        }
    }

    ActionSet::~ActionSet() {
        deactivate();

        GetEngine().removeGLFWKeyCallback(keyCallback);
        GetEngine().removeGLFWMouseButtonCallback(mouseButtonCallback);
        GetEngine().removeGLFWGamepadButtonCallback(gamepadButtonCallback);
        GetEngine().removeGLFWGamepadAxisCallback(gamepadAxisCallback);
        GetEngine().removeGLFWGamepadVec2Callback(gamepadVec2Callback);
        GetEngine().removeGLFWKeysVec2Callback(keysVec2Callback);
        GetEngine().removeGLFWMousePositionCallback(mousePositionCallback);
        GetEngine().removeGLFWMouseDeltaCallback(mouseDeltaCallback);
        GetEngine().removeGLFWMouseDeltaGrabbedCallback(mouseDeltaGrabbedCallback);
        GetEngine().removeGLFWMouseWheelCallback(mouseWheelCallback);

        getSetList().erase(std::remove(WHOLE_CONTAINER(getSetList()), this), getSetList().end());
    }

    void ActionSet::activate() {
        active = true;
    }

    void ActionSet::deactivate() {
        active = false;
        for(auto& input : boolInputs) {
            input->state.bPreviousValue = input->state.bValue = false;
        }
        for(auto& input : floatInputs) {
            input->state.fPreviousValue = input->state.fValue = 0.0f;
        }
        for(auto& input : vec2Inputs) {
            input->state.vPreviousValue = input->state.vValue = {0.0f, 0.0f};
        }
    }

    void ActionSet::add(BoolInputAction& input) {
        boolInputs.push_back(&input);
    }

    void ActionSet::add(FloatInputAction& input) {
        floatInputs.push_back(&input);
    }

    void ActionSet::add(Vec2InputAction& input) {
        vec2Inputs.push_back(&input);
    }

    void ActionSet::add(PoseInputAction& input) {
        poseInputs.push_back(&input);
    }

    void ActionSet::add(VibrationOutputAction& input) {
        vibrationOutputs.push_back(&input);
    }

    xr::ActionSet& ActionSet::getXRActionSet() {
        verify(xrActionSet, "Cannot call ActionSet::getXRActionSet if not running in VR");
        return *xrActionSet;
    }

    void ActionSet::updatePrePoll() {
        if(!active)
            return;
        for(auto& input : boolInputs) {
            input->state.bPreviousValue = input->state.bValue;
        }
        for(auto& input : floatInputs) {
            input->state.fPreviousValue = input->state.fValue;
        }
        for(auto& input : vec2Inputs) {
            input->state.vPreviousValue = input->state.vValue;
        }
    }

    void ActionSet::syncXRActions() {
        std::vector<ActionSet*> setsToSync;
        setsToSync.reserve(getSetList().size());

        for(auto* set : getSetList()) {
            if(!set->isActive())
                continue;
            if(!set->readyForUse) {
                set->prepareForUse(GetEngine());
            }
            set->updatePrePoll();

            setsToSync.emplace_back(set);
        }

        GetEngine().getVRSession().syncActionSets(setsToSync);

        for(auto* set : setsToSync) {
            set->pollXRActions();
        }
    }

    void ActionSet::updatePrePollAllSets(Carrot::Engine& engine) {
        for(auto* set : getSetList()) {
            if(!set->isActive())
                continue;
            if(!set->readyForUse) {
                set->prepareForUse(engine);
            }
            set->updatePrePoll();
        }
    }

    void ActionSet::prepareForUse(Carrot::Engine& engine) {
        if(readyForUse)
            return;

        auto changeButtonInput = [this](const ActionBinding& bindingPath, bool isPressed, bool isReleased) {
            for(auto& input : boolInputs) {
                const auto& bindings = getMappedBindings(input);
                if(std::find(WHOLE_CONTAINER(bindings), bindingPath) != bindings.end()) {
                    if(isReleased) {
                        input->state.bValue = false;
                    } else {
                        input->state.bValue |= isPressed;
                    }
                }
            }
        };

        auto changeAxisInput = [this](const ActionBinding& bindingPath, float newValue) {
            for(auto& input : floatInputs) {
                const auto& bindings = getMappedBindings(input);
                if(std::find(WHOLE_CONTAINER(bindings), bindingPath) != bindings.end()) {
                    // TODO: handle case if multiple physical inputs are bound to this action
                    input->state.fValue = newValue;
                }
            }
        };

        auto changeVec2Input = [this](const ActionBinding& bindingPath, const glm::vec2& newValue, bool additive) {
            for(auto& input : vec2Inputs) {
                const auto& bindings = getMappedBindings(input);
                if(std::find(WHOLE_CONTAINER(bindings), bindingPath) != bindings.end()) {
                    // TODO: handle case if multiple physical inputs are bound to this action
                    if(additive) {
                        input->state.vValue += newValue;
                    } else {
                        input->state.vValue = newValue;
                    }
                }
            }
        };

        keyCallback = GetEngine().addGLFWKeyCallback([this, changeButtonInput, changeAxisInput](int key, int scancode, int action, int mods) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWKeyBinding(key);

            bool isPressed = false;
            if(action == GLFW_PRESS || action == GLFW_REPEAT) {
                isPressed = true;
            }

            bool isReleased = false;
            if(action == GLFW_RELEASE) {
                isPressed = false;
                isReleased = true;
            }

            changeButtonInput(correspondingPath, isPressed, isReleased);
            changeAxisInput(correspondingPath, isPressed ? 1.0f : 0.0f);
        });

        gamepadButtonCallback = GetEngine().addGLFWGamepadButtonCallback([this, changeButtonInput, changeAxisInput](int gamepadID, int buttonID, bool pressed) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWGamepadButtonBinding(gamepadID, buttonID);

            bool isPressed = pressed;
            bool isReleased = !pressed;

            changeButtonInput(correspondingPath, isPressed, isReleased);
            changeAxisInput(correspondingPath, pressed ? 1.0f : 0.0f);
        });

        gamepadAxisCallback = GetEngine().addGLFWGamepadAxisCallback([this, changeButtonInput, changeAxisInput](int gamepadID, int axisID, float newValue, float oldValue) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWGamepadAxisBinding(gamepadID, axisID);

            bool isPressed = newValue >= 0.5f;
            bool isReleased = !isPressed;

            changeButtonInput(correspondingPath, isPressed, isReleased);
            changeAxisInput(correspondingPath, newValue);
        });

        gamepadVec2Callback = GetEngine().addGLFWGamepadVec2Callback([this, changeButtonInput, changeAxisInput, changeVec2Input](int gamepadID, IO::GameInputVectorType vectorType, glm::vec2 newValue, glm::vec2 oldValue) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWGamepadVec2Binding(gamepadID, vectorType);

            bool isPressed = glm::length(newValue) >= 0.5f;
            bool isReleased = !isPressed;

            changeButtonInput(correspondingPath, isPressed, isReleased);
            changeAxisInput(correspondingPath, glm::length(newValue));
            changeVec2Input(correspondingPath, newValue, false);
        });

        keysVec2Callback = GetEngine().addGLFWKeysVec2Callback([this, changeButtonInput, changeAxisInput, changeVec2Input](IO::GameInputVectorType vectorType, glm::vec2 newValue, glm::vec2 oldValue) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWKeysVec2Binding(vectorType);

            bool isPressed = glm::length(newValue) >= 0.5f;
            bool isReleased = !isPressed;

            changeButtonInput(correspondingPath, isPressed, isReleased);
            changeAxisInput(correspondingPath, glm::length(newValue));
            changeVec2Input(correspondingPath, newValue, false);
        });

        mouseButtonCallback = GetEngine().addGLFWMouseButtonCallback([this, changeButtonInput, changeAxisInput](int buttonID, bool pressed, int mods) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWMouseButtonBinding(buttonID);

            bool isPressed = pressed;
            bool isReleased = !pressed;

            changeButtonInput(correspondingPath, isPressed, isReleased);
            changeAxisInput(correspondingPath, isPressed ? 1.0f : 0.0f);
        });

        mousePositionCallback = GetEngine().addGLFWMousePositionCallback([this, changeVec2Input](double dx, double dy) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWMousePositionBinding;

            changeVec2Input(correspondingPath, {static_cast<float>(dx), static_cast<float>(dy)}, false);
        });

        mouseDeltaCallback = GetEngine().addGLFWMouseDeltaCallback([this, changeVec2Input](double dx, double dy) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWMouseDeltaBinding;

            changeVec2Input(correspondingPath, {static_cast<float>(dx), static_cast<float>(dy)}, true);
        });

        mouseDeltaGrabbedCallback = GetEngine().addGLFWMouseDeltaGrabbedCallback([this, changeVec2Input](double dx, double dy) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWGrabbedMouseDeltaBinding;

            changeVec2Input(correspondingPath, {static_cast<float>(dx), static_cast<float>(dy)}, true);
        });

        mouseWheelCallback = GetEngine().addGLFWMouseWheelCallback([this, changeButtonInput, changeAxisInput](double dWheel) {
            if(!active)
                return;

            const auto correspondingPath = Carrot::IO::GLFWMouseWheel;
            bool isPressed = abs(dWheel) >= 0.5f;
            bool isReleased = !isPressed;

            changeButtonInput(correspondingPath, isPressed, isReleased);
            changeAxisInput(correspondingPath, dWheel);
        });

        readyForUse = true;
    }

    void ActionSet::pollXRActions() {
        Carrot:VR::Session& vrSession = GetEngine().getVRSession();
        for(auto& boolInput : boolInputs) {
            xr::ActionStateBoolean actionState = vrSession.getActionStateBoolean(*boolInput);
            if(actionState.isActive) {
                boolInput->state.bValue = (bool) actionState.currentState;
            }
        }

        for(auto& floatInput : floatInputs) {
            xr::ActionStateFloat actionState = vrSession.getActionStateFloat(*floatInput);
            if(actionState.isActive) {
                floatInput->state.fValue = actionState.currentState;
            }
        }

        for(auto& vec2Input : vec2Inputs) {
            xr::ActionStateVector2f actionState = vrSession.getActionStateVector2f(*vec2Input);
            if(actionState.isActive) {
                vec2Input->state.vValue = glm::vec2(actionState.currentState.x, actionState.currentState.y);
            }
        }

        for(auto& poseInput : poseInputs) {
            xr::ActionStatePose actionState = vrSession.getActionStatePose(*poseInput);
            if(actionState.isActive) {
                const xr::SpaceLocation location = vrSession.locateSpace(*poseInput->xrSpace);
                poseInput->poseState.pValue.position = glm::rotateX(Carrot::toGlm(location.pose.position), glm::half_pi<float>());

                const glm::quat correction = glm::rotate(glm::identity<glm::quat>(), -glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
                const glm::quat invCorrection = glm::rotate(glm::identity<glm::quat>(), glm::half_pi<float>(), glm::vec3(1.0f, 0.0f, 0.0f));
                poseInput->poseState.pValue.orientation = invCorrection * Carrot::toGlm(location.pose.orientation) * correction;
            }
        }
    }

    void ActionSet::resetAllDeltas() {
        for(auto* set : getSetList()) {
            set->resetDeltas();
        }
    }

    void ActionSet::resetDeltas() {
        for(auto bindingPath : {Carrot::IO::GLFWGrabbedMouseDeltaBinding, Carrot::IO::GLFWMouseDeltaBinding}) {
            for(auto& input : vec2Inputs) {
                const auto& bindings = getMappedBindings(input);
                if (std::find(WHOLE_CONTAINER(bindings), bindingPath) != bindings.end()) {
                    input->state.vValue = {};
                }
            }
        }
    }
}