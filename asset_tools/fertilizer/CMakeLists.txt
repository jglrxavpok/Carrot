add_library(fertilizer-lib
        Fertilizer.cpp

        models/ModelProcessing.cpp
        models/GLTFWriter.cpp
        models/MikkTSpaceInterface.cpp
        TextureCompression.cpp
)

target_include_directories(fertilizer-lib PUBLIC ./)

add_core_includes(fertilizer-lib)
target_link_libraries(fertilizer-lib PUBLIC Vulkan::Vulkan CarrotCore meshoptimizer)

add_executable(fertilizer
        main.cpp
        )
add_core_includes(fertilizer)
target_link_libraries(fertilizer PUBLIC fertilizer-lib)