//
// Created by jglrxavpok on 25/07/2021.
//
#include "ActionSet.h"
#include <core/Macros.h>
#include "engine/Engine.h"
#include "core/io/Logging.hpp"

namespace Carrot::IO {

    std::vector<ActionSet*>& ActionSet::getSetList() {
        static std::vector<ActionSet*> list;
        return list;
    }

    ActionSet::ActionSet(std::string_view name, bool isXRSet): name(name), isXRSet(isXRSet) {
        getSetList().push_back(this);

#ifndef ENABLE_VR
        verify(!isXRSet, "Cannot create XR set when ENABLE_VR is not defined.");
#endif
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

    void ActionSet::updatePrePoll(bool setPreviousValues) {
        if(!active)
            return;
        if(setPreviousValues) {
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
    }

    void ActionSet::updatePrePollAllSets(Carrot::Engine& engine, bool setPreviousValues) {
        for(auto* set : getSetList()) {
            if(!set->isActive())
                continue;
            if(!set->readyForUse) {
                set->prepareForUse(engine);
            }
            set->updatePrePoll(setPreviousValues);
        }

#ifdef ENABLE_VR
        TODO // xrSyncActions
#endif
    }

    void ActionSet::prepareForUse(Carrot::Engine& engine) {
        if(readyForUse)
            return;

        if(isXRSet) {
#ifdef ENABLE_VR
            TODO
            auto updateActions = [&](const auto& vector) {
                for(auto& a : vector) {
                    a->createXRAction();
                }
            };
            updateActions(boolInputs);
            updateActions(floatInputs);
            updateActions(vec2Inputs);
#endif
        } else {
            // TODO: actually allow remapping, for the moment only use suggested binding
            auto getMappedBindings = [](auto& action) {
                return action->getSuggestedBindings();
            };

            auto changeButtonInput = [this, getMappedBindings](std::string_view bindingPath, bool isPressed, bool isReleased) {
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

            auto changeAxisInput = [this, getMappedBindings](std::string_view bindingPath, float newValue) {
                for(auto& input : floatInputs) {
                    const auto& bindings = getMappedBindings(input);
                    if(std::find(WHOLE_CONTAINER(bindings), bindingPath) != bindings.end()) {
                        // TODO: handle case if multiple physical inputs are bound to this action
                        input->state.fValue = newValue;
                    }
                }
            };

            auto changeVec2Input = [this, getMappedBindings](std::string_view bindingPath, const glm::vec2& newValue) {
                for(auto& input : vec2Inputs) {
                    const auto& bindings = getMappedBindings(input);
                    if(std::find(WHOLE_CONTAINER(bindings), bindingPath) != bindings.end()) {
                        // TODO: handle case if multiple physical inputs are bound to this action
                        input->state.vValue = newValue;
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
                changeVec2Input(correspondingPath, newValue);
            });

            keysVec2Callback = GetEngine().addGLFWKeysVec2Callback([this, changeButtonInput, changeAxisInput, changeVec2Input](IO::GameInputVectorType vectorType, glm::vec2 newValue, glm::vec2 oldValue) {
                if(!active)
                    return;

                const auto correspondingPath = Carrot::IO::GLFWKeysVec2Binding(vectorType);

                bool isPressed = glm::length(newValue) >= 0.5f;
                bool isReleased = !isPressed;

                changeButtonInput(correspondingPath, isPressed, isReleased);
                changeAxisInput(correspondingPath, glm::length(newValue));
                changeVec2Input(correspondingPath, newValue);
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

                changeVec2Input(correspondingPath, {static_cast<float>(dx), static_cast<float>(dy)});
            });

            mouseDeltaCallback = GetEngine().addGLFWMouseDeltaCallback([this, changeVec2Input](double dx, double dy) {
                if(!active)
                    return;

                const auto correspondingPath = Carrot::IO::GLFWMouseDeltaBinding;

                changeVec2Input(correspondingPath, {static_cast<float>(dx), static_cast<float>(dy)});
            });

            mouseDeltaGrabbedCallback = GetEngine().addGLFWMouseDeltaGrabbedCallback([this, changeVec2Input](double dx, double dy) {
                if(!active)
                    return;

                const auto correspondingPath = Carrot::IO::GLFWGrabbedMouseDeltaBinding;

                changeVec2Input(correspondingPath, {static_cast<float>(dx), static_cast<float>(dy)});
            });

        }

        readyForUse = true;
    }
}