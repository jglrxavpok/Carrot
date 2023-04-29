add_executable(bayer_matrix_gen
        main.cpp
)
add_core_includes(bayer_matrix_gen)
target_link_libraries(bayer_matrix_gen PUBLIC CarrotCore)