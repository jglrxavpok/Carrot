set(AllTests "")

function(make_test Test)
    string(REPLACE "/" "-" TestName "${Test}")
    list(APPEND AllTests ${TestName})
    add_executable("Carrot-Test${TestName}" ${Test}.cpp)
    add_engine_precompiled_headers("Carrot-Test${TestName}")
    target_link_libraries("Carrot-Test${TestName}" PUBLIC Engine-Base)
endfunction()

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        1b96fa13f549387b7549cc89e1a785cf143a1a50
        FIND_PACKAGE_ARGS NAMES GTest
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

make_test(core/Logging)
make_test(engine/old/Audio)
make_test(engine/old/Resources)
make_test(engine/old/Network-Client)
make_test(engine/old/Network-Server)
make_test(engine/old/Lua)
make_test(engine/old/GeneralMaterials)

enable_testing()

add_executable(
        Engine-Tests
        engine/CSharpECS.cpp
        engine/TestFramework.cpp

        engine/Fundamentals.cpp
)
add_core_includes(Engine-Tests)
add_engine_precompiled_headers(Engine-Tests)

add_executable(
        Core-Tests
        core/Counters.cpp
        core/CSharpScripting.cpp
        core/Document.cpp
        core/FileWatching.cpp
        core/Handles.cpp
        core/InlineAllocator.cpp
        core/Lookup.cpp
        core/Paths.cpp
        core/SparseArrays.cpp
        core/StackAllocator.cpp
        core/Strings.cpp
        core/UniquePtr.cpp
        core/Vector.cpp
        core/VFS.cpp
)
target_link_libraries(
        Engine-Tests
        PUBLIC Engine-Base
        gtest_main
)
target_link_libraries(
        Core-Tests
        PUBLIC CarrotCore
        gtest_main
)

include(GoogleTest)
gtest_discover_tests(Engine-Tests)

copy_all_resources()