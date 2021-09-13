#include <iostream>
#include <engine/scripting/LuaScript.h>
#include <glm/glm.hpp>

int main(int, char*[]) {
    Carrot::Lua::Script script = R"lua(
        v1 = glm.vec2.new(1, 2)
        v2 = glm.vec3.new(1, 2, 3)
        v2.x = 45
        v2.y = glm.length(v1)
        print('test')
    )lua";

    sol::state& lua = script.getLuaState();

    glm::vec2 v1 = lua["v1"];
    glm::vec3 v2 = lua["v2"];
    std::cout << v1.x << "; "<< v1.y << std::endl;
    std::cout << v2.x << "; "<< v2.y << "; "<< v2.z << std::endl;

    std::cout << std::endl;

    return 0;
}