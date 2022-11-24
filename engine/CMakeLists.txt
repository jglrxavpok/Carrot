set(EngineRoot "engine/")
set(ShadersRoot "${EngineRoot}resources/shaders")
set(ENGINE-SOURCES
        ${EngineRoot}Window.cpp

        ${EngineRoot}io/actions/Action.cpp
        ${EngineRoot}io/actions/ActionSet.cpp
        ${EngineRoot}io/AssimpCompatibilityLayer.cpp

        ${EngineRoot}Engine.cpp
        ${EngineRoot}render/Model.cpp

        ${EngineRoot}render/resources/Buffer.cpp
        ${EngineRoot}render/resources/BufferView.cpp
        ${EngineRoot}render/resources/DeviceMemory.cpp
        ${EngineRoot}render/resources/Image.cpp
        ${EngineRoot}render/resources/Font.cpp
        ${EngineRoot}render/resources/LightMesh.cpp
        ${EngineRoot}render/resources/Mesh.cpp
        ${EngineRoot}render/resources/SingleMesh.cpp
        ${EngineRoot}render/resources/ResourceAllocator.cpp
        ${EngineRoot}render/resources/SingleFrameStackGPUAllocator.cpp
        ${EngineRoot}render/resources/Skeleton.cpp
        ${EngineRoot}render/resources/Texture.cpp
        ${EngineRoot}render/resources/Vertex.cpp
        ${EngineRoot}render/resources/VertexFormat.cpp

        ${EngineRoot}render/resources/model_loading/AssimpLoader.cpp
        ${EngineRoot}render/resources/model_loading/GLTFLoader.cpp

        ${EngineRoot}render/shaders/ShaderModule.cpp
        ${EngineRoot}render/shaders/ShaderStages.cpp
        ${EngineRoot}render/shaders/ShaderSource.cpp
        ${EngineRoot}render/resources/Pipeline.cpp
        ${EngineRoot}render/shaders/Specialization.cpp
        ${EngineRoot}render/InstanceData.cpp
        ${EngineRoot}render/Camera.cpp
        ${EngineRoot}render/RenderPacket.cpp

        ${EngineRoot}render/animation/Animation.cpp
        ${EngineRoot}render/animation/SkeletalModelRenderer.cpp

        ${EngineRoot}utils/conversions.cpp

        ${EngineRoot}ecs/components/CameraComponent.cpp
        ${EngineRoot}ecs/components/Component.cpp
        ${EngineRoot}ecs/components/LightComponent.cpp
        ${EngineRoot}ecs/components/LuaScriptComponent.cpp
        ${EngineRoot}ecs/components/ModelComponent.cpp
        ${EngineRoot}ecs/components/RigidBodyComponent.cpp
        ${EngineRoot}ecs/components/SpriteComponent.cpp
        ${EngineRoot}ecs/components/TextComponent.cpp
        ${EngineRoot}ecs/components/TransformComponent.cpp

        ${EngineRoot}ecs/systems/CameraSystem.cpp
        ${EngineRoot}ecs/systems/LuaSystems.cpp
        ${EngineRoot}ecs/systems/ModelRenderSystem.cpp
        ${EngineRoot}ecs/systems/System.cpp
        ${EngineRoot}ecs/systems/SystemHandleLights.cpp
        ${EngineRoot}ecs/systems/SystemKinematics.cpp
        ${EngineRoot}ecs/systems/SpriteRenderSystem.cpp
        ${EngineRoot}ecs/systems/SystemSinPosition.cpp
        ${EngineRoot}ecs/systems/SystemUpdateAnimatedModelInstance.cpp
        ${EngineRoot}ecs/systems/TextRenderSystem.cpp
        ${EngineRoot}ecs/systems/RigidBodySystem.cpp

        ${EngineRoot}ecs/World.cpp
        ${EngineRoot}ecs/Signature.cpp

        ${EngineRoot}render/GBuffer.cpp
        ${EngineRoot}render/animation/AnimatedInstances.cpp
        ${EngineRoot}render/raytracing/RayTracer.cpp
        ${EngineRoot}render/raytracing/ASBuilder.cpp
        ${EngineRoot}render/raytracing/AccelerationStructure.cpp
        ${EngineRoot}single_file_header_impls.cpp
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

        ${EngineRoot}render/AnimatedSprite.cpp
        ${EngineRoot}render/BillboardRenderer.cpp
        ${EngineRoot}render/MaterialSystem.cpp
        ${EngineRoot}render/RenderContext.cpp
        ${EngineRoot}render/RenderPacketContainer.cpp
        ${EngineRoot}render/Sprite.cpp
        ${EngineRoot}render/TextureAtlas.cpp
        ${EngineRoot}render/Viewport.cpp
        ${EngineRoot}render/lighting/Lights.cpp

        ${EngineRoot}console/Console.cpp

        ${EngineRoot}console/RuntimeOption.cpp
        ${EngineRoot}console/AutocompleteField.cpp

        ${EngineRoot}math/Transform.cpp

        ${EngineRoot}network/client/Client.cpp
        ${EngineRoot}network/server/Server.cpp
        ${EngineRoot}network/NetworkInterface.cpp

        ${EngineRoot}network/packets/HandshakePackets.cpp

        ${EngineRoot}scene/Scene.cpp

        ${EngineRoot}scripting/LuaScript.cpp
        ${EngineRoot}scripting/bindings/all.cpp
        ${EngineRoot}scripting/bindings/glm.cpp
        ${EngineRoot}scripting/bindings/Engine.cpp

        ${EngineRoot}scripting/bindings/ecs/ECS.cpp

        ${EngineRoot}scripting/bindings/io/InputSystem.cpp

        ${EngineRoot}scripting/bindings/math/bindings.cpp

        ${EngineRoot}scripting/bindings/vulkan/VulkanDriver.cpp
        ${EngineRoot}scripting/bindings/vulkan/vulkan.cpp

        ${EngineRoot}scripting/bindings/render/RenderContext.cpp
        ${EngineRoot}scripting/bindings/render/Sprite.cpp
        ${EngineRoot}scripting/bindings/render/VulkanRenderer.cpp

        ${EngineRoot}scripting/bindings/render/resources/Texture.cpp

        ${EngineRoot}edition/EditorSettings.cpp
        ${EngineRoot}edition/FreeCameraController.cpp
        ${EngineRoot}edition/DragDropTypes.cpp


        ${EngineRoot}physics/Colliders.cpp
        ${EngineRoot}physics/MeshCollisionShape.cpp
        ${EngineRoot}physics/PhysicsSystem.cpp
        ${EngineRoot}physics/RigidBody.cpp

        ${EngineRoot}task/TaskScheduler.cpp

        ${EngineRoot}utils/Profiling.cpp

        ${EngineRoot}vulkan/CustomTracyVulkan.cpp
        ${EngineRoot}vulkan/DebugNameable.cpp
        ${EngineRoot}vulkan/SynchronizedQueue.cpp
        ${EngineRoot}vulkan/VulkanDriver.cpp

        ${EngineRoot}utils/AftermathIntegration.cpp

        # VR specific code
        ${EngineRoot}vr/HandTracking.cpp
        ${EngineRoot}vr/Session.cpp
        ${EngineRoot}vr/VRInterface.cpp
        )

set(VERTEX_SHADERS
        gBuffer.vertex.glsl
        screenQuad.vertex.glsl
        skybox.vertex.glsl
        particles.vertex.glsl
        screenQuad-transformed.vertex.glsl
        text-rendering.vertex.glsl
        gBufferSprite.vertex.glsl
        billboards.vertex.glsl
        gBufferWireframe.vertex.glsl
        gBufferWithBoneInfo.vertex.glsl

        #PARENT_SCOPE
        )

set(FRAGMENT_SHADERS
        gBuffer.fragment.glsl
        blit.fragment.glsl
        skybox.fragment.glsl
        particles.fragment.glsl
        lighting-noraytracing.fragment.glsl
        lighting-raytracing.fragment.glsl
        composer-blit.fragment.glsl
        text-rendering.fragment.glsl
        gBufferSprite.fragment.glsl
        billboards.fragment.glsl

        post-process/denoise.fragment.glsl
        post-process/merge-lighting.fragment.glsl
        post-process/tone-mapping.fragment.glsl

        #PARENT_SCOPE
        )

set(COMPUTE_SHADERS
        compute/animation-skinning.compute.glsl
        compute/particles.compute.glsl
        compute/skeleton-skinning.compute.glsl

        #PARENT_SCOPE
        )

set(RAYGEN_SHADERS
        rt/raytrace.rgen

        #PARENT_SCOPE
        )

set(RAYMISS_SHADERS
        rt/raytrace.rmiss
        rt/shadow.rmiss

        #PARENT_SCOPE
        )

set(RAYHIT_SHADERS
        rt/raytrace.rchit

        #PARENT_SCOPE
        )

set(ENGINE-SHADERS "" PARENT_SCOPE)
foreach(shader ${VERTEX_SHADERS})
    add_spirv_shader(vertex "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${CMAKE_BINARY_DIR}/resources/shaders/${shader}.spv")
endforeach()

foreach(shader ${FRAGMENT_SHADERS})
    add_spirv_shader(fragment "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${CMAKE_BINARY_DIR}/resources/shaders/${shader}.spv")
endforeach()

foreach(shader ${COMPUTE_SHADERS})
    add_spirv_shader(compute "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${CMAKE_BINARY_DIR}/resources/shaders/${shader}.spv")
endforeach()

foreach(shader ${RAYGEN_SHADERS})
    add_spirv_shader(rgen "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${CMAKE_BINARY_DIR}/resources/shaders/${shader}.spv")
endforeach()

foreach(shader ${RAYMISS_SHADERS})
    add_spirv_shader(rmiss "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${CMAKE_BINARY_DIR}/resources/shaders/${shader}.spv")
endforeach()

foreach(shader ${RAYHIT_SHADERS})
    add_spirv_shader(rchit "${shader}" "${shader}.spv")
    set(ENGINE-SHADERS "${ENGINE-SHADERS}" "${CMAKE_BINARY_DIR}/resources/shaders/${shader}.spv")
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

if (MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/bigobj>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")
endif()

set(ALL_BASE_LIBS
        CarrotCore fertilizer-lib
        Vulkan::Vulkan spirv-cross-core spirv-cross-glsl spirv-cross-reflect glfw glm assimp::assimp OpenAL SPIRV nfd ${LUA_LIBRARIES} OpenEXR::OpenEXR reactphysics3d
        ${OPENXR-LOADER-LIB-LOCATION}
        ${ADDITIONAL_LIBS})

message("ENGINE ADDITIONAL LIBS = " ${ALL_BASE_LIBS})

copy_all_resources()
file(COPY "resources/icon32.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "resources/icon64.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "resources/icon128.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "resources/icon128.ktx2" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "resources/splash.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")

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
