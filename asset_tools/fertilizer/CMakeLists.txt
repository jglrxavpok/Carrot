add_library(fertilizer-lib
        Fertilizer.cpp

        gpu_assistance/VulkanHelper.cpp
        models/ModelProcessing.cpp
        models/GLTFWriter.cpp
        models/MikkTSpaceInterface.cpp
        TextureCompression.cpp



        ${ProjectRoot}thirdparty/bc7enc_rdo/rgbcx.cpp
)

target_include_directories(fertilizer-lib PUBLIC ./)
target_include_directories(fertilizer-lib PUBLIC ${ProjectRoot}thirdparty/bc7enc_rdo)

add_core_includes(fertilizer-lib)
target_link_libraries(fertilizer-lib PUBLIC CarrotCore meshoptimizer)

add_executable(fertilizer
        main.cpp
        )
add_core_includes(fertilizer)
target_link_libraries(fertilizer PUBLIC fertilizer-lib)