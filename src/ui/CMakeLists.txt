# UI module CMakeLists.txt

# Create UI library
add_library(arbitrage_ui STATIC
    HttpServer.cpp
    DataExporter.cpp
    DashboardApp.cpp
)

# Include directories
target_include_directories(arbitrage_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/external/nlohmann_json/include
)

# Link dependencies
target_link_libraries(arbitrage_ui
    arbitrage_utils
    arbitrage_core
    arbitrage_data
)

# Compiler features
target_compile_features(arbitrage_ui PUBLIC cxx_std_17)

# Platform-specific threading
find_package(Threads REQUIRED)
target_link_libraries(arbitrage_ui Threads::Threads)

# Install targets
install(TARGETS arbitrage_ui
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/arbitrage/ui
    FILES_MATCHING PATTERN "*.hpp"
)
