# If using Ninja, use -d keepdepfile to ensure JetLive works properly!

cmake_minimum_required(VERSION 3.22)
project(Carrot)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) # required for JetLive

list(APPEND CMAKE_PREFIX_PATH "cmake_modules/")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

set(Arch "x64")

if(WIN32)
    set(Platform "Windows")
else()
    set(Platform "Linux")
endif()
set(ProjectRoot "${CMAKE_CURRENT_SOURCE_DIR}/")

# Hot reloading support
set(HAS_JETLIVE false)
set(HAS_LIVEPP false)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Under MSVC, attempt to use Live++
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
    if(EXISTS "${ProjectRoot}LivePP/API/x64/LPP_API.h")
        set(HAS_LIVEPP true)
        file(COPY ${ProjectRoot}LivePP/ DESTINATION LivePP/)
        message("Live++ support activated.")
    else()
        set(HAS_LIVEPP false)
        message("No Live++ support.")
    endif()
else()
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        # otherwise, in debug, attempt to use jet-live
        set(HAS_JETLIVE true)
        include(thirdparty/jet-live/cmake/jet_live_setup.cmake)
        set(JET_LIVE_BUILD_EXAMPLE OFF)
        set(JET_LIVE_SHARED OFF)
        add_subdirectory(thirdparty/jet-live)
    endif ()
endif ()

if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo" OR CMAKE_BUILD_TYPE STREQUAL "Release")
    set(IS_RELEASE_BUILD True)
    add_compile_definitions(IS_RELEASE_BUILD)
elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(IS_DEBUG_BUILD True)
    add_compile_definitions(IS_DEBUG_BUILD)
endif()

include(FetchContent)
find_package(Vulkan REQUIRED)

find_package(NsightAftermath)
if(NsightAftermath_FOUND)
    message("Found Aftermath SDK")
    set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} NsightAftermath::NsightAftermath)
    message("ADDITIONAL_LIBS = " ${ADDITIONAL_LIBS})

    get_filename_component(AFTERMATH_LIB_NAME ${NsightAftermath_LIBRARIES} NAME_WLE)
    get_filename_component(AFTERMATH_LIB_PATH ${NsightAftermath_LIBRARIES} PATH)
    set(DLL_PATH "${AFTERMATH_LIB_PATH}/${AFTERMATH_LIB_NAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
    file(COPY ${DLL_PATH} DESTINATION "${CMAKE_BINARY_DIR}")
endif()

file(COPY "${ProjectRoot}thirdparty/mono-install/${Platform}/" DESTINATION "${CMAKE_BINARY_DIR}/mono-install/")
if(NOT WIN32)
    file(COPY "${ProjectRoot}thirdparty/mono/bin/${Arch}/Linux/Debug/" DESTINATION "${CMAKE_BINARY_DIR}/mono-install/lib")
endif ()


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
set(OPENEXR_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(OPENEXR_INSTALL_EXAMPLES OFF CACHE BOOL "" FORCE)

SET(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT FALSE)
SET(ASSIMP_BUILD_FBX_IMPORTER TRUE)
SET(ASSIMP_BUILD_OBJ_IMPORTER TRUE)
SET(ASSIMP_BUILD_M3D_IMPORTER TRUE)
SET(ASSIMP_BUILD_GLTF_IMPORTER TRUE)

SET(ASSIMP_BUILD_M3D_EXPORTER FALSE)
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

set(KTX_FEATURE_TESTS OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_STATIC_LIBRARY OFF CACHE BOOL "" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "" FORCE)

set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANG_RHI OFF CACHE BOOL "" FORCE)
set(SLANG_ENABLE_SLANGRT OFF CACHE BOOL "" FORCE)

add_subdirectory(thirdparty/KTX-Software)

add_subdirectory(thirdparty/glfw-3.3.2/)
add_subdirectory(thirdparty/glm-0.9.9.8/glm/)

add_subdirectory(thirdparty/assimp)

add_subdirectory(thirdparty/nativefiledialog-extended)
add_subdirectory(thirdparty/openexr)

set(USE_STATIC_MSVC_RUNTIME_LIBRARY OFF)
set(CPP_RTTI_ENABLED ON)
add_subdirectory(thirdparty/JoltPhysics/Build)
target_compile_definitions(Jolt PUBLIC "$<$<CONFIG:RelWithDebInfo>:NDEBUG;JPH_PROFILE_ENABLED;JPH_DEBUG_RENDERER>")

set(TINYGLTF_HEADER_ONLY ON CACHE INTERNAL "" FORCE)
set(TINYGLTF_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory(thirdparty/tinygltf)

add_subdirectory(thirdparty/SPIRV-Cross)
set(ENABLE_OPT 0)
add_subdirectory(thirdparty/glslang)

FetchContent_Declare(openal-soft
        GIT_REPOSITORY      https://github.com/kcat/openal-soft.git
        GIT_TAG             1.24.1)
FetchContent_MakeAvailable(openal-soft)

add_subdirectory(thirdparty/cider)
add_subdirectory(thirdparty/meshoptimizer)

# OpenXR config & projects
set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/OpenXR-SDK-Source)

set(OPENXR_SDK_SRC_DIR ${ProjectRoot}thirdparty/OpenXR-SDK-Source)
set(OPENXR_SDK_BUILD_DIR ${CMAKE_BINARY_DIR}/thirdparty/OpenXR-SDK-Source)
add_subdirectory(thirdparty/OpenXR-Hpp)

set(MI_OVERRIDE OFF CACHE BOOL "" FORCE)
set(MI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(thirdparty/mimalloc)
add_subdirectory(thirdparty/bc7enc_rdo)
add_subdirectory(thirdparty/tomlplusplus)

# Without this, on Linux, libslang.so conflicts with another dynamic library of the same name, but used for networking
#  this makes slangc crash on init with this error:
# symbol lookup error: /usr/local/bin/slangc: undefined symbol: _Z29slang_createGlobalSessionImplPK22SlangGlobalSessionDescPKN5Slang25GlobalSessionInternalDescEPPN5slang14IGlobalSessionE
set(SLANG_LIB_TYPE STATIC)
set(SLANG_ENABLE_SLANG_GLSLANG FALSE)
set(SLANG_ENABLE_EXAMPLES FALSE)
set(SLANG_ENABLE_REPLAYER FALSE)
set(SLANG_ENABLE_TESTS FALSE)
set(SLANG_ENABLE_SLANGC FALSE)
set(SLANG_ENABLE_SLANGI FALSE)
set(SLANG_ENABLE_GFX FALSE)
add_subdirectory(thirdparty/slang)

function(add_includes Target)
    add_core_includes(${Target})
    target_include_directories(${Target} PUBLIC ${ProjectRoot}engine/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui/misc/cpp/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/dr_libs/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/imgui-node-editor/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/asio/asio/include/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/VulkanMemoryAllocator/include/)
    target_include_directories(${Target} PUBLIC ${ProjectRoot}thirdparty/robin-hood-hashing/src/include/)
    target_include_directories(${Target} PUBLIC ${LUA_INCLUDE_DIR})
    target_include_directories(${Target} PUBLIC ${OpenXR_INCLUDE_DIR})
    target_include_directories(${Target} PUBLIC ${CMAKE_BINARY_DIR}/thirdparty/OpenXR-Hpp/include/) # generated by 'generate_headers'
    target_link_directories(${Target} PUBLIC ${OpenAL_BINARY_DIR})

    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_link_options(${Target} PRIVATE "/FUNCTIONPADMIN" "/SAFESEH:NO" "/OPT:NOREF" "/OPT:NOICF" "/DEBUG:FULL")
        if(HAS_LIVEPP)
            target_compile_definitions("${Target}" PRIVATE USE_LIVEPP=1)
            target_include_directories(${Target} PUBLIC ${ProjectRoot}LivePP/API/x64/)
        else()
            target_compile_definitions("${Target}" PRIVATE USE_LIVEPP=0)
        endif()
        target_compile_options(${Target} PRIVATE "/Z7")

        target_compile_options(${Target} PRIVATE "$<$<C_COMPILER_ID:MSVC>:/utf-8>")
        target_compile_options(${Target} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

        target_compile_options(${Target} PRIVATE "$<$<C_COMPILER_ID:MSVC>:/bigobj>")
        target_compile_options(${Target} PRIVATE "$<$<CXX_COMPILER_ID:MSVC>:/bigobj>")

    else()
        # jet-live support:
        if(HAS_JETLIVE)
            target_compile_definitions("${Target}" PRIVATE USE_JETLIVE=1)
            target_link_libraries(${Target} PUBLIC jet-live)
        endif ()
    endif ()

    if(NsightAftermath_FOUND)
        target_include_directories("${Target}" PRIVATE ${NsightAftermath_INCLUDE_DIRS})
        # TODO: command line
        target_compile_definitions("${Target}" PRIVATE AFTERMATH_ENABLE=1)
        target_compile_definitions("${Target}" PRIVATE GPU_ASSISTED_VALIDATION=1)
    endif()

    # workaround for Visual Studio to use the right working directory by default
    set_target_properties(
            "${Target}" PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}")
endfunction()

function(add_core_precompiled_headers Target)
    target_precompile_headers(${Target} PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:<core/utils/PortabilityHelper.h$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<glm/glm.hpp$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<chrono$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<unordered_map$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<filesystem$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<vector$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<string$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<memory$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<algorithm$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<cstring$<ANGLE-R>>"
            "$<$<COMPILE_LANGUAGE:CXX>:<core/utils/Types.h$<ANGLE-R>>"
    )
endfunction()

add_subdirectory(core)
add_subdirectory(asset_tools)

function(add_spirv_shader SHADER_STAGE INPUT_FILE OUTPUT_FILE)
    set(depfile "${CMAKE_BINARY_DIR}/resources/shaders/${OUTPUT_FILE}.d")
    set(basePath "${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/")
    set(inputFilePath "${basePath}${INPUT_FILE}")
    set(outputFilePath "${CMAKE_BINARY_DIR}/resources/shaders/${OUTPUT_FILE}")
    add_custom_command(
            OUTPUT "${outputFilePath}"
            COMMAND shadercompiler "${basePath}" "${inputFilePath}" "${outputFilePath}" "${SHADER_STAGE}"
            COMMENT "Compiling shader ${inputFilePath}"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS "${inputFilePath}"
            DEPENDS shadercompiler
            BYPRODUCTS "${depfile}"
            DEPFILE "${depfile}"
    )
endfunction()

function(add_pipeline PIPELINE_NAME)
    set(depfile "${CMAKE_BINARY_DIR}/resources/pipelines/${PIPELINE_NAME}.pipeline.d")
    set(basePath "${CMAKE_CURRENT_SOURCE_DIR}")
    set(outputBasePath "${CMAKE_BINARY_DIR}")
    set(inputFilePath "resources/pipelines/${PIPELINE_NAME}.json")
    add_custom_command(
            OUTPUT "${outputBasePath}/resources/pipelines/${PIPELINE_NAME}.pipeline"
            COMMAND pipelinecompiler "${basePath}" "${outputBasePath}" "${inputFilePath}"
            COMMENT "Compiling pipeline ${PIPELINE_NAME}"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS "${basePath}/${inputFilePath}"
            DEPENDS pipelinecompiler shadercompiler
            BYPRODUCTS "${depfile}"
            DEPFILE "${depfile}"
    )
endfunction()

function(prepare_assets_folder INPUT_FOLDER OUTPUT_FOLDER)
    # TODO: set(depfile "${CMAKE_BINARY_DIR}/resources/shaders/${OUTPUT_FILE}.d")
    set(basePath "${CMAKE_CURRENT_SOURCE_DIR}/")
    set(inputFilePath "${basePath}${INPUT_FOLDER}")
    set(outputFilePath "${CMAKE_BINARY_DIR}/${OUTPUT_FOLDER}")
    add_custom_command(
            OUTPUT "${outputFilePath}/test"
            COMMAND fertilizer -f -r "${inputFilePath}" "${outputFilePath}"
            COMMENT "Preparing assets in folder ${inputFilePath}"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS "${inputFilePath}"
            DEPENDS fertilizer
            # TODO: BYPRODUCTS "${depfile}"
            # TODO: DEPFILE "${depfile}"
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
    copy_resources(materials)
    copy_resources(sounds)
    copy_resources(musics)
    copy_resources(node_templates)
    copy_resources(particle_projects)
    copy_resources(fonts)
    copy_resources(maps)
    copy_resources(scenes)
    copy_resources(text)
    copy_resources(misc)
endfunction()

function(make_asan_target Target)
    if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        #
        #   Requires manual copy of ASAN dll inside output directory (next to final executable file)
        #
        set(ASSIMP_ASAN ON)
        target_compile_options("${Target}" PRIVATE -fsanitize=address) # /MD will be used implicitly
        #target_link_libraries("${Target}" PRIVATE clang_rt.asan_dynamic-x86_64 clang_rt.asan_dynamic_runtime_thunk-x86_64)
        #target_link_options("${Target}" PRIVATE /wholearchive:clang_rt.asan_dynamic_runtime_thunk-x86_64.lib)
        target_compile_definitions("${Target}" PRIVATE USE_LIVEPP=0)
    endif()
endfunction()

add_subdirectory(engine)
add_subdirectory(samples)
add_subdirectory(editor)
add_subdirectory(runtime)
add_subdirectory(tests)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/.gdbinit" DESTINATION "${CMAKE_BINARY_DIR}/.gdbinit")