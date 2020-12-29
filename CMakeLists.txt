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

function(copy_resources TYPE)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/${TYPE}" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
    message("Copying resources/${TYPE} next to binary output")
endfunction()

copy_resources(textures)
copy_resources(models)
copy_resources(pipelines)
copy_resources(materials)

set(SOURCES
        src/io/IO.cpp
        src/memory/NakedPtr.hpp
        src/Engine.cpp
        src/render/Mesh.cpp
        src/render/Model.cpp
        src/render/Vertex.cpp
        src/render/Buffer.cpp
        src/render/CameraBufferObject.cpp
        src/render/Image.cpp
        src/render/Material.cpp
        src/render/shaders/ShaderModule.cpp
        src/render/shaders/ShaderStages.cpp
        src/render/Pipeline.cpp
        src/render/shaders/Specialization.cpp
        src/render/InstanceData.cpp
        src/render/Camera.cpp
        src/render/VertexFormat.cpp
        src/render/animation/Animation.cpp
        src/utils/conversions.cpp
        src/utils/stringmanip.cpp
        src/vulkan/CustomTracyVulkan.cpp
        src/render/GBuffer.cpp
        src/render/raytracing/RayTracer.cpp
        src/main.cpp
        src/stb_impls.cpp

        src/game/Game.cpp
        src/game/Unit.cpp
        )

set(VERTEX_SHADERS
        resources/shaders/gBuffer.vertex.glsl
        resources/shaders/gBufferWithBoneInfo.vertex.glsl
        resources/shaders/screenQuad.vertex.glsl
        )

set(FRAGMENT_SHADERS
        resources/shaders/gBuffer.fragment.glsl
        resources/shaders/blit.fragment.glsl
        resources/shaders/gResolve.fragment.glsl
        )

set(COMPUTE_SHADERS
        resources/shaders/skinning.compute.glsl
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

foreach(shader ${COMPUTE_SHADERS})
    add_spirv_shader(compute "${shader}" "${shader}.spv")
    set(SHADERS "${SHADERS}" "${shader}.spv")
endforeach()

set(EXTERNAL_SOURCES thirdparty/tracy/TracyClient.cpp)

include_directories(src/)
include_directories(thirdparty/stb/)
include_directories(thirdparty/rapidjson/include)
include_directories(thirdparty/tracy/)

set(ALL_SOURCES ${SOURCES} ${SHADERS} ${EXTERNAL_SOURCES})
set(ALL_LIBS Vulkan::Vulkan spirv-cross-core glfw glm assimp::assimp)

add_executable(Carrot ${ALL_SOURCES})
target_link_libraries(Carrot ${ALL_LIBS})
target_compile_definitions(Carrot PRIVATE TRACY_ENABLE=1)

add_executable(Carrot-Renderdoc ${ALL_SOURCES})
target_compile_definitions(Carrot-Renderdoc PRIVATE DEBUG_MARKERS=1)
target_link_libraries(Carrot-Renderdoc ${ALL_LIBS})

add_executable(Carrot-NoTracy ${ALL_SOURCES})
target_link_libraries(Carrot-NoTracy ${ALL_LIBS})