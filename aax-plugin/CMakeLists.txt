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

# ==============================================================================
# Python Scripts Integration
# ==============================================================================
# Copy Python scripts from companion/ to Resources/ at build time
# This keeps Resources/ synchronized with the originals automatically
#
# NOTE: During development, you can use symlinks instead:
#   cd Resources/python/Scripts
#   mklink standalone_api_client.py ..\..\..\..\companion\standalone_api_client.py
#   cd Resources
#   mklink /D ptsl_integration ..\..\companion\ptsl_integration
#
# CMake will skip copying if symlinks exist and point to the same content.
# For production builds, delete symlinks and CMake will copy real files.

set(COMPANION_DIR ${CMAKE_SOURCE_DIR}/../companion)
set(RESOURCES_DIR ${CMAKE_SOURCE_DIR}/Resources)
set(RESOURCES_PYTHON_SCRIPTS ${CMAKE_SOURCE_DIR}/Resources/python/Scripts)

# Copy standalone_api_client.py to embedded Python Scripts directory
add_custom_command(
    TARGET pt_v2a POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${COMPANION_DIR}/standalone_api_client.py
        ${RESOURCES_PYTHON_SCRIPTS}/standalone_api_client.py
    COMMENT "Syncing standalone_api_client.py to embedded Python Scripts/"
)

# Copy ptsl_integration package to embedded Python site-packages
# This makes the plugin completely self-contained
add_custom_command(
    TARGET pt_v2a POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${COMPANION_DIR}/ptsl_integration
        ${CMAKE_SOURCE_DIR}/Resources/python/Lib/site-packages/ptsl_integration
    COMMENT "Syncing ptsl_integration/ to embedded Python site-packages/"
)

# Monitor source files for changes (CMake will reconfigure if they change)
file(GLOB_RECURSE COMPANION_SOURCES 
    CONFIGURE_DEPENDS
    ${COMPANION_DIR}/*.py
)

# ==============================================================================
# Embedded Python Integration
# ==============================================================================
# Copy Resources/python/ to plugin bundle at build time
# Note: JUCE's juce_add_plugin already handles copying Resources/ for AAX format
# The Resources/ folder structure should match:
#   Resources/python/python.exe
#   Resources/python/Lib/site-packages/
#   Resources/python/Scripts/

# Post-build: copy .aaxplugin into Avid folder (requires admin permissions)
# Note: This will be done manually for testing
