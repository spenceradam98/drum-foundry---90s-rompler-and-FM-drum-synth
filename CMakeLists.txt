cmake_minimum_required(VERSION 3.22)

# macOS 11 (Big Sur) on Intel is the minimum; build universal (x86_64 + arm64).
# No -march flags anywhere: the DSP is scalar C++ and runs on any x86-64 (no AVX2 needed).
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "macOS architectures")
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")  # static CRT: no VC++ redist needed
endif()
project(DrumFoundry VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Core engine (no JUCE dependency, unit-testable) ----
add_library(drumfoundry_engine STATIC
    Source/engine/Engine.cpp
    Source/engine/Samples.cpp
    Source/engine/Presets.cpp)
target_include_directories(drumfoundry_engine PUBLIC Source/engine)
set_target_properties(drumfoundry_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)

option(DRUMFOUNDRY_BUILD_TESTS "Build engine tests" ON)
if(DRUMFOUNDRY_BUILD_TESTS)
    add_executable(drumfoundry_tests tests/EngineTests.cpp)
    target_link_libraries(drumfoundry_tests PRIVATE drumfoundry_engine)
    enable_testing()
    add_test(NAME engine COMMAND drumfoundry_tests)
endif()

option(DRUMFOUNDRY_BUILD_PLUGIN "Build the JUCE plugin" ON)
if(NOT DRUMFOUNDRY_BUILD_PLUGIN)
    return()
endif()

# ---- JUCE ----
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(DF_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND DF_FORMATS AU)
endif()

juce_add_plugin(DrumFoundry
    COMPANY_NAME "Drum Foundry Audio"
    PLUGIN_MANUFACTURER_CODE Dfnd
    PLUGIN_CODE Dfy1
    FORMATS ${DF_FORMATS}
    PRODUCT_NAME "Drum Foundry"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

set(DF_PLUGIN_SOURCES
    Source/plugin/PluginProcessor.cpp
    Source/plugin/PluginEditor.cpp
    Source/plugin/Themes.cpp)
set(DF_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_sources(DrumFoundry PRIVATE ${DF_PLUGIN_SOURCES})
target_compile_definitions(DrumFoundry PUBLIC ${DF_DEFS})
target_link_libraries(DrumFoundry
    PRIVATE
        drumfoundry_engine
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Developer tool: renders the editor in each theme to PNG (docs/screenshots). Off by default.
option(DRUMFOUNDRY_BUILD_SCREENSHOTS "Build the screenshot renderer" OFF)
if(DRUMFOUNDRY_BUILD_SCREENSHOTS)
    juce_add_console_app(DrumFoundryShots PRODUCT_NAME "DrumFoundryShots")
    target_sources(DrumFoundryShots PRIVATE tools/Screenshots.cpp ${DF_PLUGIN_SOURCES})
    target_include_directories(DrumFoundryShots PRIVATE Source/plugin)
    target_compile_definitions(DrumFoundryShots PRIVATE ${DF_DEFS} JucePlugin_Name="Drum Foundry"
        JucePlugin_Build_Standalone=0 JucePlugin_IsSynth=1)
    target_link_libraries(DrumFoundryShots PRIVATE drumfoundry_engine juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags)
endif()
