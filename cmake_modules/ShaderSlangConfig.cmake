# Custom module for Slang, because Vulkan SDK 1.4.335.0 does not ship with a slang module on Windows, but does on Linux...

find_path(ShaderSlang_INCLUDE_DIR
          NAMES slang.h
          PATHS
          "$ENV{VULKAN_SDK}/include/slang"
          NO_DEFAULT_PATH)

find_library(ShaderSlang_LIBS
        NAMES slang
        PATHS
        "$ENV{VULKAN_SDK}/lib"
        NO_DEFAULT_PATH
)

set(ShaderSlang_LIBRARIES ${ShaderSlang_LIBS})
set(ShaderSlang_INCLUDE_DIRS ${ShaderSlang_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ShaderSlang
        DEFAULT_MSG
        ShaderSlang_LIBRARIES ShaderSlang_INCLUDE_DIRS)

mark_as_advanced(ShaderSlang_INCLUDE_DIRS ShaderSlang_LIBRARIES)

if(ShaderSlang_FOUND AND NOT TARGET ShaderSlang::ShaderSlang)
    add_library(ShaderSlang::ShaderSlang UNKNOWN IMPORTED)
    set_target_properties(ShaderSlang::ShaderSlang PROPERTIES
            IMPORTED_LOCATION "${ShaderSlang_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${ShaderSlang_INCLUDE_DIRS}")
endif()

if(NOT ShaderSlang_FOUND)
    message("Could not find Slang inside Vulkan SDK")
endif()