# =============================================================================
# Tests using Catch2
# =============================================================================

# Catch2 is provided via CPM in the parent directory.
# We need to add its CMake helpers to the module path.
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)

add_executable(orion_tests
    test_main.cpp
    test_simulation.cpp
)

target_include_directories(orion_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(orion_tests
    PRIVATE
        Catch2::Catch2WithMain
        orion_core
        imgui_ri      # Brings ImGui + rlImGui sources + raylib (for a minimal input simulation harness)
)

# Make ctest aware of these tests
catch_discover_tests(orion_tests)

# Copy data files next to the test executable (so tests can load economy/tech JSON if needed)
set(TEST_DATA_DEST_DIR "$<TARGET_FILE_DIR:orion_tests>/data")
add_custom_command(
    TARGET orion_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TEST_DATA_DEST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/../data"
            "${TEST_DATA_DEST_DIR}"
    COMMENT "Copying game data files for tests"
)