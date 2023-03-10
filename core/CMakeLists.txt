set(CoreRoot "core/")
set(CORE-SOURCES

        ${CoreRoot}async/Coroutines.cpp
        ${CoreRoot}async/Counter.cpp
        ${CoreRoot}async/Executors.cpp
        ${CoreRoot}async/Locks.cpp
        ${CoreRoot}async/OSThreads.cpp

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

        ${CoreRoot}math/Segment2D.cpp

        ${CoreRoot}render/Skeleton.cpp
        ${CoreRoot}render/VertexTypes.cpp

        ${CoreRoot}scene/GLTFLoader.cpp

        ${CoreRoot}scripting/csharp/Engine.cpp
        ${CoreRoot}scripting/csharp/Module.cpp

        ${CoreRoot}tasks/Tasks.cpp
        ${CoreRoot}tasks/Timer.cpp

        ${CoreRoot}utils/CarrotTinyGLTF.cpp
        ${CoreRoot}utils/Identifiable.cpp
        ${CoreRoot}utils/Profiling.cpp
        ${CoreRoot}utils/stringmanip.cpp
        ${CoreRoot}utils/UUID.cpp

        )

set(CORE-THIRDPARTY-SOURCES
        ${ProjectRoot}thirdparty/tracy/TracyClient.cpp
        ${ProjectRoot}thirdparty/MikkTSpace/mikktspace.c
        )

function(add_core_includes Target)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}core/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/tracy/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/stb/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/rapidjson/include)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/stduuid/include)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/debugbreak)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/MikkTSpace)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/mono/include)

    target_compile_definitions(${Target} PUBLIC GLM_FORCE_SWIZZLE=1)
    target_compile_definitions(${Target} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
    target_compile_definitions(${Target} PUBLIC RAPIDJSON_HAS_STDSTRING=1)
    target_compile_definitions("${Target}" PUBLIC TRACY_ENABLE=1)
    target_compile_definitions("${Target}" PUBLIC TRACY_ON_DEMAND=1)

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        #target_link_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/mono/lib/${Arch}/Debug/)
        file(GLOB MonoLibs "${ProjectRoot}thirdparty/mono/lib/${Arch}/Debug/mono-2.0-sgen.lib")
        file(GLOB MonoDLLs "${ProjectRoot}thirdparty/mono/bin/${Arch}/Debug/*.dll")
    else()
        #target_link_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/mono/lib/${Arch}/Release/)
        file(GLOB MonoLibs "${ProjectRoot}thirdparty/mono/lib/${Arch}/Release/mono-2.0-sgen.lib")
        file(GLOB MonoDLLs "${ProjectRoot}thirdparty/mono/bin/${Arch}/Release/*.dll")
    endif()

    file(COPY ${MonoDLLs} DESTINATION ${CMAKE_BINARY_DIR})
    target_link_libraries("${Target}" PUBLIC ${MonoLibs})

endfunction()

set(ALL_CORE_LIBS ktx glm tinygltf)
add_library(CarrotCore ${CORE-SOURCES} ${CORE-THIRDPARTY-SOURCES})
add_core_includes(CarrotCore)
target_link_libraries(CarrotCore PUBLIC ${ALL_CORE_LIBS})