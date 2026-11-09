cmake_minimum_required(VERSION 3.22)
project(ThaSuite VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# ThaVOICE : suite vocale (AUTOTUNE, COMP, EQ, FX, DLY, REV)
juce_add_plugin(YazaVox
    COMPANY_NAME "ThaVOICE"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Thav
    PLUGIN_CODE Tvox
    FORMATS VST3
    PRODUCT_NAME "Fck ThaVOICE")

target_sources(YazaVox PRIVATE
    YazaVox/Source/PluginProcessor.cpp
    YazaVox/Source/PluginEditor.cpp)

target_compile_definitions(YazaVox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_include_directories(YazaVox PRIVATE YazaVox/third_party)

target_compile_options(YazaVox PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(YazaVox
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)


# ThaMASTER : mastering (SPECTRAL, COMP, ANALOG, STEREO, EQ, CLIP, LIMITER, mesures LUFS)
juce_add_plugin(ThaMaster
    COMPANY_NAME "ThaVOICE"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Thav
    PLUGIN_CODE Tmas
    FORMATS VST3
    PRODUCT_NAME "Fck ThaMASTER")

target_sources(ThaMaster PRIVATE
    ThaMaster/Source/PluginProcessor.cpp
    ThaMaster/Source/PluginEditor.cpp)

target_compile_definitions(ThaMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_compile_options(ThaMaster PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

target_link_libraries(ThaMaster
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
