# Attempts to find Mono .NET libraries from the environment variable MONO_SDK_PATH

find_path(
        MONO_NET_LIBS
        NAMES
        Mono.CSharp.dll
        PATHS
        "$ENV{MONO_SDK_PATH}/lib/mono/4.5"
        NO_SYSTEM_ENVIRONMENT_PATH
)

set(MonoNETLibraries_PATH ${MONO_NET_LIBS})
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MonoNETLibraries
        DEFAULT_MSG
        MONO_NET_LIBS)

if(NOT MonoNETLibraries_FOUND)
    if(DEFINED ENV{MONO_SDK_PATH})
        message("Could not find Mono .NET libraries at MONO_SDK_PATH ('$ENV{MONO_SDK_PATH}')")
    else()
        message("Could not find Mono .NET libraries, no MONO_SDK_PATH environment variable has been set")
    endif()
endif()