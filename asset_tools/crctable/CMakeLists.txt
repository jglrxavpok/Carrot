add_executable(crctable
        gencrc64table.c
)
add_core_includes(crctable)
target_link_libraries(crctable)