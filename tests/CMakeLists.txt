# Tests CMakeLists.txt

# Create test executable
add_executable(run_tests
    test_utils.cpp
)

# Add dependencies
add_dependencies(run_tests catch2)

# Link libraries
target_link_libraries(run_tests
    PRIVATE
        arbitrage_utils
        ${CMAKE_SOURCE_DIR}/external/catch2/lib/libCatch2.a
        ${CMAKE_SOURCE_DIR}/external/catch2/lib/libCatch2Main.a
        Threads::Threads
)

# Include directories
target_include_directories(run_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/external/catch2/include
)

# Set properties
set_target_properties(run_tests PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Add test to CTest
add_test(NAME UnitTests COMMAND run_tests)

# Create test data directory
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test_logs)
