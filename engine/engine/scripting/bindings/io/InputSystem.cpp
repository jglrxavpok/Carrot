//
// Created by jglrxavpok on 26/05/2022.
//

#include "InputSystem.h"
#include "engine/io/actions/ActionSet.h"
#include "engine/io/actions/Action.hpp"
#include <core/utils/Lookup.hpp>

namespace Carrot::IO {
    using GLFWKeyType = std::int32_t;

    static Lookup keyTable = std::array {
            /* The unknown key */
            LookupEntry<GLFWKeyType>(GLFW_KEY_UNKNOWN, "GLFW_KEY_UNKNOWN"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_SPACE, "GLFW_KEY_SPACE"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_APOSTROPHE, "GLFW_KEY_APOSTROPHE"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_COMMA, "GLFW_KEY_COMMA"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_MINUS, "GLFW_KEY_MINUS"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_PERIOD, "GLFW_KEY_PERIOD"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_SLASH, "GLFW_KEY_SLASH"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_0, "GLFW_KEY_0"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_1, "GLFW_KEY_1"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_2, "GLFW_KEY_2"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_3, "GLFW_KEY_3"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_4, "GLFW_KEY_4"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_5, "GLFW_KEY_5"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_6, "GLFW_KEY_6"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_7, "GLFW_KEY_7"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_8, "GLFW_KEY_8"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_9, "GLFW_KEY_9"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_SEMICOLON, "GLFW_KEY_SEMICOLON"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_EQUAL, "GLFW_KEY_EQUAL"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_A, "GLFW_KEY_A"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_B, "GLFW_KEY_B"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_C, "GLFW_KEY_C"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_D, "GLFW_KEY_D"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_E, "GLFW_KEY_E"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F, "GLFW_KEY_F"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_G, "GLFW_KEY_G"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_H, "GLFW_KEY_H"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_I, "GLFW_KEY_I"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_J, "GLFW_KEY_J"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_K, "GLFW_KEY_K"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_L, "GLFW_KEY_L"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_M, "GLFW_KEY_M"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_N, "GLFW_KEY_N"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_O, "GLFW_KEY_O"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_P, "GLFW_KEY_P"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_Q, "GLFW_KEY_Q"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_R, "GLFW_KEY_R"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_S, "GLFW_KEY_S"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_T, "GLFW_KEY_T"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_U, "GLFW_KEY_U"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_V, "GLFW_KEY_V"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_W, "GLFW_KEY_W"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_X, "GLFW_KEY_X"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_Y, "GLFW_KEY_Y"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_Z, "GLFW_KEY_Z"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_LEFT_BRACKET, "GLFW_KEY_LEFT_BRACKET"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_BACKSLASH, "GLFW_KEY_BACKSLASH"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_RIGHT_BRACKET, "GLFW_KEY_RIGHT_BRACKET"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_GRAVE_ACCENT, "GLFW_KEY_GRAVE_ACCENT"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_WORLD_1, "GLFW_KEY_WORLD_1"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_WORLD_2, "GLFW_KEY_WORLD_2"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_ESCAPE, "GLFW_KEY_ESCAPE"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_ENTER, "GLFW_KEY_ENTER"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_TAB, "GLFW_KEY_TAB"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_BACKSPACE, "GLFW_KEY_BACKSPACE"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_INSERT, "GLFW_KEY_INSERT"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_DELETE, "GLFW_KEY_DELETE"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_RIGHT, "GLFW_KEY_RIGHT"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_LEFT, "GLFW_KEY_LEFT"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_DOWN, "GLFW_KEY_DOWN"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_UP, "GLFW_KEY_UP"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_PAGE_UP, "GLFW_KEY_PAGE_UP"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_PAGE_DOWN, "GLFW_KEY_PAGE_DOWN"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_HOME, "GLFW_KEY_HOME"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_END, "GLFW_KEY_END"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_CAPS_LOCK, "GLFW_KEY_CAPS_LOCK"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_SCROLL_LOCK, "GLFW_KEY_SCROLL_LOCK"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_NUM_LOCK, "GLFW_KEY_NUM_LOCK"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_PRINT_SCREEN, "GLFW_KEY_PRINT_SCREEN"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_PAUSE, "GLFW_KEY_PAUSE"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F1, "GLFW_KEY_F1"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F2, "GLFW_KEY_F2"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F3, "GLFW_KEY_F3"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F4, "GLFW_KEY_F4"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F5, "GLFW_KEY_F5"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F6, "GLFW_KEY_F6"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F7, "GLFW_KEY_F7"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F8, "GLFW_KEY_F8"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F9, "GLFW_KEY_F9"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F10, "GLFW_KEY_F10"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F11, "GLFW_KEY_F11"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F12, "GLFW_KEY_F12"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F13, "GLFW_KEY_F13"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F14, "GLFW_KEY_F14"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F15, "GLFW_KEY_F15"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F16, "GLFW_KEY_F16"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F17, "GLFW_KEY_F17"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F18, "GLFW_KEY_F18"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F19, "GLFW_KEY_F19"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F20, "GLFW_KEY_F20"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F21, "GLFW_KEY_F21"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F22, "GLFW_KEY_F22"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F23, "GLFW_KEY_F23"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F24, "GLFW_KEY_F24"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_F25, "GLFW_KEY_F25"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_0, "GLFW_KEY_KP_0"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_1, "GLFW_KEY_KP_1"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_2, "GLFW_KEY_KP_2"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_3, "GLFW_KEY_KP_3"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_4, "GLFW_KEY_KP_4"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_5, "GLFW_KEY_KP_5"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_6, "GLFW_KEY_KP_6"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_7, "GLFW_KEY_KP_7"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_8, "GLFW_KEY_KP_8"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_9, "GLFW_KEY_KP_9"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_DECIMAL, "GLFW_KEY_KP_DECIMAL"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_DIVIDE, "GLFW_KEY_KP_DIVIDE"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_MULTIPLY, "GLFW_KEY_KP_MULTIPLY"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_SUBTRACT, "GLFW_KEY_KP_SUBTRACT"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_ADD, "GLFW_KEY_KP_ADD"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_ENTER, "GLFW_KEY_KP_ENTER"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_KP_EQUAL, "GLFW_KEY_KP_EQUAL"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_LEFT_SHIFT, "GLFW_KEY_LEFT_SHIFT"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_LEFT_CONTROL, "GLFW_KEY_LEFT_CONTROL"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_LEFT_ALT, "GLFW_KEY_LEFT_ALT"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_LEFT_SUPER, "GLFW_KEY_LEFT_SUPER"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_RIGHT_SHIFT, "GLFW_KEY_RIGHT_SHIFT"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_RIGHT_CONTROL, "GLFW_KEY_RIGHT_CONTROL"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_RIGHT_ALT, "GLFW_KEY_RIGHT_ALT"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_RIGHT_SUPER, "GLFW_KEY_RIGHT_SUPER"),
            LookupEntry<GLFWKeyType>(GLFW_KEY_MENU, "GLFW_KEY_MENU"),
    };

    template<typename Input>
    static void registerActionInput(const std::string& name, sol::table& destination) {
        auto usertype = destination.new_usertype<Input>(name,
                                                        sol::call_constructor,
                                                        sol::constructors<Input(std::string_view)>(),
                                                        "name", sol::property([](const Input& i) { return i.getName(); }),
                                                        "suggestedBindings", sol::property([](const Input& i) { return i.getSuggestedBindings(); }),
                                                        "suggestBinding", &Input::suggestBinding
                                                        //"forceValue", &Input::forceValue
                                                        );

        if constexpr(std::same_as<Input, BoolInputAction>) {
            usertype.set("isPressed", sol::property([](const Input& i) { return i.isPressed(); }));
            usertype.set("wasJustPressed", sol::property([](const Input& i) { return i.wasJustPressed(); }));
            usertype.set("wasJustReleased", sol::property([](const Input& i) { return i.wasJustReleased(); }));
        } else {
            usertype.set("value", sol::property([](const Input& input) {
                return input.getValue();
            }));
            usertype.set("delta", sol::property([](const Input& input) {
                return input.getDelta();
            }));
        }
    }

    void registerIOUsertypes(sol::state& destination) {
        auto carrotNamespace = destination["Carrot"].get_or_create<sol::table>();
        auto ioNamespace = carrotNamespace["IO"].get_or_create<sol::table>();

        ioNamespace.new_usertype<ActionSet>("ActionSet",
                                            sol::call_constructor,
                                            sol::constructors<ActionSet(std::string_view)>(),
                                            "add", sol::overload(
                                                    [](ActionSet& self, BoolInputAction& action) {
                                                        self.add(action);
                                                    },
                                                    [](ActionSet& self, FloatInputAction& action) {
                                                        self.add(action);
                                                    },
                                                    [](ActionSet& self, Vec2InputAction& action) {
                                                        self.add(action);
                                                    }
                                            ),
                                            "activate", &ActionSet::activate,
                                            "deactivate", &ActionSet::deactivate
        );

        ioNamespace.set("GLFWKeyBinding", GLFWKeyBinding);
        ioNamespace.set("GLFWMouseDeltaBinding", GLFWMouseDeltaBinding);
        ioNamespace.set("GLFWGrabbedMouseDeltaBinding", GLFWGrabbedMouseDeltaBinding);
        ioNamespace.set("GLFWMousePositionBinding", GLFWMousePositionBinding);
        ioNamespace.set("GLFWMouseButtonBinding", GLFWMouseButtonBinding);

        ioNamespace.set("GLFWGamepadAxisBinding", GLFWGamepadAxisBinding);
        ioNamespace.set("GLFWGamepadButtonBinding", GLFWGamepadButtonBinding);

        ioNamespace.set("GLFWKeysVec2Binding", GLFWKeysVec2Binding);
        ioNamespace.set("GLFWGamepadVec2Binding", GLFWGamepadVec2Binding);

        registerActionInput<BoolInputAction>("BoolInputAction", ioNamespace);
        registerActionInput<FloatInputAction>("FloatInputAction", ioNamespace);
        registerActionInput<Vec2InputAction>("Vec2InputAction", ioNamespace);

        for(const LookupEntry<GLFWKeyType>& key : keyTable) {
            destination[key.name] = key.value;
        }
    }
}