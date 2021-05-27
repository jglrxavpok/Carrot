cmake_minimum_required(VERSION 3.17)
project(Carrot)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
list(APPEND CMAKE_PREFIX_PATH "cmake_modules/")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(ARCH "x64")

include(FetchContent)
find_package(Vulkan REQUIRED)
find_package(Aftermath)
add_subdirectory(thirdparty/glfw-3.3.2/)
add_subdirectory(thirdparty/glm-0.9.9.8/glm/)

set(ASSIMP_BUILD_TESTS CACHE BOOL OFF)
set(ASSIMP_BUILD_GLTF_IMPORTER CACHE BOOL OFF)
set(ASSIMP_BUILD_GLTF_EXPORTER CACHE BOOL OFF)
add_subdirectory(thirdparty/assimp)

add_subdirectory(thirdparty/nativefiledialog-extended)

FetchContent_Declare(spirv_cross
        GIT_REPOSITORY      https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG             2021-01-15)
FetchContent_MakeAvailable(spirv_cross)
add_subdirectory(thirdparty/glslang)

FetchContent_Declare(openal-soft
        GIT_REPOSITORY      https://github.com/kcat/openal-soft.git
        GIT_TAG             1.21.1)
FetchContent_MakeAvailable(openal-soft)

function(add_spirv_shader SHADER_STAGE INPUT_FILE OUTPUT_FILE)
    set(FULL_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/${INPUT_FILE}")
    set(FULL_OUTPUT "${CMAKE_BINARY_DIR}/${OUTPUT_FILE}")
    add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/resources/shaders/"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/resources/shaders/compute"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/resources/shaders/includes"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/resources/shaders/rt"
            COMMAND echo glslc -g --target-env=vulkan1.2 -fshader-stage=${SHADER_STAGE} "${FULL_INPUT}" -o "${FULL_OUTPUT}"
            COMMAND glslc -g --target-env=vulkan1.2 -fshader-stage=${SHADER_STAGE} "${FULL_INPUT}" -o "${FULL_OUTPUT}"
            MAIN_DEPENDENCY ${FULL_INPUT}
    )
endfunction()

function(copy_resources TYPE)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/${TYPE}" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
    message("Copying resources/${TYPE} next to binary output")
endfunction()

copy_resources(textures)
copy_resources(textures/icons)
copy_resources(models)
copy_resources(pipelines)
copy_resources(materials)
copy_resources(sounds)
copy_resources(musics)
copy_resources(node_templates)
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon32.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon64.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon128.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/splash.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")

set(ENGINE-SOURCES
        src/engine/io/IO.cpp
        src/engine/memory/NakedPtr.hpp
        src/engine/Engine.cpp
        src/engine/render/resources/Mesh.cpp
        src/engine/render/Model.cpp
        src/engine/render/resources/Vertex.cpp
        src/engine/render/resources/Buffer.cpp
        src/engine/render/resources/BufferView.cpp
        src/engine/render/resources/Image.cpp
        src/engine/render/resources/ResourceAllocator.cpp
        src/engine/render/Material.cpp
        src/engine/render/shaders/ShaderModule.cpp
        src/engine/render/shaders/ShaderStages.cpp
        src/engine/render/resources/Pipeline.cpp
        src/engine/render/shaders/Specialization.cpp
        src/engine/render/InstanceData.cpp
        src/engine/render/Camera.cpp
        src/engine/render/resources/VertexFormat.cpp
        src/engine/render/animation/Animation.cpp
        src/engine/utils/conversions.cpp
        src/engine/utils/stringmanip.cpp

        src/engine/ecs/components/Component.cpp
        src/engine/ecs/components/Transform.cpp
        src/engine/ecs/systems/System.cpp
        src/engine/ecs/systems/SystemUpdateAnimatedModelInstance.cpp
        src/engine/ecs/systems/SystemSinPosition.cpp
        src/engine/ecs/systems/SystemUpdateLightPosition.cpp
        src/engine/ecs/World.cpp
        src/engine/ecs/Signature.cpp

        src/engine/vulkan/CustomTracyVulkan.cpp
        src/engine/render/GBuffer.cpp
        src/engine/render/animation/AnimatedInstances.cpp
        src/engine/render/raytracing/RayTracer.cpp
        src/engine/render/raytracing/ASBuilder.cpp
        src/engine/render/raytracing/AccelerationStructure.cpp
        src/engine/single_file_header_impls.cpp
        src/engine/vulkan/VulkanDriver.cpp
        src/engine/render/VulkanRenderer.cpp
        src/engine/LoadingScreen.cpp

        src/engine/audio/SoundSource.cpp
        src/engine/audio/Sound.cpp
        src/engine/audio/SoundThread.cpp
        src/engine/audio/decoders/WavDecoder.cpp
        src/engine/audio/decoders/MP3Decoder.cpp
        src/engine/audio/decoders/VorbisDecoder.cpp
        src/engine/audio/SoundListener.cpp

        src/engine/render/particles/ParticleSystem.cpp
        src/engine/render/particles/ParticleEmitter.cpp
        src/engine/render/particles/ParticleBlueprint.cpp

        src/engine/render/ComputePipeline.cpp

        src/engine/expressions/Expressions.cpp
        )

set(VERTEX_SHADERS
        resources/shaders/gBuffer.vertex.glsl
        resources/shaders/gBufferWithBoneInfo.vertex.glsl
        resources/shaders/screenQuad.vertex.glsl
        resources/shaders/skybox.vertex.glsl
        resources/shaders/particles.vertex.glsl
        )

set(FRAGMENT_SHADERS
        resources/shaders/gBuffer.fragment.glsl
        resources/shaders/blit.fragment.glsl
        resources/shaders/gResolve.fragment.glsl
        resources/shaders/skybox.fragment.glsl
        resources/shaders/particles.fragment.glsl
        )

set(COMPUTE_SHADERS
        resources/shaders/skinning.compute.glsl
        resources/shaders/compute/particles.compute.glsl
        )

set(RAYGEN_SHADERS
        resources/shaders/rt/raytrace.rgen
        )

set(RAYMISS_SHADERS
        resources/shaders/rt/raytrace.rmiss
        resources/shaders/rt/shadow.rmiss
        )

set(RAYHIT_SHADERS
        resources/shaders/rt/raytrace.rchit
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

foreach(shader ${RAYGEN_SHADERS})
    add_spirv_shader(rgen "${shader}" "${shader}.spv")
    set(SHADERS "${SHADERS}" "${shader}.spv")
endforeach()

foreach(shader ${RAYMISS_SHADERS})
    add_spirv_shader(rmiss "${shader}" "${shader}.spv")
    set(SHADERS "${SHADERS}" "${shader}.spv")
endforeach()

foreach(shader ${RAYHIT_SHADERS})
    add_spirv_shader(rchit "${shader}" "${shader}.spv")
    set(SHADERS "${SHADERS}" "${shader}.spv")
endforeach()

set(EXTERNAL_SOURCES
        thirdparty/tracy/TracyClient.cpp
        thirdparty/imgui/backends/imgui_impl_vulkan.cpp
        thirdparty/imgui/backends/imgui_impl_glfw.cpp
        thirdparty/imgui/imgui.cpp
        thirdparty/imgui/imgui_draw.cpp
        thirdparty/imgui/imgui_demo.cpp
        thirdparty/imgui/imgui_widgets.cpp
        thirdparty/imgui/imgui_tables.cpp

        thirdparty/imgui-node-editor/crude_json.cpp
        thirdparty/imgui-node-editor/imgui_canvas.cpp
        thirdparty/imgui-node-editor/imgui_node_editor.cpp
        thirdparty/imgui-node-editor/imgui_node_editor_api.cpp
        )

include_directories(src/)
include_directories(thirdparty/stb/)
include_directories(thirdparty/rapidjson/include)
include_directories(thirdparty/tracy/)
include_directories(thirdparty/imgui/)
include_directories(thirdparty/dr_libs/)
include_directories(thirdparty/imgui-node-editor/)
include_directories(thirdparty/stduuid/include)

set(GAME-SOURCES
        src/main.cpp

        src/game/Game.cpp
        )

set(TOOLS-SOURCES
        src/tools/ParticleEditor.cpp
        src/tools/EditorSettings.cpp
        src/tools/EditorGraph.cpp
        src/tools/EditorNode.cpp
        src/tools/nodes/TerminalNodes.cpp

        src/tools/generators/ParticleShaderGenerator.cpp



        src/tools/ToolsEntryPoint.cpp
        )

set(ALL_SOURCES ${ENGINE-SOURCES} ${GAME-SOURCES} ${SHADERS} ${EXTERNAL_SOURCES})
set(ALL_LIBS Vulkan::Vulkan spirv-cross-core spirv-cross-glsl glfw glm assimp::assimp OpenAL SPIRV nfd)

add_executable(Carrot ${ALL_SOURCES})
target_link_libraries(Carrot ${ALL_LIBS})
target_compile_definitions(Carrot PRIVATE TRACY_ENABLE=1)

add_executable(Carrot-Renderdoc ${ALL_SOURCES})
target_compile_definitions(Carrot-Renderdoc PRIVATE DEBUG_MARKERS=1)
target_link_libraries(Carrot-Renderdoc ${ALL_LIBS})

add_executable(Carrot-NoTracy ${ALL_SOURCES})
target_link_libraries(Carrot-NoTracy ${ALL_LIBS})

add_executable(Carrot-Tools ${ENGINE-SOURCES} ${SHADERS} ${EXTERNAL_SOURCES} ${TOOLS-SOURCES})
target_link_libraries(Carrot-Tools ${ALL_LIBS})

link_directories(OpenAL_BINARY_DIR)
add_executable(Carrot-TestAudio ${ENGINE-SOURCES} ${EXTERNAL_SOURCES} tests/Audio.cpp)
target_link_libraries(Carrot-TestAudio ${ALL_LIBS})

if(AFTERMATH_LIBS)
    set(AFTERMATH_SOURCES
            src/engine/utils/AftermathIntegration.cpp)
    add_executable(Carrot-GPUDebug ${ALL_SOURCES} ${AFTERMATH_SOURCES})
    target_include_directories(Carrot-GPUDebug PRIVATE ${AFTERMATH_INCLUDES})
    target_compile_definitions(Carrot-GPUDebug PRIVATE AFTERMATH_ENABLE=1)
    target_compile_definitions(Carrot-GPUDebug PRIVATE GPU_ASSISTED_VALIDATION=1)
    target_compile_definitions(Carrot-GPUDebug PRIVATE DEBUG_MARKERS=1)
    target_link_libraries(Carrot-GPUDebug ${ALL_LIBS} ${AFTERMATH_LIBS})
else()
    message("Could not find Aftermath SDK")
endif()