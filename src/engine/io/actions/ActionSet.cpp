//
// Created by jglrxavpok on 25/07/2021.
//
#include "ActionSet.h"
#include "engine/utils/Macros.h"
#include "engine/Engine.h"
#include "engine/io/Logging.hpp"

namespace Carrot::IO {

    std::vector<ActionSet*>& ActionSet::getSetList() {
        static std::vector<ActionSet*> list;
        return list;
    }

    ActionSet::ActionSet(std::string_view name, bool isXRSet): name(name), isXRSet(isXRSet) {
        getSetList().push_back(this);

#ifndef ENABLE_VR
        runtimeAssert(!isXRSet, "Cannot create XR set when ENABLE_VR is not defined.");
#endif
    }

    ActionSet::~ActionSet() {
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
            // TODO: handle case where this action set is destroyed
            engine.addGLFWKeyCallback([this](int key, int scancode, int action, int mods) {
                const auto correspondingPath = Carrot::IO::GLFWKeyBinding(key);

                // TODO: actually allow remapping, for the moment only use suggested binding
                auto getMappedBinding = [&](auto& action) {
                    return action->getSuggestedBinding();
                };

                bool isPressed = false;
                if(action == GLFW_PRESS || action == GLFW_REPEAT) {
                    isPressed = true;
                }

                bool isReleased = false;
                if(action == GLFW_RELEASE) {
                    isPressed = false;
                    isReleased = true;
                }

                for(auto& input : boolInputs) {
                    std::string binding = getMappedBinding(input);
                    if(binding == correspondingPath) {
                        if(isReleased) {
                            input->state.bValue = false;
                        } else {
                            input->state.bValue |= isPressed;
                        }
                    }
                }
                for(auto& input : floatInputs) {
                    std::string binding = getMappedBinding(input);
                    if(binding == correspondingPath) {
                        // TODO: handle case if multiple physical inputs are bound to this action
                        if(isReleased) {
                            input->state.fValue = 0.0f;
                        } else {
                            input->state.fValue = isPressed ? 1.0f : 0.0f;
                        }
                    }
                }
            });
        }

        readyForUse = true;
    }
}