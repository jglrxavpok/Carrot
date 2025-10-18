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
        ${EngineRoot}render/resources/SemaphorePool.cpp
        ${EngineRoot}render/resources/SingleFrameStackGPUAllocator.cpp
        ${EngineRoot}render/resources/SingleMesh.cpp
        ${EngineRoot}render/resources/Texture.cpp
        ${EngineRoot}render/resources/Vertex.cpp
        ${EngineRoot}render/resources/VertexFormat.cpp

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

        ${EngineRoot}ecs/components/AnimatedModelComponent.cpp
        ${EngineRoot}ecs/components/BillboardComponent.cpp
        ${EngineRoot}ecs/components/CameraComponent.cpp
        ${EngineRoot}ecs/components/CSharpComponent.cpp
        ${EngineRoot}ecs/components/Component.cpp
        ${EngineRoot}ecs/components/ErrorComponent.cpp
        ${EngineRoot}ecs/components/LightComponent.cpp
        ${EngineRoot}ecs/components/MissingComponent.cpp
        ${EngineRoot}ecs/components/ModelComponent.cpp
        ${EngineRoot}ecs/components/NavMeshComponent.cpp
        ${EngineRoot}ecs/components/ParticleEmitterComponent.cpp
        ${EngineRoot}ecs/components/PhysicsCharacterComponent.cpp
        ${EngineRoot}ecs/components/PrefabInstanceComponent.cpp
        ${EngineRoot}ecs/components/RigidBodyComponent.cpp
        ${EngineRoot}ecs/components/SoundListenerComponent.cpp
        ${EngineRoot}ecs/components/SpriteComponent.cpp
        ${EngineRoot}ecs/components/TextComponent.cpp
        ${EngineRoot}ecs/components/TransformComponent.cpp

        ${EngineRoot}ecs/systems/AnimatedModelRenderSystem.cpp
        ${EngineRoot}ecs/systems/BillboardSystem.cpp
        ${EngineRoot}ecs/systems/CameraSystem.cpp
        ${EngineRoot}ecs/systems/CSharpLogicSystem.cpp
        ${EngineRoot}ecs/systems/ModelRenderSystem.cpp
        ${EngineRoot}ecs/systems/PhysicsCharacterSystem.cpp
        ${EngineRoot}ecs/systems/System.cpp
        ${EngineRoot}ecs/systems/SystemHandleLights.cpp
        ${EngineRoot}ecs/systems/SystemKinematics.cpp
        ${EngineRoot}ecs/systems/SystemParticles.cpp
        ${EngineRoot}ecs/systems/SoundListenerSystem.cpp
        ${EngineRoot}ecs/systems/SpriteRenderSystem.cpp
        ${EngineRoot}ecs/systems/SystemSinPosition.cpp
        ${EngineRoot}ecs/systems/SystemTransformSwapBuffers.cpp
        ${EngineRoot}ecs/systems/TextRenderSystem.cpp
        ${EngineRoot}ecs/systems/RigidBodySystem.cpp

        ${EngineRoot}ecs/EntityTypes.cpp
        ${EngineRoot}ecs/Prefab.cpp
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
        ${EngineRoot}Settings.cpp

        ${EngineRoot}assets/AssetServer.cpp

        ${EngineRoot}audio/AudioManager.cpp
        ${EngineRoot}audio/AudioThread.cpp
        ${EngineRoot}audio/Music.cpp
        ${EngineRoot}audio/OpenAL.cpp
        ${EngineRoot}audio/SFX.cpp
        ${EngineRoot}audio/SoundSource.cpp
        ${EngineRoot}audio/Sound.cpp
        ${EngineRoot}audio/SoundListener.cpp

        ${EngineRoot}audio/decoders/NoDecoder.cpp
        ${EngineRoot}audio/decoders/MP3Decoder.cpp
        ${EngineRoot}audio/decoders/VorbisDecoder.cpp
        ${EngineRoot}audio/decoders/WavDecoder.cpp

        ${EngineRoot}render/particles/ParticleSystem.cpp
        ${EngineRoot}render/particles/ParticleEmitter.cpp
        ${EngineRoot}render/particles/RenderableParticleBlueprint.cpp

        ${EngineRoot}render/RenderGraph.cpp
        ${EngineRoot}render/RenderPass.cpp
        ${EngineRoot}render/Composer.cpp
        ${EngineRoot}render/TextureRepository.cpp

        ${EngineRoot}render/ComputePipeline.cpp

        ${EngineRoot}render/AnimatedSprite.cpp
        ${EngineRoot}render/BillboardRenderer.cpp
        ${EngineRoot}render/DebugRenderer.cpp
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
        ${EngineRoot}render/lighting/LightingPasses.cpp

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

        ${EngineRoot}scripting/bindings/ModelBindings.cpp
        ${EngineRoot}scripting/bindings/PrefabBindings.cpp

        ${EngineRoot}edition/DragDropTypes.cpp
        ${EngineRoot}edition/EditorSettings.cpp
        ${EngineRoot}edition/FreeCameraController.cpp
        ${EngineRoot}edition/ProjectMenuHolder.cpp
        ${EngineRoot}edition/Widgets.cpp

        ${EngineRoot}pathfinding/AStar.cpp
        ${EngineRoot}pathfinding/NavMesh.cpp
        ${EngineRoot}pathfinding/NavMeshBuilder.cpp
        ${EngineRoot}pathfinding/NavPath.cpp
        ${EngineRoot}pathfinding/SparseVoxelGrid.cpp

        ${EngineRoot}physics/Character.cpp
        ${EngineRoot}physics/Colliders.cpp
        ${EngineRoot}physics/CollisionLayers.cpp
        ${EngineRoot}physics/JoltDebugRenderer.cpp
        ${EngineRoot}physics/PhysicsBody.cpp
        ${EngineRoot}physics/PhysicsSystem.cpp
        ${EngineRoot}physics/RigidBody.cpp

        ${EngineRoot}task/TaskScheduler.cpp

        ${EngineRoot}vulkan/CustomTracyVulkan.cpp
        ${EngineRoot}vulkan/DebugNameable.cpp
        ${EngineRoot}vulkan/SynchronizedQueue.cpp
        ${EngineRoot}vulkan/VulkanDriver.cpp

        ${EngineRoot}utils/AftermathIntegration.cpp
        ${EngineRoot}utils/conversions.cpp
        ${EngineRoot}utils/Macros.cpp
        ${EngineRoot}utils/Time.cpp

        # VR specific code
        ${EngineRoot}vr/HandTracking.cpp
        ${EngineRoot}vr/Session.cpp
        ${EngineRoot}vr/VRInterface.cpp
        )

set(PIPELINES
        compute/animation-skinning
        compute/clear-singleint
        compute/clear-visibility-buffer
        compute/compute-rt-clusters
        compute/copy-variance
        compute/debug-buffer-viewer
        compute/debug-texture-viewer

        lighting/gi/accumulate-radiance
        lighting/gi/apply-gi
        lighting/gi/debug-gi
        lighting/gi/decay-cells
        lighting/gi/gi-template
        lighting/gi/reorder-rays
        lighting/gi/spawn-rays
        lighting/gi/spawn-screen-probes
        lighting/gi/trace-rays

        lighting/ao
        lighting/ao-noraytracing
        lighting/ao-raytracing
        lighting/denoise-ao
        lighting/denoise-base
        lighting/denoise-direct
        lighting/direct
        lighting/direct-noraytracing
        lighting/direct-raytracing
        lighting/reflections
        lighting/reflections-noraytracing
        lighting/reflections-raytracing
        lighting/temporal-denoise

        post-process/visibility-buffer-debug/_template
        post-process/visibility-buffer-debug/clusters
        post-process/visibility-buffer-debug/lods
        post-process/visibility-buffer-debug/screen-error
        post-process/visibility-buffer-debug/triangles

        post-process/_post-process
        post-process/merge-lighting
        post-process/temporal-denoise
        post-process/tone-mapping

        billboards
        blit
        composer-blit
        forward-noraytracing
        forward-raytracing
        gBuffer
        gBuffer-transparent
        gBufferBlockout
        gBufferLines
        gBufferSprite
        gBufferWireframe
        imgui
        material-pass
        particles
        text-rendering
        unlitGBuffer
        unlitGBufferWireframe
        visibility-buffer

        #PARENT_SCOPE
)

set(ENGINE-PIPELINES "" PARENT_SCOPE)
foreach(pipeline_name ${PIPELINES})
    add_pipeline("${pipeline_name}")
    set(ENGINE-PIPELINES "${ENGINE-PIPELINES}" "${CMAKE_BINARY_DIR}/resources/pipelines/${pipeline_name}.pipeline")
endforeach()

set(THIRDPARTY-SOURCES
        ${ProjectRoot}thirdparty/imgui/backends/imgui_impl_vulkan.cpp
        ${ProjectRoot}thirdparty/imgui/backends/imgui_impl_glfw.cpp
        )

set(ALL_BASE_LIBS
        CarrotCore fertilizer-lib
        Vulkan::Vulkan spirv-cross-core spirv-cross-glsl spirv-cross-reflect glfw glm OpenAL SPIRV Jolt
        OpenXR::headers
        OpenXR::openxr_loader
        OpenEXR::OpenEXR
        gvc
        gvplugin_dot_layout
        gvplugin_core
        ${ADDITIONAL_LIBS})

add_dependencies(CarrotCore
        OpenXR::headers
        OpenXR::openxr_loader
        generate_headers)

message("ENGINE ADDITIONAL LIBS = " ${ALL_BASE_LIBS})

copy_all_resources()
file(COPY "resources/icon32.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "resources/icon64.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "resources/icon128.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "resources/icon128.ktx2" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
file(COPY "resources/splash.png" DESTINATION "${CMAKE_BINARY_DIR}/resources/")

add_library(Engine-Base ${ENGINE-SOURCES} ${ENGINE-PIPELINES} ${THIRDPARTY-SOURCES})
target_link_libraries(Engine-Base PUBLIC ${ALL_BASE_LIBS})
add_includes(Engine-Base)
add_dependencies(Engine-Base generate_headers) # OpenXR-Hpp header generation

add_library(Engine-ASAN ${ENGINE-SOURCES} ${ENGINE-PIPELINES} ${THIRDPARTY-SOURCES})
target_link_libraries(Engine-ASAN PUBLIC ${ALL_BASE_LIBS})
make_asan_target(Engine-ASAN)
add_includes(Engine-ASAN)
add_dependencies(Engine-ASAN generate_headers) # OpenXR-Hpp header generation

# Using this instead of REUSE_FROM, because REUSE_FROM seems to take forever to build/validate
function(add_engine_precompiled_headers Target)
    add_core_precompiled_headers(${Target})
    target_precompile_headers(${Target} PRIVATE
            <engine/vulkan/includes.h>
            <engine/vr/includes.h>
            <Jolt/Jolt.h>
            )
endfunction()

add_engine_precompiled_headers(Engine-Base)
add_engine_precompiled_headers(Engine-ASAN)

# SteamDeck support
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_BINARY_DIR})
    SET(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
    SET(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
    include(InstallRequiredSystemLibraries)

    add_custom_target(CopyRuntimeLibs
            COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} "${CMAKE_BINARY_DIR}"
            )
    add_dependencies(Engine-Base CopyRuntimeLibs)
endif()

# Post build copies
add_custom_command(TARGET Engine-Base POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
        "$<TARGET_SONAME_FILE:ktx>"
        "${PROJECT_BINARY_DIR}/lib/$<TARGET_SONAME_FILE_NAME:ktx>"
)
add_custom_command(TARGET Engine-Base POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
        "$<TARGET_SONAME_FILE:OpenXR::openxr_loader>"
        "${PROJECT_BINARY_DIR}/lib/$<TARGET_SONAME_FILE_NAME:OpenXR::openxr_loader>"
)
add_custom_command(TARGET Engine-Base POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy
        "$<TARGET_SONAME_FILE:OpenAL::OpenAL>"
        "${PROJECT_BINARY_DIR}/lib/$<TARGET_SONAME_FILE_NAME:OpenAL::OpenAL>"
)

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
file(COPY "${EngineRoot}../resources/default-imgui.ini" DESTINATION "${CMAKE_BINARY_DIR}/")