# Utils library CMakeLists.txt

set(UTILS_SOURCES
    ConfigManager.cpp
    Logger.cpp
    ErrorHandler.cpp
    ThreadUtils.cpp
)

set(UTILS_HEADERS
    ConfigManager.hpp
    Logger.hpp
    ErrorHandler.hpp
    ThreadUtils.hpp
)

# Create utils library
add_library(arbitrage_utils STATIC ${UTILS_SOURCES} ${UTILS_HEADERS})

# Add dependencies
add_dependencies(arbitrage_utils nlohmann_json spdlog)

# Link libraries
target_link_libraries(arbitrage_utils
    PUBLIC
        nlohmann_json_lib
        spdlog_lib
        Threads::Threads
        ${CMAKE_DL_LIBS}
)

# Include directories
target_include_directories(arbitrage_utils
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

# Set properties
set_target_properties(arbitrage_utils PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
