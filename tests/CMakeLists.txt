set(AllTests "")

function(make_test Test)
    string(REPLACE "/" "-" TestName "${Test}")
    list(APPEND AllTests ${TestName})
    add_executable("Carrot-Test${TestName}" ${Test}.cpp)
    target_link_libraries("Carrot-Test${TestName}" PUBLIC Engine-Base)
endfunction()

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

make_test(engine/Audio)
make_test(engine/Resources)
make_test(engine/Network-Client)
make_test(engine/Network-Server)
make_test(engine/Lua)
make_test(engine/GeneralMaterials)

enable_testing()

add_executable(
        Engine-Tests
        engine/CSharpECS.cpp
        engine/test_game_main.cpp
)
add_core_includes(Engine-Tests)
add_engine_precompiled_headers(Engine-Tests)

add_executable(
        Core-Tests
        core/Coroutines.cpp
        core/Counters.cpp
        core/CSharpScripting.cpp
        core/FileWatching.cpp
        core/Lookup.cpp
        core/Paths.cpp
        core/Strings.cpp
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