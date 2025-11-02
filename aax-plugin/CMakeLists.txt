# ==============================================================================
# Project Configuration
# ==============================================================================
cmake_minimum_required(VERSION 3.21)

# Project metadata - version is used throughout the build
project(pt_v2a 
    VERSION 0.1.0 
    LANGUAGES CXX
    DESCRIPTION "Pro Tools Video-to-Audio Plugin with AI Generation"
)

# C++17 required for JUCE and modern features
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==============================================================================
# External Dependencies
# ==============================================================================

# JUCE Framework (submodule in external/JUCE)
# Provides audio plugin framework, GUI components, and AAX support
add_subdirectory(${CMAKE_SOURCE_DIR}/../external/JUCE ${CMAKE_BINARY_DIR}/juce_build)

# AAX SDK (Avid Audio eXtension SDK)
# Required for Pro Tools plugin compatibility
# Must be set as environment variable: set AAX_SDK_PATH=C:\path\to\aax-sdk-2-9-0
if (NOT DEFINED ENV{AAX_SDK_PATH})
  message(FATAL_ERROR 
    "AAX SDK not found!\n"
    "Set AAX_SDK_PATH environment variable to your AAX SDK root directory.\n"
    "Example: set AAX_SDK_PATH=C:\\SDKs\\aax-sdk-2-9-0"
  )
endif()
set(AAX_SDK_PATH $ENV{AAX_SDK_PATH})
message(STATUS "Using AAX SDK: ${AAX_SDK_PATH}")

# Build AAX SDK as part of this project
add_subdirectory(${AAX_SDK_PATH} ${CMAKE_BINARY_DIR}/aax_sdk_build)

# ==============================================================================
# Plugin Definition
# ==============================================================================
# Creates the AAX plugin target with all metadata
juce_add_plugin(pt_v2a
    # Version inherited from project() - keeps it consistent
    VERSION ${PROJECT_VERSION}
    
    # Plugin Identity (registered with Avid)
    COMPANY_NAME "ldegenhardt"
    PLUGIN_MANUFACTURER_CODE LUDE     # 4-char manufacturer code (must be registered)
    PLUGIN_CODE PTV2                  # 4-char plugin ID (unique identifier)
    AAX_IDENTIFIER "com.ldegenhardt.ptv2a"
    
    # Plugin Metadata
    PRODUCT_NAME "PTV2A"              # Display name in Pro Tools
    FORMATS AAX                       # AAX only (Pro Tools native format)
    PLUGIN_AAX_CATEGORY "Utility"     # Shows in "Utility" menu in Pro Tools
    
    # Plugin Type Configuration
    IS_SYNTH FALSE                    # Audio effect (not synthesizer)
    NEEDS_MIDI_INPUT FALSE            # No MIDI required
    NEEDS_MIDI_OUTPUT FALSE           # No MIDI output
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE # GUI doesn't capture keyboard
    
    # Resources Embedding - CRITICAL for embedded Python + FFmpeg
    COPY_PLUGIN_AFTER_BUILD TRUE      # Enables automatic plugin installation after build
)

# ==============================================================================
# Plugin Source Files
# ==============================================================================
target_sources(pt_v2a PRIVATE         # only for this target
    Source/PluginProcessor.cpp        # Audio processing and business logic
    Source/PluginEditor.cpp           # GUI implementation
)

# ==============================================================================
# Embed Resources Folder
# ==============================================================================
# JUCE's COPY_PLUGIN_AFTER_BUILD does NOT automatically copy the Resources/ folder!
# We must explicitly add a post-build command to copy it into the .aaxplugin bundle.
add_custom_command(TARGET pt_v2a_AAX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo "Copying Resources folder to plugin bundle..."
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources
        $<TARGET_FILE_DIR:pt_v2a_AAX>/../Resources
    COMMENT "Embedding Python runtime and scripts into plugin bundle"
    VERBATIM
)

# ==============================================================================
# Compile Options
# ==============================================================================
# Disable JUCE web browser component (not needed), enable cURL for HTTP requests
target_compile_definitions(pt_v2a PRIVATE
    JUCE_WEB_BROWSER=0                # No web browser component (reduces binary size)
    JUCE_USE_CURL=1                   # Enable cURL for MMAudio API calls
)

# ==============================================================================
# Link Dependencies
# ==============================================================================
target_link_libraries(pt_v2a PRIVATE
    juce::juce_audio_processors       # Core audio plugin framework
    juce::juce_gui_basics             # Basic GUI components
    juce::juce_gui_extra              # Extended GUI features (for rich UI)
)

# ==============================================================================
# Python Scripts Integration
# ==============================================================================
# Strategy: Copy Python scripts from companion/ into Resources/ at build time
# This embeds all Python dependencies directly into the plugin bundle
#
# DEVELOPMENT WORKFLOW:
#   Use symbolic links to avoid manual syncing during development:
#   
#   Windows CMD:
#     cd Resources\python\Scripts
#     mklink standalone_api_client.py ..\..\..\..\companion\standalone_api_client.py
#     cd Resources
#     mklink /D ptsl_integration ..\..\companion\ptsl_integration
#
#   Benefits: Edit companion/ files directly, changes immediately available
#   
# PRODUCTION BUILD:
#   Delete symlinks before building release → CMake will copy real files
#   This ensures the plugin bundle is fully self-contained

# Path configuration
set(COMPANION_DIR ${CMAKE_SOURCE_DIR}/../companion)
set(RESOURCES_DIR ${CMAKE_SOURCE_DIR}/Resources)
set(RESOURCES_PYTHON_DIR ${RESOURCES_DIR}/python)
set(RESOURCES_PYTHON_SCRIPTS ${RESOURCES_PYTHON_DIR}/Scripts)
set(RESOURCES_PYTHON_SITEPACKAGES ${RESOURCES_PYTHON_DIR}/Lib/site-packages)

# Ensure target directories exist before copying
add_custom_command(
    TARGET pt_v2a POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCES_PYTHON_SCRIPTS}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${RESOURCES_PYTHON_SITEPACKAGES}
    COMMENT "Creating Python resource directories..."
)

# Copy standalone_api_client.py to embedded Python Scripts directory
# Uses copy_if_different to avoid unnecessary rebuilds
add_custom_command(
    TARGET pt_v2a POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${COMPANION_DIR}/standalone_api_client.py
        ${RESOURCES_PYTHON_SCRIPTS}/standalone_api_client.py
    COMMENT "Syncing standalone_api_client.py → embedded Python Scripts/"
    VERBATIM
)

# Copy ptsl_integration package to embedded Python site-packages
# Note: copy_directory always copies (no diff check), but necessary for package structure
# Alternative: Use copy_if_different for each .py file, but harder to maintain
add_custom_command(
    TARGET pt_v2a POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${COMPANION_DIR}/ptsl_integration
        ${RESOURCES_PYTHON_SITEPACKAGES}/ptsl_integration
    COMMENT "Syncing ptsl_integration/ → embedded Python site-packages/"
    VERBATIM
)

# Optional: Force CMake reconfigure if companion Python files change
# Uncomment if you want automatic rebuild detection (may slow down CMake configuration)
# file(GLOB_RECURSE COMPANION_SOURCES 
#     CONFIGURE_DEPENDS
#     ${COMPANION_DIR}/*.py
# )

# ==============================================================================
# Embedded Python Runtime
# ==============================================================================
# IMPORTANT: JUCE copies Resources/ folder into the .aaxplugin bundle because
# we set COPY_PLUGIN_AFTER_BUILD TRUE in juce_add_plugin() above.
#
# Expected Resources/ structure (must be prepared manually):
#   Resources/
#     python/
#       python.exe                    # Python 3.12 embedded distribution
#       python312.dll
#       python312.zip                 # Standard library
#       Lib/
#         site-packages/              # Python packages (py-ptsl, grpcio, etc.)
#           ptsl_integration/         # ← Synced by CMake (see above)
#       Scripts/
#         standalone_api_client.py    # ← Synced by CMake (see above)
#
# The plugin runtime locates python.exe relative to the .aaxplugin bundle path.

# ==============================================================================
# Plugin Installation (Manual)
# ==============================================================================
# Pro Tools AAX plugins must be installed to:
#   C:\Program Files\Common Files\Avid\Audio\Plug-Ins\
#
# IMPORTANT: Requires Administrator permissions!
#
# Manual installation steps:
#   1. Build the project (generates build/pt_v2a_artefacts/AAX/pt_v2a.aaxplugin/)
#   2. Copy the .aaxplugin folder to Avid Plug-Ins directory (run as admin)
#   3. Restart Pro Tools
#