set(EngineRoot "engine/")
set(ShadersRoot "${EngineRoot}resources/shaders")
set(ENGINE-SOURCES
        ${EngineRoot}Window.cpp

        ${EngineRoot}io/actions/Action.cpp
        ${EngineRoot}io/actions/ActionSet.cpp
        ${EngineRoot}io/AssimpCompatibilityLayer.cpp

        ${EngineRoot}CarrotRenderingPipeline.cpp
        ${EngineRoot}Engine.cpp
        ${EngineRoot}render/Model.cpp
        ${EngineRoot}render/ModelRenderer.cpp

        ${EngineRoot}render/resources/Buffer.cpp
        ${EngineRoot}render/resources/BufferAllocation.cpp
        ${EngineRoot}render/resources/BufferView.cpp
        ${EngineRoot}render/resources/DeviceMemory.cpp
        ${EngineRoot}render/resources/Image.cpp
        ${EngineRoot}render/resources/Font.cpp
        ${EngineRoot}render/resources/LightMesh.cpp
        ${EngineRoot}render/resources/Mesh.cpp
        ${EngineRoot}render/resources/ResourceAllocator.cpp
        ${EngineRoot}render/resources/SingleFrameStackGPUAllocator.cpp
        ${EngineRoot}render/resources/SingleMesh.cpp
        ${EngineRoot}render/resources/Texture.cpp
        ${EngineRoot}render/resources/Vertex.cpp
        ${EngineRoot}render/resources/VertexFormat.cpp

        ${EngineRoot}render/resources/model_loading/AssimpLoader.cpp
        ${EngineRoot}render/resources/model_loading/SceneLoader.cpp

        ${EngineRoot}render/shaders/ShaderModule.cpp
        ${EngineRoot}render/shaders/ShaderStages.cpp
        ${EngineRoot}render/shaders/ShaderSource.cpp
        ${EngineRoot}render/resources/Pipeline.cpp
        ${EngineRoot}render/shaders/Specialization.cpp

        ${EngineRoot}render/Camera.cpp
        ${EngineRoot}render/CameraBufferObject.cpp
        ${EngineRoot}render/InstanceData.cpp
        ${EngineRoot}render/RenderPacket.cpp

        ${EngineRoot}render/animation/AnimatedInstances.cpp
        ${EngineRoot}render/animation/AnimatedModel.cpp
        ${EngineRoot}render/animation/SkeletalModelRenderer.cpp

        ${EngineRoot}utils/conversions.cpp
        ${EngineRoot}utils/Macros.cpp

        ${EngineRoot}ecs/components/AnimatedModelComponent.cpp
        ${EngineRoot}ecs/components/BillboardComponent.cpp
        ${EngineRoot}ecs/components/CameraComponent.cpp
        ${EngineRoot}ecs/components/CSharpComponent.cpp
        ${EngineRoot}ecs/components/Component.cpp
        ${EngineRoot}ecs/components/LightComponent.cpp
        ${EngineRoot}ecs/components/LuaScriptComponent.cpp
        ${EngineRoot}ecs/components/ModelComponent.cpp
        ${EngineRoot}ecs/components/NavMeshComponent.cpp
        ${EngineRoot}ecs/components/PhysicsCharacterComponent.cpp
        ${EngineRoot}ecs/components/RigidBodyComponent.cpp
        ${EngineRoot}ecs/components/SoundListenerComponent.cpp
        ${EngineRoot}ecs/components/SpriteComponent.cpp
        ${EngineRoot}ecs/components/TextComponent.cpp
        ${EngineRoot}ecs/components/TransformComponent.cpp

        ${EngineRoot}ecs/systems/AnimatedModelRenderSystem.cpp
        ${EngineRoot}ecs/systems/BillboardSystem.cpp
        ${EngineRoot}ecs/systems/CameraSystem.cpp
        ${EngineRoot}ecs/systems/CSharpLogicSystem.cpp
        ${EngineRoot}ecs/systems/LuaSystems.cpp
        ${EngineRoot}ecs/systems/ModelRenderSystem.cpp
        ${EngineRoot}ecs/systems/PhysicsCharacterSystem.cpp
        ${EngineRoot}ecs/systems/System.cpp
        ${EngineRoot}ecs/systems/SystemHandleLights.cpp
        ${EngineRoot}ecs/systems/SystemKinematics.cpp
        ${EngineRoot}ecs/systems/SoundListenerSystem.cpp
        ${EngineRoot}ecs/systems/SpriteRenderSystem.cpp
        ${EngineRoot}ecs/systems/SystemSinPosition.cpp
        ${EngineRoot}ecs/systems/SystemTransformSwapBuffers.cpp
        ${EngineRoot}ecs/systems/TextRenderSystem.cpp
        ${EngineRoot}ecs/systems/RigidBodySystem.cpp

        ${EngineRoot}ecs/EntityTypes.cpp
        ${EngineRoot}ecs/Signature.cpp
        ${EngineRoot}ecs/World.cpp
        ${EngineRoot}ecs/WorldData.cpp

        ${EngineRoot}render/GBuffer.cpp
        ${EngineRoot}render/ClusterManager.cpp
        ${EngineRoot}render/VisibilityBuffer.cpp
        ${EngineRoot}render/raytracing/RayTracer.cpp
        ${EngineRoot}render/raytracing/ASBuilder.cpp
        ${EngineRoot}render/raytracing/AccelerationStructure.cpp
        ${EngineRoot}single_file_header_impls.cpp
        ${EngineRoot}LoadingScreen.cpp

        ${EngineRoot}assets/AssetServer.cpp

        ${EngineRoot}audio/AudioManager.cpp
        ${EngineRoot}audio/AudioThread.cpp
        ${EngineRoot}audio/OpenAL.cpp
        ${EngineRoot}audio/SFX.cpp
        ${EngineRoot}audio/SoundSource.cpp
        ${EngineRoot}audio/Sound.cpp
        ${EngineRoot}audio/SoundListener.cpp

        ${EngineRoot}audio/decoders/noDecoder.cpp
        ${EngineRoot}audio/decoders/MP3Decoder.cpp
        ${EngineRoot}audio/decoders/VorbisDecoder.cpp
        ${EngineRoot}audio/decoders/WavDecoder.cpp

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
        ${EngineRoot}render/ImGuiBackend.cpp
        ${EngineRoot}render/MaterialSystem.cpp
        ${EngineRoot}render/RenderContext.cpp
        ${EngineRoot}render/RenderPacketContainer.cpp
        ${EngineRoot}render/Sprite.cpp
        ${EngineRoot}render/TextureAtlas.cpp
        ${EngineRoot}render/VulkanRenderer.cpp
        ${EngineRoot}render/Viewport.cpp
        ${EngineRoot}render/ViewportBufferObject.cpp
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
        ${EngineRoot}scene/SceneManager.cpp

        ${EngineRoot}scripting/ComponentProperty.cpp
        ${EngineRoot}scripting/CSharpBindings.cpp
        ${EngineRoot}scripting/CSharpReflectionHelper.cpp
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

        ${EngineRoot}pathfinding/AStar.cpp
        ${EngineRoot}pathfinding/NavMesh.cpp
        ${EngineRoot}pathfinding/NavMeshBuilder.cpp
        ${EngineRoot}pathfinding/NavPath.cpp
        ${EngineRoot}pathfinding/SparseVoxelGrid.cpp

        ${EngineRoot}physics/Character.cpp
        ${EngineRoot}physics/Colliders.cpp
        ${EngineRoot}physics/CollisionLayers.cpp
        ${EngineRoot}physics/DebugRenderer.cpp
        ${EngineRoot}physics/PhysicsSystem.cpp
        ${EngineRoot}physics/RigidBody.cpp

        ${EngineRoot}task/TaskScheduler.cpp

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
        visibility-buffer.vertex.glsl
        screenQuad.vertex.glsl
        skybox.vertex.glsl
        particles.vertex.glsl
        screenQuad-transformed.vertex.glsl
        text-rendering.vertex.glsl
        gBufferSprite.vertex.glsl
        billboards.vertex.glsl
        gBufferWireframe.vertex.glsl
        gBufferWithBoneInfo.vertex.glsl

        imgui.vertex.glsl

        #PARENT_SCOPE
        )

set(FRAGMENT_SHADERS
        gBuffer.fragment.glsl
        gBuffer-transparent.fragment.glsl
        gBufferBlockout.fragment.glsl
        material-pass.fragment.glsl
        visibility-buffer.fragment.glsl
        unlitGBuffer.fragment.glsl

        blit.fragment.glsl
        skybox.fragment.glsl
        particles.fragment.glsl

        lighting-noraytracing.fragment.glsl
        lighting-raytracing.fragment.glsl

        composer-blit.fragment.glsl
        text-rendering.fragment.glsl
        gBufferSprite.fragment.glsl
        billboards.fragment.glsl

        forward-raytracing.fragment.glsl
        forward-noraytracing.fragment.glsl

        imgui.fragment.glsl

        post-process/temporal-denoise.fragment.glsl
        post-process/merge-lighting.fragment.glsl
        post-process/tone-mapping.fragment.glsl
        post-process/visibility-buffer-debug-view.fragment.glsl

        #PARENT_SCOPE
        )

set(COMPUTE_SHADERS
        compute/animation-skinning.compute.glsl
        compute/clear-visibility-buffer.compute.glsl
        compute/copy-variance.compute.glsl
        compute/firefly-rejection.compute.glsl
        compute/particles.compute.glsl
        compute/skeleton-skinning.compute.glsl
        compute/spatial-denoise.compute.glsl

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

set(ALL_BASE_LIBS
        CarrotCore fertilizer-lib
        Vulkan::Vulkan spirv-cross-core spirv-cross-glsl spirv-cross-reflect glfw glm assimp::assimp OpenAL SPIRV ${LUA_LIBRARIES} OpenEXR::OpenEXR Jolt
        OpenXR::headers
        OpenXR::openxr_loader
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
add_dependencies(Engine-Base generate_headers) # OpenXR-Hpp header generation

add_library(Engine-Base-Renderdoc ${ENGINE-SOURCES} ${ENGINE-SHADERS} ${THIRDPARTY-SOURCES})
target_link_libraries(Engine-Base-Renderdoc PUBLIC ${ALL_BASE_LIBS})
target_compile_definitions(Engine-Base-Renderdoc PRIVATE DEBUG_MARKERS=1)
add_includes(Engine-Base-Renderdoc)
add_dependencies(Engine-Base-Renderdoc generate_headers) # OpenXR-Hpp header generation

add_library(Engine-ASAN ${ENGINE-SOURCES} ${ENGINE-SHADERS} ${THIRDPARTY-SOURCES})
target_link_libraries(Engine-ASAN PUBLIC ${ALL_BASE_LIBS})
make_asan_target(Engine-ASAN)
add_includes(Engine-ASAN)
add_dependencies(Engine-ASAN generate_headers) # OpenXR-Hpp header generation

# Using this instead of REUSE_FROM, because REUSE_FROM seems to take forever to build/validate
function(add_engine_precompiled_headers Target)
    target_precompile_headers(${Target} PRIVATE
            <engine/vulkan/includes.h>
            <engine/vr/includes.h>
            <sol/sol.hpp>
            <glm/glm.hpp>
            <chrono>
            <unordered_map>
            <filesystem>
            <vector>
            <string>
            <Jolt/Jolt.h>
            )
endfunction()

add_engine_precompiled_headers(Engine-Base)
add_engine_precompiled_headers(Engine-Base-Renderdoc)
add_engine_precompiled_headers(Engine-ASAN)

# TODO: move to post-build
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB CarrotScriptDLLs "script-api/Carrot/bin/Debug/*.dll")
    file(GLOB CarrotScriptPDBs "script-api/Carrot/bin/Debug/*.pdb")
else()
    file(GLOB CarrotScriptDLLs "script-api/Carrot/bin/Release/*.dll")
    file(GLOB CarrotScriptPDBs "script-api/Carrot/bin/Release/*.pdb")
endif()

file(COPY ${CarrotScriptDLLs} DESTINATION "${CMAKE_BINARY_DIR}/scripting")
file(COPY ${CarrotScriptPDBs} DESTINATION "${CMAKE_BINARY_DIR}/scripting")