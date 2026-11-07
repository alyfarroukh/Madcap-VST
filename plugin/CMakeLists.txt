include(FetchContent)

# JUCE 8 (CMake API, no Projucer). Pass -DMADCAP_JUCE_DIR=/path/to/JUCE to use a local checkout.
set(MADCAP_JUCE_DIR "" CACHE PATH "Local JUCE checkout (optional)")
if(MADCAP_JUCE_DIR)
    add_subdirectory(${MADCAP_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(MADCAP_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND MADCAP_FORMATS AU)
endif()

juce_add_plugin(Madcap
    PRODUCT_NAME                  "Madcap"
    COMPANY_NAME                  "Madcap Audio"
    COMPANY_WEBSITE               "https://github.com/"
    BUNDLE_ID                     com.madcapaudio.madcap
    VERSION                       ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE      Mdcp
    PLUGIN_CODE                   Mcap
    DESCRIPTION                   "Granular voice instrument and image sweeper"
    FORMATS                       ${MADCAP_FORMATS}
    IS_SYNTH                      TRUE
    NEEDS_MIDI_INPUT              TRUE
    NEEDS_MIDI_OUTPUT             FALSE
    IS_MIDI_EFFECT                FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS   TRUE
    COPY_PLUGIN_AFTER_BUILD       FALSE
    VST3_CATEGORIES               Instrument Sampler Synth
    AU_MAIN_TYPE                  kAudioUnitType_MusicDevice
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT    "Madcap records your voice from the microphone to use it as a sample."
    HARDENED_RUNTIME_ENABLED      FALSE)

juce_add_binary_data(MadcapAssets
    HEADER_NAME BinaryData.h
    NAMESPACE   BinaryData
    SOURCES
        Resources/fonts/Rajdhani-Medium.ttf
        Resources/fonts/Rajdhani-SemiBold.ttf
        Resources/fonts/Rajdhani-Bold.ttf)

target_sources(Madcap PRIVATE
    Source/PluginProcessor.cpp
    Source/Parameters.cpp
    Source/SampleStore.cpp
    Source/ImageStore.cpp
    Source/ImageDecode.cpp
    Source/Presets.cpp
    Source/PluginEditor.cpp
    Source/ui/MadcapLookAndFeel.cpp
    Source/ui/Widgets.cpp
    Source/ui/Displays.cpp
    Source/ui/TopBar.cpp
    Source/ui/Pages.cpp
    Source/third_party/picojpeg.c)

target_include_directories(Madcap PRIVATE Source)

# third-party decoder: keep its warnings out of the build log
if(MSVC)
    set_source_files_properties(Source/third_party/picojpeg.c PROPERTIES COMPILE_OPTIONS "/W0")
else()
    set_source_files_properties(Source/third_party/picojpeg.c PROPERTIES COMPILE_OPTIONS "-w")
endif()

target_compile_definitions(Madcap PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_USE_WINDOWS_MEDIA_FORMAT=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(Madcap
    PRIVATE
        madcap_dsp
        MadcapAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Windows: keep debug symbols (PDB) and a linker map for release builds, so a crash
# address from Event Viewer or a host's crash report can be traced to a function.
if(MSVC)
    target_compile_options(Madcap PRIVATE /Zi)
    target_compile_options(madcap_dsp PRIVATE /Zi)
    foreach(t Madcap_VST3 Madcap_Standalone)
        if(TARGET ${t})
            target_link_options(${t} PRIVATE /DEBUG /OPT:REF /OPT:ICF "/MAP:${CMAKE_BINARY_DIR}/symbols/${t}.map")
        endif()
    endforeach()
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/symbols)
endif()
