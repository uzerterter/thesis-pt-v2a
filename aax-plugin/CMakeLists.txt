cmake_minimum_required(VERSION 3.21)
project(pt_v2a VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)

# JUCE as submodule
add_subdirectory(${CMAKE_SOURCE_DIR}/../external/JUCE ${CMAKE_BINARY_DIR}/juce_build)

# AAX SDK path (set AAX_SDK_PATH as a user env var)
if (NOT DEFINED ENV{AAX_SDK_PATH})
  message(FATAL_ERROR "Set AAX_SDK_PATH to your AAX SDK root")
endif()
set(AAX_SDK_PATH $ENV{AAX_SDK_PATH})

# Add AAX SDK as subdirectory to build it
add_subdirectory(${AAX_SDK_PATH} ${CMAKE_BINARY_DIR}/aax_sdk_build)

# Plugin target
juce_add_plugin(pt_v2a
    VERSION 0.1.0
    COMPANY_NAME "ldegenhardt"
    PLUGIN_MANUFACTURER_CODE LUDD     # 4 chars
    PLUGIN_CODE PTV2                  # Fixed to 4 chars
    FORMATS AAX                       # AAX only for now
    PRODUCT_NAME "PTV2A"
    AAX_IDENTIFIER "com.ldegenhardt.ptv2a"
    PLUGIN_AAX_CATEGORY "Utility"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
)

target_sources(pt_v2a PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(pt_v2a PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=1
)

target_link_libraries(pt_v2a PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# Post-build: copy .aaxplugin into Avid folder (requires admin permissions)
# Note: This will be done manually for testing
