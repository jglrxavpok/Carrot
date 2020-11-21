cmake_minimum_required(VERSION 3.17)
project(Carrot)
set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
add_subdirectory(thirdparty/glfw-3.3.2/)
add_subdirectory(thirdparty/glm-0.9.9.8/glm/)


set(SOURCES
        src/main.cpp
        src/CarrotEngine.cpp
        src/memory/NakedPtr.hpp
        )
add_executable(Carrot ${SOURCES})
target_link_libraries(Carrot Vulkan::Vulkan glfw glm)
