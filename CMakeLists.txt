cmake_minimum_required(VERSION 3.17)
project(Carrot)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)

list(APPEND CMAKE_PREFIX_PATH "cmake_modules/")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(Arch "x64")
set(Platform "win64") # TODO: auto-detect
set(ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}/")

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(OPENXR-LOADER-LIB openxr_loader)
    set(OPENXR-LOADER-LIB-LOCATION "${ProjectRoot}thirdparty/OpenXR-SDK-Source/build/${Platform}/src/loader/Release/${OPENXR-LOADER-LIB}.lib")
    set(IS_RELEASE_BUILD True)
    add_compile_definitions(IS_RELEASE_BUILD)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(OPENXR-LOADER-LIB openxr_loaderd)
    set(OPENXR-LOADER-LIB-LOCATION "${ProjectRoot}thirdparty/OpenXR-SDK-Source/build/${Platform}/src/loader/Debug/${OPENXR-LOADER-LIB}.lib")
    set(IS_DEBUG_BUILD True)
    add_compile_definitions(IS_DEBUG_BUILD)
endif()

set(OPENXR-INCLUDE-DIR ${ProjectRoot}thirdparty/OpenXR-SDK-Source/build/${Platform}/include)

include(FetchContent)
find_package(Vulkan REQUIRED)
find_package(Aftermath)
find_package(Lua REQUIRED)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ALSOFT_NO_CONFIG_UTIL ON CACHE BOOL "" FORCE)
set(ALSOFT_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ALSOFT_UTILS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENABLE_CTEST OFF CACHE BOOL "" FORCE)

set(CMAKE_DEBUG_POSTFIX "")
set(OPENEXR_FORCE_INTERNAL_IMATH ON CACHE BOOL "" FORCE)

SET(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT FALSE)
SET(ASSIMP_BUILD_FBX_IMPORTER TRUE)
SET(ASSIMP_BUILD_OBJ_IMPORTER TRUE)
SET(ASSIMP_BUILD_M3D_IMPORTER TRUE)

SET(ASSIMP_BUILD_M3D_EXPORTER TRUE)
SET(ASSIMP_BUILD_ASSBIN_EXPORTER TRUE)
SET(ASSIMP_BUILD_ASSXML_EXPORTER TRUE)
SET(ASSIMP_BUILD_GLTF_EXPORTER FALSE)
SET(ASSIMP_BUILD_OBJ_EXPORTER FALSE)
SET(ASSIMP_BUILD_OPENGEX_EXPORTER FALSE)
SET(ASSIMP_BUILD_PLY_EXPORTER FALSE)
SET(ASSIMP_BUILD_3DS_EXPORTER FALSE)
SET(ASSIMP_BUILD_COLLADA_EXPORTER FALSE)
SET(ASSIMP_BUILD_FBX_EXPORTER FALSE)
SET(ASSIMP_BUILD_STL_EXPORTER FALSE)
SET(ASSIMP_BUILD_X_EXPORTER FALSE)
SET(ASSIMP_BUILD_X3D_EXPORTER FALSE)
SET(ASSIMP_BUILD_3MF_EXPORTER FALSE)
SET(ASSIMP_BUILD_ASSJSON_EXPORTER FALSE)
SET(ASSIMP_BUILD_STEP_EXPORTER FALSE)

add_subdirectory(thirdparty/glfw-3.3.2/)
add_subdirectory(thirdparty/glm-0.9.9.8/glm/)

add_subdirectory(thirdparty/assimp)

add_subdirectory(thirdparty/nativefiledialog-extended)
add_subdirectory(thirdparty/openexr)
add_subdirectory(thirdparty/reactphysics3d)

FetchContent_Declare(spirv_cross
        GIT_REPOSITORY      https://github.com/KhronosGroup/SPIRV-Cross.git
        GIT_TAG             e9cc6403341baf0edd430a4027b074d0a06b782f)
FetchContent_MakeAvailable(spirv_cross)
add_subdirectory(thirdparty/glslang)

FetchContent_Declare(openal-soft
        GIT_REPOSITORY      https://github.com/kcat/openal-soft.git
        GIT_TAG             1.21.1)
FetchContent_MakeAvailable(openal-soft)

function(add_includes Target)
    add_core_includes(${Target})
    target_include_directories(${Target} PUBLIC ${ProjectRoot}engine/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/tracy/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui/misc/cpp)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/dr_libs/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui-node-editor/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/asio/asio/include/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/sol3/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/VulkanMemoryAllocator-Hpp)
    target_include_directories(${Target} PUBLIC ${LUA_INCLUDE_DIR})
    target_link_directories(${Target} PUBLIC ${OpenAL_BINARY_DIR})

    target_compile_definitions(${Target} PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE=1)
    target_compile_definitions(${Target} PUBLIC RAPIDJSON_HAS_STDSTRING=1)
    if(IS_DEBUG_BUILD)
        target_compile_definitions("${Target}" PRIVATE TRACY_ENABLE=1)
    endif()
endfunction()

add_subdirectory(core)
add_subdirectory(asset_tools)

function(add_spirv_shader SHADER_STAGE INPUT_FILE OUTPUT_FILE)
    add_custom_command(
            OUTPUT "${CMAKE_BINARY_DIR}/resources/shaders/${OUTPUT_FILE}"
            COMMAND shadercompiler "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/${INPUT_FILE}" "${CMAKE_BINARY_DIR}/resources/shaders/${OUTPUT_FILE}" "${SHADER_STAGE}"
            COMMENT "Compiling shader ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/${INPUT_FILE}"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/${INPUT_FILE}"
            DEPENDS shadercompiler
    )
endfunction()

function(copy_resources TYPE)
    set(input ${CMAKE_CURRENT_SOURCE_DIR}/resources/${TYPE})
    if(EXISTS ${input})
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/${TYPE}" DESTINATION "${CMAKE_BINARY_DIR}/resources/")
        message("Copying ${input} next to binary output")
    else()
        message("No folder at ${input}, not copying.")
    endif()
endfunction()

function(copy_all_resources)
    copy_resources(textures)
    copy_resources(textures/icons)
    copy_resources(models)
    copy_resources(pipelines)
    copy_resources(materials)
    copy_resources(sounds)
    copy_resources(musics)
    copy_resources(node_templates)
    copy_resources(particles)
    copy_resources(fonts)
    copy_resources(shaders)
endfunction()

if(EXISTS "${OPENXR-LOADER-LIB-LOCATION}")
    set(CAN_SUPPORT_VR true)
    file(COPY "${OPENXR-LOADER-LIB-LOCATION}" DESTINATION "${CMAKE_BINARY_DIR}/")
else()
    set(CAN_SUPPORT_VR false)
endif()

function(make_asan_target Target)
    if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        #
        #   Requires manual copy of ASAN dll inside output directory (next to final executable file)
        #
        set(ASSIMP_ASAN ON)
        target_compile_options("${Target}" PRIVATE -fsanitize=address) # /MD will be used implicitly
        target_link_libraries("${Target}" PRIVATE clang_rt.asan_dynamic-x86_64 clang_rt.asan_dynamic_runtime_thunk-x86_64)
        target_link_options("${Target}" PRIVATE /wholearchive:clang_rt.asan_dynamic_runtime_thunk-x86_64.lib)
    endif()
endfunction()

add_subdirectory(engine)
add_subdirectory(samples)
add_subdirectory(engine/tools)
add_subdirectory(editor)
add_subdirectory(tests)

if(AFTERMATH_LIBS)
    set(AFTERMATH_SOURCES
            engine/engine/utils/AftermathIntegration.cpp)
    add_executable(Carrot-GPUDebug ${ALL_SOURCES} ${AFTERMATH_SOURCES})
    target_include_directories(Carrot-GPUDebug PRIVATE ${AFTERMATH_INCLUDES})
    target_compile_definitions(Carrot-GPUDebug PRIVATE AFTERMATH_ENABLE=1)
    target_compile_definitions(Carrot-GPUDebug PRIVATE GPU_ASSISTED_VALIDATION=1)
    target_compile_definitions(Carrot-GPUDebug PRIVATE DEBUG_MARKERS=1)
    target_link_libraries(Carrot-GPUDebug ${ALL_BASE_LIBS} ${AFTERMATH_LIBS})
else()
    message("Could not find Aftermath SDK")
endif()