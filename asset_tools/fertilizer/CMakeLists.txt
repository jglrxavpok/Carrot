add_library(fertilizer-lib
        Fertilizer.cpp

        gltf/GLTFProcessing.cpp
        gltf/GLTFWriter.cpp
        gltf/MikkTSpaceInterface.cpp
        TextureCompression.cpp
)

target_include_directories(fertilizer-lib PUBLIC ./)

add_core_includes(fertilizer-lib)
target_link_libraries(fertilizer-lib PUBLIC Vulkan::Vulkan CarrotCore)

add_executable(fertilizer
        main.cpp
        )
add_core_includes(fertilizer)
target_link_libraries(fertilizer PUBLIC fertilizer-lib)