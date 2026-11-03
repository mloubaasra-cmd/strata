cmake_minimum_required(VERSION 3.22)

project(Strata VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# JUCE: use a local checkout if JUCE_PATH is given, otherwise download it.
# ------------------------------------------------------------------------------
set(JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (optional)")

if(JUCE_PATH)
    add_subdirectory(${JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ------------------------------------------------------------------------------
# The plug-in
# ------------------------------------------------------------------------------
juce_add_plugin(Strata
    COMPANY_NAME                "Strata Audio"
    BUNDLE_ID                   com.strataaudio.strata
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    PLUGIN_MANUFACTURER_CODE    Stra
    PLUGIN_CODE                 Strt
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Instrument Synth
    PRODUCT_NAME                "Strata")

set(STRATA_SOURCES
    Source/Parameters.cpp
    Source/Wavetable.cpp
    Source/Engine.cpp
    Source/Effects.cpp
    Source/PluginProcessor.cpp
    Source/Presets.cpp
    Source/FactoryPresets.cpp
    Source/UI/LookAndFeel.cpp
    Source/UI/Controls.cpp
    Source/UI/Editors.cpp
    Source/UI/Pages.cpp
    Source/UI/PresetBrowser.cpp
    Source/UI/Tutorial.cpp
    Source/PluginEditor.cpp)

target_sources(Strata PRIVATE ${STRATA_SOURCES})
target_include_directories(Strata PRIVATE Source)

target_compile_definitions(Strata PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Strata
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------------------
# Optional offline test: renders every factory preset and checks the output.
# ------------------------------------------------------------------------------
option(STRATA_BUILD_TESTS "Build the offline preset render test" OFF)

if(STRATA_BUILD_TESTS)
    juce_add_console_app(StrataTest PRODUCT_NAME "StrataTest")
    target_sources(StrataTest PRIVATE Tests/RenderTest.cpp ${STRATA_SOURCES})
    target_include_directories(StrataTest PRIVATE Source)
    target_compile_definitions(StrataTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Strata"
        JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1 JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0 JucePlugin_EditorRequiresKeyboardFocus=0)
    target_link_libraries(StrataTest PRIVATE
        juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
