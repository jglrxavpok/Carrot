set(CoreRoot "core/")

set(CORE-SOURCES

        ${CoreRoot}allocators/MallocAllocator.cpp
        ${CoreRoot}allocators/LowLevelMemoryAllocations.cpp
        ${CoreRoot}allocators/StackAllocator.cpp
        ${CoreRoot}allocators/TrackingAllocator.cpp

        ${CoreRoot}async/Counter.cpp
        ${CoreRoot}async/Executors.cpp
        ${CoreRoot}async/Locks.cpp
        ${CoreRoot}async/OSThreads.cpp

        ${CoreRoot}data/Hashes.cpp
        ${CoreRoot}data/ParticleBlueprint.cpp
        ${CoreRoot}data/ShaderMetadata.cpp

        ${CoreRoot}expressions/Expressions.cpp

        ${CoreRoot}io/Document.cpp
        ${CoreRoot}io/FileHandle.cpp
        ${CoreRoot}io/Files.cpp
        ${CoreRoot}io/FileSystemOS.cpp
        ${CoreRoot}io/FileWatcher.cpp
        ${CoreRoot}io/IO.cpp
        ${CoreRoot}io/Logging.cpp
        ${CoreRoot}io/Path.cpp
        ${CoreRoot}io/Resource.cpp
        ${CoreRoot}io/Serialisation.cpp
        ${CoreRoot}io/Strings.cpp
        ${CoreRoot}io/vfs/VirtualFileSystem.cpp

        ${CoreRoot}io/linux/PlatformFileHandle.cpp
        ${CoreRoot}io/windows/PlatformFileHandle.cpp

        ${CoreRoot}math/AABB.cpp
        ${CoreRoot}math/Plane.cpp
        ${CoreRoot}math/Segment2D.cpp
        ${CoreRoot}math/Sphere.cpp
        ${CoreRoot}math/Triangle.cpp

        ${CoreRoot}render/ImageFormats.cpp
        ${CoreRoot}render/Skeleton.cpp
        ${CoreRoot}render/VertexTypes.cpp

        ${CoreRoot}scene/AssimpLoader.cpp
        ${CoreRoot}scene/GLTFLoader.cpp

        ${CoreRoot}scripting/csharp/CSAppDomain.cpp
        ${CoreRoot}scripting/csharp/CSArray.cpp
        ${CoreRoot}scripting/csharp/CSAssembly.cpp
        ${CoreRoot}scripting/csharp/CSClass.cpp
        ${CoreRoot}scripting/csharp/CSField.cpp
        ${CoreRoot}scripting/csharp/CSMethod.cpp
        ${CoreRoot}scripting/csharp/CSObject.cpp
        ${CoreRoot}scripting/csharp/CSProject.cpp
        ${CoreRoot}scripting/csharp/CSProperty.cpp
        ${CoreRoot}scripting/csharp/Engine.cpp

        ${CoreRoot}tasks/Tasks.cpp
        ${CoreRoot}tasks/Timer.cpp

        ${CoreRoot}utils/CarrotTinyGLTF.cpp
        ${CoreRoot}utils/Identifiable.cpp
        ${CoreRoot}utils/Identifier.cpp
        ${CoreRoot}utils/ImGuiUtils.cpp
        ${CoreRoot}utils/Profiling.cpp
        ${CoreRoot}utils/single_file_header_impls.cpp
        ${CoreRoot}utils/stringmanip.cpp
        ${CoreRoot}utils/TOML.cpp
        ${CoreRoot}utils/UserNotifications.cpp
        ${CoreRoot}utils/UUID.cpp

        )

set(CORE-THIRDPARTY-SOURCES
        ${ProjectRoot}thirdparty/tracy/public/TracyClient.cpp
        ${ProjectRoot}thirdparty/MikkTSpace/mikktspace.c

        ${ProjectRoot}thirdparty/tribox3/tribox3.cpp

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

# TODO: Seems like my Release build of Mono is broken?
#if(CMAKE_BUILD_TYPE STREQUAL "Debug")
if(WIN32)
    file(GLOB MonoLibs "${ProjectRoot}thirdparty/mono/lib/${Arch}/Windows/Debug/mono-2.0-sgen.lib")
    file(GLOB MonoDLLs "${ProjectRoot}thirdparty/mono/bin/${Arch}/Windows/Debug/*.dll")
else ()
    file(GLOB MonoLibs "${ProjectRoot}thirdparty/mono/lib/${Arch}/Linux/Debug/*.a")
    file(GLOB MonoDLLs "${ProjectRoot}thirdparty/mono/bin/${Arch}/Linux/Debug/*.so")
endif ()
#else()
#    file(GLOB MonoLibs "${ProjectRoot}thirdparty/mono/lib/${Arch}/Release/mono-2.0-sgen.lib")
#    file(GLOB MonoDLLs "${ProjectRoot}thirdparty/mono/bin/${Arch}/Release/*.dll")
#endif()

function(add_core_includes Target)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}core/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui-node-editor/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui/misc/cpp/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/IconFontCppHeaders/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/tracy/public/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/stb/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/rapidjson/include)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/rapidxml/rapidxml-1.13)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/stduuid/include)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/debugbreak)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/MikkTSpace)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/mono/include)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/eventpp/include)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/robin-hood-hashing/src/include/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/tribox3/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/cider/include/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/mimalloc/include/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/concurrentqueue/)

    target_compile_definitions(${Target} PUBLIC GLM_FORCE_SWIZZLE=1)
    target_compile_definitions(${Target} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
    target_compile_definitions(${Target} PUBLIC RAPIDJSON_HAS_STDSTRING=1)
    target_compile_definitions("${Target}" PUBLIC TRACY_ENABLE=1)
    target_compile_definitions("${Target}" PUBLIC TRACY_ON_DEMAND=1)
    target_compile_definitions("${Target}" PUBLIC TRACY_FIBERS=1)
    target_compile_definitions("${Target}" PUBLIC USE_PIX=1)
    target_compile_definitions("${Target}" PUBLIC GLM_ENABLE_EXPERIMENTAL=1)
    target_link_libraries("${Target}" PUBLIC ${MonoLibs})
    target_link_libraries("${Target}" PUBLIC mimalloc-static)
    target_link_libraries("${Target}" PUBLIC tomlplusplus::tomlplusplus)

    # METIS
    target_include_directories("${Target}" PUBLIC ${ProjectRoot}thirdparty/libMETIS/)

    if(WIN32)
        # PIX on Windows
        target_include_directories("${Target}" PUBLIC ${ProjectRoot}thirdparty/WinPixEventRuntime/)
        target_link_libraries("${Target}" PUBLIC ${ProjectRoot}thirdparty/WinPixEventRuntime/WinPixEventRuntime.lib)

        # METIS libs
        target_link_libraries("${Target}" PUBLIC ${ProjectRoot}thirdparty/libMETIS/GKlib.lib ${ProjectRoot}thirdparty/libMETIS/metis.lib)
    else()
        # GKlib is added twice to ensure CMake puts GKlib before metis during link, otherwise metis does not find symbols of GKlib
        target_link_libraries("${Target}" PUBLIC ${ProjectRoot}thirdparty/libMETIS/libGKlib.a ${ProjectRoot}thirdparty/libMETIS/libmetis.a ${ProjectRoot}thirdparty/libMETIS/libGKlib.a)

        # https://en.cppreference.com/w/cpp/algorithm/execution_policy_tag.html
        # GCC depends on an external dependency to use <execution> ??
        target_link_libraries("${Target}" PUBLIC tbb)
    endif()


endfunction()

set(ALL_CORE_LIBS Vulkan::Vulkan ktx glm tinygltf nfd cider assimp::assimp spirv-cross-core spirv-cross-glsl spirv-cross-reflect glm glslang SPIRV)
add_library(CarrotCore STATIC ${CORE-SOURCES} ${CORE-THIRDPARTY-SOURCES})
add_core_includes(CarrotCore)
target_link_libraries(CarrotCore PUBLIC ${ALL_CORE_LIBS})
add_core_precompiled_headers(CarrotCore)

add_library(CarrotCore-ASAN STATIC ${CORE-SOURCES} ${CORE-THIRDPARTY-SOURCES})
add_core_includes(CarrotCore-ASAN)
target_link_libraries(CarrotCore-ASAN PUBLIC ${ALL_CORE_LIBS})
add_core_precompiled_headers(CarrotCore-ASAN)
make_asan_target(CarrotCore-ASAN)

file(COPY ${MonoDLLs} DESTINATION ${CMAKE_BINARY_DIR})
file(COPY ${ProjectRoot}thirdparty/WinPixEventRuntime/WinPixEventRuntime.dll DESTINATION ${CMAKE_BINARY_DIR})