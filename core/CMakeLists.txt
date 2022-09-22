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
        ${CoreRoot}io/vfs/VirtualFileSystem.cpp

        ${CoreRoot}math/Segment2D.cpp

        ${CoreRoot}tasks/Tasks.cpp
        ${CoreRoot}tasks/Timer.cpp

        ${CoreRoot}utils/Identifiable.cpp
        ${CoreRoot}utils/stringmanip.cpp
        ${CoreRoot}utils/UUID.cpp

        )

function(add_core_includes Target)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}core/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/stb/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/rapidjson/include)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/stduuid/include)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/debugbreak)
endfunction()

set(ALL_CORE_LIBS glm)
add_library(CarrotCore ${CORE-SOURCES})
add_core_includes(CarrotCore)
target_link_libraries(CarrotCore ${ALL_CORE_LIBS})