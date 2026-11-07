cmake_minimum_required(VERSION 3.22)

project(Madcap VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
# Static MSVC runtime: the plugin then needs no Visual C++ redistributable on the user's machine.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MADCAP_BUILD_PLUGIN "Build the JUCE plugin (VST3 / AU / Standalone)" ON)
option(MADCAP_BUILD_TESTS  "Build the framework-free DSP unit tests" ON)
option(MADCAP_SANITIZE     "Build with AddressSanitizer + UBSan (GCC / Clang)" OFF)

if(MADCAP_SANITIZE AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
endif()

add_subdirectory(dsp)

if(MADCAP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(MADCAP_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
