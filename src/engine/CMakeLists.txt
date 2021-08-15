#set(EngineRoot src/engine/)
set(EngineRoot "")
set(ENGINE-SOURCES
        ${EngineRoot}io/IO.cpp
        ${EngineRoot}io/Logging.cpp
        ${EngineRoot}io/FileHandle.cpp
        ${EngineRoot}io/Resource.cpp

        ${EngineRoot}io/actions/ActionSet.cpp


        ${EngineRoot}memory/NakedPtr.hpp
        ${EngineRoot}Engine.cpp
        ${EngineRoot}render/resources/Mesh.cpp
        ${EngineRoot}render/Model.cpp
        ${EngineRoot}render/resources/Vertex.cpp
        ${EngineRoot}render/resources/Buffer.cpp
        ${EngineRoot}render/resources/BufferView.cpp
        ${EngineRoot}render/resources/Image.cpp
        ${EngineRoot}render/resources/ResourceAllocator.cpp
        ${EngineRoot}render/resources/Texture.cpp
        ${EngineRoot}render/Material.cpp
        ${EngineRoot}render/shaders/ShaderModule.cpp
        ${EngineRoot}render/shaders/ShaderStages.cpp
        ${EngineRoot}render/resources/Pipeline.cpp
        ${EngineRoot}render/shaders/Specialization.cpp
        ${EngineRoot}render/InstanceData.cpp
        ${EngineRoot}render/Camera.cpp
        ${EngineRoot}render/resources/VertexFormat.cpp
        ${EngineRoot}render/animation/Animation.cpp
        ${EngineRoot}utils/conversions.cpp
        ${EngineRoot}utils/stringmanip.cpp
        ${EngineRoot}render/resources/Font.cpp

        ${EngineRoot}ecs/components/Component.cpp
        ${EngineRoot}ecs/components/Transform.cpp
        ${EngineRoot}ecs/systems/System.cpp
        ${EngineRoot}ecs/systems/SystemUpdateAnimatedModelInstance.cpp
        ${EngineRoot}ecs/systems/SystemSinPosition.cpp
        ${EngineRoot}ecs/systems/SystemUpdateLightPosition.cpp
        ${EngineRoot}ecs/systems/SystemKinematics.cpp
        ${EngineRoot}ecs/World.cpp
        ${EngineRoot}ecs/Signature.cpp

        ${EngineRoot}vulkan/CustomTracyVulkan.cpp
        ${EngineRoot}render/GBuffer.cpp
        ${EngineRoot}render/animation/AnimatedInstances.cpp
        ${EngineRoot}render/raytracing/RayTracer.cpp
        ${EngineRoot}render/raytracing/ASBuilder.cpp
        ${EngineRoot}render/raytracing/AccelerationStructure.cpp
        ${EngineRoot}single_file_header_impls.cpp
        ${EngineRoot}vulkan/VulkanDriver.cpp
        ${EngineRoot}render/VulkanRenderer.cpp
        ${EngineRoot}LoadingScreen.cpp

        ${EngineRoot}audio/SoundSource.cpp
        ${EngineRoot}audio/Sound.cpp
        ${EngineRoot}audio/SoundThread.cpp
        ${EngineRoot}audio/decoders/WavDecoder.cpp
        ${EngineRoot}audio/decoders/MP3Decoder.cpp
        ${EngineRoot}audio/decoders/VorbisDecoder.cpp
        ${EngineRoot}audio/SoundListener.cpp

        ${EngineRoot}render/particles/ParticleSystem.cpp
        ${EngineRoot}render/particles/ParticleEmitter.cpp
        ${EngineRoot}render/particles/ParticleBlueprint.cpp

        ${EngineRoot}render/RenderGraph.cpp
        ${EngineRoot}render/RenderPass.cpp
        ${EngineRoot}render/Composer.cpp
        ${EngineRoot}render/TextureRepository.cpp

        ${EngineRoot}render/ComputePipeline.cpp

        ${EngineRoot}expressions/Expressions.cpp
        ${EngineRoot}console/Console.cpp

        ${EngineRoot}console/RuntimeOption.cpp
        ${EngineRoot}console/AutocompleteField.cpp

        ${EngineRoot}tasks/PresentThread.cpp


       # PARENT_SCOPE
        )

set(VERTEX_SHADERS
        ${ProjectRoot}resources/shaders/gBuffer.vertex.glsl
        ${ProjectRoot}resources/shaders/gBufferWithBoneInfo.vertex.glsl
        ${ProjectRoot}resources/shaders/screenQuad.vertex.glsl
        ${ProjectRoot}resources/shaders/skybox.vertex.glsl
        ${ProjectRoot}resources/shaders/particles.vertex.glsl
        ${ProjectRoot}resources/shaders/screenQuad-transformed.vertex.glsl
        ${ProjectRoot}resources/shaders/text-rendering.vertex.glsl

        #PARENT_SCOPE
        )

set(FRAGMENT_SHADERS
        ${ProjectRoot}resources/shaders/gBuffer.fragment.glsl
        ${ProjectRoot}resources/shaders/blit.fragment.glsl
        ${ProjectRoot}resources/shaders/gResolve.fragment.glsl
        ${ProjectRoot}resources/shaders/skybox.fragment.glsl
        ${ProjectRoot}resources/shaders/particles.fragment.glsl
        ${ProjectRoot}resources/shaders/gResolve-rendergraph.fragment.glsl
        ${ProjectRoot}resources/shaders/composer-blit.fragment.glsl
        ${ProjectRoot}resources/shaders/text-rendering.fragment.glsl

        #PARENT_SCOPE
        )

set(COMPUTE_SHADERS
        ${ProjectRoot}resources/shaders/compute/skinning.compute.glsl
        ${ProjectRoot}resources/shaders/compute/particles.compute.glsl

        #PARENT_SCOPE
        )

set(RAYGEN_SHADERS
        ${ProjectRoot}resources/shaders/rt/raytrace.rgen

        #PARENT_SCOPE
        )

set(RAYMISS_SHADERS
        ${ProjectRoot}resources/shaders/rt/raytrace.rmiss
        ${ProjectRoot}resources/shaders/rt/shadow.rmiss

        #PARENT_SCOPE
        )

set(RAYHIT_SHADERS
        ${ProjectRoot}resources/shaders/rt/raytrace.rchit

        #PARENT_SCOPE
        )

set(ENGINE-SHADERS "" PARENT_SCOPE)
foreach(shader ${VERTEX_SHADERS})
    add_spirv_shader(vertex "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${shader}.spv")
endforeach()

foreach(shader ${FRAGMENT_SHADERS})
    add_spirv_shader(fragment "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${shader}.spv")
endforeach()

foreach(shader ${COMPUTE_SHADERS})
    add_spirv_shader(compute "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${shader}.spv")
endforeach()

foreach(shader ${RAYGEN_SHADERS})
    add_spirv_shader(rgen "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${shader}.spv")
endforeach()

foreach(shader ${RAYMISS_SHADERS})
    add_spirv_shader(rmiss "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${shader}.spv")
endforeach()

foreach(shader ${RAYHIT_SHADERS})
    add_spirv_shader(rchit "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${shader}.spv")
endforeach()

set(THIRDPARTY-SOURCES
        ${ProjectRoot}thirdparty/tracy/TracyClient.cpp
        ${ProjectRoot}thirdparty/imgui/backends/imgui_impl_vulkan.cpp
        ${ProjectRoot}thirdparty/imgui/backends/imgui_impl_glfw.cpp
        ${ProjectRoot}thirdparty/imgui/imgui.cpp
        ${ProjectRoot}thirdparty/imgui/imgui_draw.cpp
        ${ProjectRoot}thirdparty/imgui/imgui_demo.cpp
        ${ProjectRoot}thirdparty/imgui/imgui_widgets.cpp
        ${ProjectRoot}thirdparty/imgui/imgui_tables.cpp
        ${ProjectRoot}thirdparty/imgui/misc/cpp/imgui_stdlib.cpp

        ${ProjectRoot}thirdparty/imgui-node-editor/crude_json.cpp
        ${ProjectRoot}thirdparty/imgui-node-editor/imgui_canvas.cpp
        ${ProjectRoot}thirdparty/imgui-node-editor/imgui_node_editor.cpp
        ${ProjectRoot}thirdparty/imgui-node-editor/imgui_node_editor_api.cpp
        )

set(VR_SOURCES
        vr/Session.cpp
        vr/VRInterface.cpp
        )

set(ALL_BASE_LIBS Vulkan::Vulkan spirv-cross-core spirv-cross-glsl glfw glm assimp::assimp OpenAL SPIRV nfd)

add_library(Engine-Base ${ENGINE-SOURCES} ${ENGINE-SHADERS} ${THIRDPARTY-SOURCES})
target_link_libraries(Engine-Base PUBLIC ${ALL_BASE_LIBS})
add_includes(Engine-Base)

add_library(Engine-Base-Renderdoc ${ENGINE-SOURCES} ${ENGINE-SHADERS} ${THIRDPARTY-SOURCES})
target_link_libraries(Engine-Base-Renderdoc PUBLIC ${ALL_BASE_LIBS})
target_compile_definitions(Engine-Base-Renderdoc PRIVATE DEBUG_MARKERS=1)
add_includes(Engine-Base-Renderdoc)

add_library(Engine-ASAN ${ENGINE-SOURCES} ${ENGINE-SHADERS} ${THIRDPARTY-SOURCES})
target_link_libraries(Engine-ASAN PUBLIC ${ALL_BASE_LIBS})
make_asan_target(Engine-ASAN)
add_includes(Engine-ASAN)

add_library(Engine-Tracy ${ENGINE-SOURCES} ${ENGINE-SHADERS} ${THIRDPARTY-SOURCES})
target_link_libraries(Engine-Tracy PUBLIC ${ALL_BASE_LIBS})
make_tracy_target(Engine-Tracy)
add_includes(Engine-Tracy)

add_library(Engine-VR ${ENGINE-SOURCES} ${ENGINE-SHADERS} ${THIRDPARTY-SOURCES} ${VR_SOURCES})
target_include_directories(Engine-VR PUBLIC ${OPENXR-INCLUDE-DIR})
target_include_directories(Engine-VR PUBLIC ${ProjectRoot}thirdparty/openxr-hpp/include/)
target_compile_definitions(Engine-VR PUBLIC ENABLE_VR=1)
target_compile_definitions(Engine-VR PUBLIC TRACY_ENABLE=1)
target_link_libraries(Engine-VR ${ALL_BASE_LIBS} ${OPENXR-LOADER-LIB-LOCATION})
add_includes(Engine-VR)