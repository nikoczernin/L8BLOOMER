cmake_minimum_required(VERSION 3.22)
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS" FORCE)
endif()
project(L8BLOOMER VERSION 1.3.1 LANGUAGES C CXX)
if(NOT APPLE)
    message(FATAL_ERROR "This project builds a macOS Audio Unit. Use a Mac to build it.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/JUCE-9.0.5" CACHE PATH "JUCE 9.0.5 source")
if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE is missing. Run bash scripts/build.sh to download the pinned dependency.")
endif()
add_subdirectory("${JUCE_DIR}" JUCE EXCLUDE_FROM_ALL)

juce_add_plugin(SlowBloom
    COMPANY_NAME "Niko Audio"
    BUNDLE_ID "audio.niko.slowbloom"
    PLUGIN_MANUFACTURER_CODE Niko
    PLUGIN_CODE SlBl
    PRODUCT_NAME "L8BLOOMER"
    DESCRIPTION "One-shot volume, filter, and frequency-shift sweeps"
    FORMATS AU
    AU_MAIN_TYPE kAudioUnitType_Effect
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)
target_sources(SlowBloom PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(SlowBloom PUBLIC Source)
target_compile_definitions(SlowBloom PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SlowBloom PRIVATE juce::juce_audio_processors
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

enable_testing()
add_executable(DspTests Tests/DspTests.cpp)
target_include_directories(DspTests PRIVATE Source)
target_compile_options(DspTests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME DSP COMMAND DspTests)
add_executable(FrequencyTests Tests/FrequencyTests.cpp)
target_include_directories(FrequencyTests PRIVATE Source)
add_test(NAME FrequencyDSP COMMAND FrequencyTests)
add_executable(DspBenchmark Tests/DspBenchmark.cpp)
target_include_directories(DspBenchmark PRIVATE Source)

add_executable(PluginTests Tests/PluginTests.cpp)
target_include_directories(PluginTests PRIVATE "${JUCE_DIR}/modules")
target_compile_definitions(PluginTests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1)
target_link_libraries(PluginTests PRIVATE SlowBloom)
add_test(NAME PluginState COMMAND PluginTests --render-shift "${CMAKE_CURRENT_BINARY_DIR}/L8BLOOMER-preview.png")
