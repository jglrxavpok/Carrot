cmake_minimum_required(VERSION 3.17)
project(Carrot)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

include(FetchContent)
find_package(Vulkan REQUIRED)
add_subdirectory(thirdparty/glfw-3.3.2/)
add_subdirectory(thirdparty/glm-0.9.9.8/glm/)

set(ASSIMP_BUILD_TESTS CACHE BOOL OFF)
set(ASSIMP_BUILD_GLTF_IMPORTER CACHE BOOL OFF)
set(ASSIMP_BUILD_GLTF_EXPORTER CACHE BOOL OFF)
add_subdirectory(thirdparty/assimp)

FetchContent_Declare(spirv_cross
        GIT_REPOSITORY      https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG             2020-09-17)
FetchContent_MakeAvailable(spirv_cross)

function(add_spirv_shader SHADER_STAGE INPUT_FILE OUTPUT_FILE)
    set(FULL_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${INPUT_FILE}")
    set(FULL_OUTPUT "${CMAKE_BINARY_DIR}/${OUTPUT_FILE}")
    add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/resources/shaders/"
            COMMAND echo glslc -fshader-stage=${SHADER_STAGE} "${FULL_INPUT}" -o "${FULL_OUTPUT}"
            COMMAND glslc -fshader-stage=${SHADER_STAGE} "${FULL_INPUT}" -o "${FULL_OUTPUT}"
            MAIN_DEPENDENCY ${FULL_INPUT}
    )
endfunction()

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/textures" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/models" DESTINATION "${CMAKE_BINARY_DIR}/resources/")

set(SOURCES
        src/io/IO.cpp
        src/memory/NakedPtr.hpp
        src/Engine.cpp
        src/render/Mesh.cpp
        src/render/Model.cpp
        src/render/Vertex.cpp
        src/render/Buffer.cpp
        src/render/UniformBufferObject.cpp
        src/render/Image.cpp
        src/render/Material.cpp
        src/render/ShaderModule.cpp
        src/render/ShaderStages.cpp
        src/render/Pipeline.cpp
        src/utils/stringmanip.cpp
        src/main.cpp
        src/stb_impls.cpp
        )

set(VERTEX_SHADERS
        resources/shaders/default.vertex.glsl
        )

set(FRAGMENT_SHADERS
        resources/shaders/default.fragment.glsl
        )

set(SHADERS "")
foreach(shader ${VERTEX_SHADERS})
    add_spirv_shader(vertex "${shader}" "${shader}.spv")
    set(SHADERS "${SHADERS}" "${shader}.spv")
endforeach()

foreach(shader ${FRAGMENT_SHADERS})
    add_spirv_shader(fragment "${shader}" "${shader}.spv")
    set(SHADERS "${SHADERS}" "${shader}.spv")
endforeach()

include_directories(src/)
include_directories(thirdparty/stb/)
add_executable(Carrot ${SOURCES} ${SHADERS})
target_link_libraries(Carrot Vulkan::Vulkan spirv-cross-core glfw glm assimp::assimp)
