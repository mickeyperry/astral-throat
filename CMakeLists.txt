cmake_minimum_required(VERSION 3.22)
project(AstralThroat VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(JUCE)

set(ASTRAL_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

set(ASTRAL_DEFS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_MODAL_LOOPS_PERMITTED=1)

juce_add_plugin(AstralThroat
    COMPANY_NAME "Throat Lab"
    PLUGIN_MANUFACTURER_CODE Thrt
    PLUGIN_CODE AsTh
    FORMATS VST3 Standalone
    PRODUCT_NAME "Astral Throat"
    BUNDLE_ID com.throatlab.astralthroat
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(AstralThroat PRIVATE ${ASTRAL_SOURCES})
target_compile_definitions(AstralThroat PUBLIC ${ASTRAL_DEFS})
target_link_libraries(AstralThroat
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)

# Headless render test: renders audio to a WAV and the editor to a PNG.
juce_add_console_app(RenderTest PRODUCT_NAME "RenderTest")
target_sources(RenderTest PRIVATE ${ASTRAL_SOURCES} Test/RenderTest.cpp)
target_compile_definitions(RenderTest PRIVATE ${ASTRAL_DEFS})
target_link_libraries(RenderTest
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags)
