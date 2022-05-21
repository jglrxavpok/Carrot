//
// Created by jglrxavpok on 13/09/2021.
//

#include <glm/glm.hpp>
#include <sol/sol.hpp>
#include "glm.hpp"

namespace Carrot::Lua {
    template<glm::length_t dim, typename Elem, glm::qualifier qualifier, typename fullConstructor>
    static sol::usertype<glm::vec<dim, Elem, qualifier>> registerVectorType(sol::table& state, std::string_view name) {
        using vec = glm::vec<dim, Elem, qualifier>;
        auto userType = state.new_usertype<vec>(name,
                                                sol::call_constructor,
                                                sol::constructors<vec(), vec(Elem), fullConstructor>(),
                                                "x", &vec::x,
                                                "r", &vec::r,
                                                "s", &vec::s,
                                                sol::meta_function::addition, sol::overload(
                                                        [](vec& a, vec& b) {
                                                            return a + b;
                                                        }
                                                ),
                                                sol::meta_function::subtraction, sol::overload(
                                                        [](vec& a, vec& b) {
                                                            return a - b;
                                                        }
                                                ),
                                                sol::meta_function::multiplication, sol::overload(
                                                        [](vec& a, vec& b) {
                                                            return a * b;
                                                        },
                                                        [](vec& a, Elem f) {
                                                            return a * f;
                                                        }
                                                )
        );
        if constexpr(!std::same_as<bool, std::decay_t<Elem>>) {
            userType.set(sol::meta_function::division, sol::overload(
                    [](vec& a, vec& b) {
                        return a / b;
                    },
                    [](vec& a, Elem f) {
                        return a / f;
                    }
            ));
        }
        if constexpr (dim >= 2) {
            userType["y"] = &vec::y;
            userType["g"] = &vec::g;
            userType["t"] = &vec::t;
        }
        if constexpr (dim >= 3) {
            userType["z"] = &vec::z;
            userType["b"] = &vec::b;
        }
        if constexpr (dim >= 4) {
            userType["w"] = &vec::w;
            userType["a"] = &vec::a;
        }
        return userType;
    }

    template<typename Elem>
    static void registerVectors(sol::table& glmNamespace, const char* prefix) {
        std::string name = std::string(prefix) + "vec";
        registerVectorType<1, Elem, glm::defaultp, glm::vec1(Elem)>(glmNamespace, name + "1");
        registerVectorType<2, Elem, glm::defaultp, glm::vec2(Elem, Elem)>(glmNamespace, name + "2");
        registerVectorType<3, Elem, glm::defaultp, glm::vec3(Elem, Elem, Elem)>(glmNamespace, name + "3");
        registerVectorType<4, Elem, glm::defaultp, glm::vec4(Elem, Elem, Elem, Elem)>(glmNamespace, name + "4");

    }

    void registerGLMUsertypes(sol::state& state) {
        auto glmNamespace = state["glm"].get_or_create<sol::table>();
        registerVectors<float>(glmNamespace, "");
        registerVectors<double>(glmNamespace, "d");
        registerVectors<bool>(glmNamespace, "b");
        registerVectors<int>(glmNamespace, "i");
        registerVectors<glm::uint>(glmNamespace, "u");

        glmNamespace["length"] = sol::overload(
                glm::length<1, float, glm::defaultp>,
                glm::length<2, float, glm::defaultp>,
                glm::length<3, float, glm::defaultp>,
                glm::length<4, float, glm::defaultp>,

                glm::length<1, double, glm::defaultp>,
                glm::length<2, double, glm::defaultp>,
                glm::length<3, double, glm::defaultp>,
                glm::length<4, double, glm::defaultp>
        );
    }
}