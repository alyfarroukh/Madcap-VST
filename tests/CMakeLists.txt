include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.16.1
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(madcap_tests
    test_engine.cpp
    test_harmony.cpp
    test_params.cpp
)
target_link_libraries(madcap_tests PRIVATE madcap_dsp Catch2::Catch2WithMain)

add_test(NAME madcap_tests COMMAND madcap_tests)
