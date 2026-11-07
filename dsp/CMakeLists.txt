# Framework-free DSP: the Madcap Vita C engine + a small C++20 wrapper. No JUCE.
add_library(madcap_dsp STATIC
    src/arp.c
    src/demo.c
    src/engine.c
    src/fx.c
    src/imgsynth.c
    src/params.c
    src/posters.c
    src/slicer.c
    src/theory.c
    src/MadcapEngine.cpp
    src/MidiChord.cpp
    src/Resample.cpp
)

target_include_directories(madcap_dsp PUBLIC include src)
target_compile_features(madcap_dsp PUBLIC cxx_std_20)
set_target_properties(madcap_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(MSVC)
    target_compile_definitions(madcap_dsp PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(madcap_dsp PRIVATE /W3)
else()
    target_compile_options(madcap_dsp PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
    if(NOT APPLE)
        target_link_libraries(madcap_dsp PUBLIC m)
    endif()
endif()
