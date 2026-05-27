cmake_minimum_required(VERSION 3.25)
project(OrionReborn VERSION 0.1.0 LANGUAGES CXX C)

# =============================================================================
# Global Project Settings
# =============================================================================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin)

# =============================================================================
# CPM Bootstrap (no manual install required)
# =============================================================================
set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake (v${CPM_DOWNLOAD_VERSION})...")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        ${CPM_DOWNLOAD_LOCATION}
        STATUS CPM_DOWNLOAD_STATUS
    )
    list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_ERROR)
    if(CPM_DOWNLOAD_ERROR)
        message(FATAL_ERROR "Failed to download CPM.cmake. Check network or manually place it at ${CPM_DOWNLOAD_LOCATION}")
    endif()
    message(STATUS "CPM.cmake downloaded successfully.")
endif()

include(${CPM_DOWNLOAD_LOCATION})

# =============================================================================
# Dependencies via CPM (fetched automatically, no system installs needed)
# =============================================================================

# ---- raylib (2D graphics, input, audio, windowing) ----
# Using 6.0 (April 2026 release) - latest stable with excellent C++ support
CPMAddPackage(
    NAME raylib
    GITHUB_REPOSITORY raysan5/raylib
    GIT_TAG 6.0
    OPTIONS
        "BUILD_EXAMPLES OFF"
        "BUILD_GAMES OFF"
        "BUILD_TESTING OFF"
        "USE_EXTERNAL_GLFW OFF"
)

# ---- Dear ImGui (docking branch for best compatibility with rlImGui) ----
CPMAddPackage(
    NAME imgui
    GITHUB_REPOSITORY ocornut/imgui
    GIT_TAG docking
    GIT_SHALLOW TRUE
)

# ---- rlImGui (raylib backend for Dear ImGui) ----
# main is currently compatible with raylib 6.0; pin to a known commit if issues arise
CPMAddPackage(
    NAME rlImGui
    GITHUB_REPOSITORY raylib-extras/rlImGui
    GIT_TAG main
    GIT_SHALLOW TRUE
)

# ---- simdjson (used for both saving and loading games as JSON, using its builder API) ----
CPMAddPackage(
    NAME simdjson
    GITHUB_REPOSITORY simdjson/simdjson
    GIT_TAG master          # master has the improved builder API documented in builder.md
    GIT_SHALLOW TRUE
)

# ---- Catch2 (testing framework) ----
CPMAddPackage(
    NAME Catch2
    GITHUB_REPOSITORY catchorg/Catch2
    GIT_TAG v3.5.3
    OPTIONS
        "CATCH_BUILD_TESTING OFF"
        "CATCH_INSTALL_DOCS OFF"
)

# Enable CTest integration
enable_testing()

# =============================================================================
# ImGui + rlImGui static library (clean encapsulation)
# =============================================================================
add_library(imgui_ri STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp          # Keep for development/debug (remove in release builds later)
    ${rlImGui_SOURCE_DIR}/rlImGui.cpp
)

target_include_directories(imgui_ri
    PUBLIC
        ${imgui_SOURCE_DIR}
        ${rlImGui_SOURCE_DIR}
)

# Make sure raylib headers (raylib.h) are visible when compiling rlImGui.cpp
target_link_libraries(imgui_ri PUBLIC raylib)

# ImGui compile definitions (optional but useful)
target_compile_definitions(imgui_ri PUBLIC
    IMGUI_DISABLE_OBSOLETE_FUNCTIONS
    # IMGUI_DISABLE_DEMO_WINDOWS   # Uncomment once we have our own UI
)

# Platform-specific compile flags for ImGui
if(MSVC)
    target_compile_options(imgui_ri PRIVATE /W0)
else()
    target_compile_options(imgui_ri PRIVATE -w)
endif()

# =============================================================================
# Core Simulation Library (pure game logic, no rendering / UI)
# =============================================================================
add_library(orion_core STATIC
    src/core/GameSimulation.cpp
    src/core/SaveLoad.cpp
    src/core/EconomyConfig.cpp
    src/core/TechTree.cpp
    # More files will be moved here over time (GameState logic, turn processing, etc.)
)

target_include_directories(orion_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(orion_core
    PUBLIC
        raylib                  # Temporary: for Vector2 in data structures
        simdjson::simdjson
)

# =============================================================================
# Main Executable
# =============================================================================
add_executable(orion-reborn
    src/main.cpp
    src/core/SaveLoad.cpp
)

target_link_libraries(orion-reborn
    PRIVATE
        raylib
        imgui_ri
        simdjson::simdjson
        orion_core
)

target_include_directories(orion-reborn
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# C++23 feature flags and warnings
target_compile_features(orion-reborn PRIVATE cxx_std_23)

if(MSVC)
    target_compile_options(orion-reborn PRIVATE /W4 /permissive- /Zc:__cplusplus)
else()
    target_compile_options(orion-reborn PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
endif()

# =============================================================================
# Copy data files (economy.json, tech_tree.json, etc.) to the runtime directory
# so the game can load them from bin/data/ at runtime.
# =============================================================================
set(DATA_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
set(DATA_DEST_DIR "$<TARGET_FILE_DIR:orion-reborn>/data")

add_custom_command(
    TARGET orion-reborn POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DATA_DEST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${DATA_SOURCE_DIR}"
            "${DATA_DEST_DIR}"
    COMMENT "Copying game data files to ${DATA_DEST_DIR}"
)

# =============================================================================
# Platform-specific linking (raylib often needs these explicitly)
# =============================================================================
if(APPLE)
    target_link_libraries(orion-reborn PRIVATE
        "-framework IOKit"
        "-framework Cocoa"
        "-framework OpenGL"
        "-framework CoreVideo"
        "-framework CoreAudio"
        "-framework AudioToolbox"
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(orion-reborn PRIVATE m pthread dl)
elseif(WIN32)
    # raylib handles most on Windows via its CMake
endif()

# =============================================================================
# Installation (optional, for distribution later)
# =============================================================================
install(TARGETS orion-reborn RUNTIME DESTINATION bin)

# =============================================================================
# Tests (Catch2 + CTest)
# =============================================================================
add_subdirectory(tests)

# =============================================================================
# Summary
# =============================================================================
message(STATUS "")
message(STATUS "=== Orion Reborn Configuration ===")
message(STATUS "  Build type:            ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:          ${CMAKE_CXX_STANDARD}")
message(STATUS "  raylib:                ${raylib_SOURCE_DIR} (v6.0)")
message(STATUS "  ImGui (docking):       ${imgui_SOURCE_DIR}")
message(STATUS "  rlImGui:               ${rlImGui_SOURCE_DIR}")
message(STATUS "  simdjson:              ${simdjson_SOURCE_DIR}")
message(STATUS "==================================")
message(STATUS "")