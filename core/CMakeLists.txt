set(CoreRoot "core/")
set(CORE-SOURCES

        ${CoreRoot}async/Coroutines.cpp
        ${CoreRoot}async/Counter.cpp
        ${CoreRoot}async/Executors.cpp
        ${CoreRoot}async/Locks.cpp
        ${CoreRoot}async/OSThreads.cpp

        ${CoreRoot}data/Hashes.cpp
        ${CoreRoot}data/ShaderMetadata.cpp

        ${CoreRoot}expressions/Expressions.cpp

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

        ${CoreRoot}io/windows/PlatformFileHandle.cpp

        ${CoreRoot}math/AABB.cpp
        ${CoreRoot}math/Plane.cpp
        ${CoreRoot}math/Segment2D.cpp
        ${CoreRoot}math/Sphere.cpp
        ${CoreRoot}math/Triangle.cpp

        ${CoreRoot}render/Skeleton.cpp
        ${CoreRoot}render/VertexTypes.cpp

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
        ${CoreRoot}utils/ImGuiUtils.cpp
        ${CoreRoot}utils/Profiling.cpp
        ${CoreRoot}utils/stringmanip.cpp
        ${CoreRoot}utils/UUID.cpp

        )

set(CORE-THIRDPARTY-SOURCES
        ${ProjectRoot}thirdparty/tracy/TracyClient.cpp
        ${ProjectRoot}thirdparty/MikkTSpace/mikktspace.c

        ${ProjectRoot}thirdparty/tribox3/tribox3.cpp
        )

# TODO: Seems like my Release build of Mono is broken?
#if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    file(GLOB MonoLibs "${ProjectRoot}thirdparty/mono/lib/${Arch}/Debug/mono-2.0-sgen.lib")
    file(GLOB MonoDLLs "${ProjectRoot}thirdparty/mono/bin/${Arch}/Debug/*.dll")
#else()
#    file(GLOB MonoLibs "${ProjectRoot}thirdparty/mono/lib/${Arch}/Release/mono-2.0-sgen.lib")
#    file(GLOB MonoDLLs "${ProjectRoot}thirdparty/mono/bin/${Arch}/Release/*.dll")
#endif()

function(add_core_includes Target)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}core/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui/misc/cpp/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/IconFontCppHeaders/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/tracy/)
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

    target_compile_definitions(${Target} PUBLIC GLM_FORCE_SWIZZLE=1)
    target_compile_definitions(${Target} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
    target_compile_definitions(${Target} PUBLIC RAPIDJSON_HAS_STDSTRING=1)
    target_compile_definitions("${Target}" PUBLIC TRACY_ENABLE=1)
    target_compile_definitions("${Target}" PUBLIC TRACY_ON_DEMAND=1)
    target_compile_definitions("${Target}" PUBLIC TRACY_FIBERS=1)
    target_link_libraries("${Target}" PUBLIC ${MonoLibs})

endfunction()

set(ALL_CORE_LIBS ktx glm tinygltf nfd cider)
add_library(CarrotCore ${CORE-SOURCES} ${CORE-THIRDPARTY-SOURCES})
add_core_includes(CarrotCore)
target_link_libraries(CarrotCore PUBLIC ${ALL_CORE_LIBS})

file(COPY ${MonoDLLs} DESTINATION ${CMAKE_BINARY_DIR})