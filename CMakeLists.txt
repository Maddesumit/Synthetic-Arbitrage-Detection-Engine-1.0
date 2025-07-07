cmake_minimum_required(VERSION 3.16)
project(SyntheticArbitrageEngine VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options for optimization
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
endif()

# Enable position independent code
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/external)

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem thread)
find_package(OpenSSL REQUIRED)

# Find Eigen3
find_package(Eigen3 REQUIRED)
if(Eigen3_FOUND)
    message(STATUS "Found Eigen3: ${EIGEN3_INCLUDE_DIR}")
else()
    # Fallback for systems where Eigen3 is not found via find_package
    find_path(EIGEN3_INCLUDE_DIR Eigen/Dense
        PATHS /opt/homebrew/include/eigen3 /usr/local/include/eigen3 /usr/include/eigen3
        PATH_SUFFIXES ""
    )
    if(EIGEN3_INCLUDE_DIR)
        message(STATUS "Found Eigen3 headers at: ${EIGEN3_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "Eigen3 not found!")
    endif()
endif()

# Add RapidJSON headers (header-only library)
find_path(RAPIDJSON_INCLUDE_DIR rapidjson/document.h
    PATHS /opt/homebrew/include /usr/local/include
    PATH_SUFFIXES ""
)
if(RAPIDJSON_INCLUDE_DIR)
    message(STATUS "Found RapidJSON: ${RAPIDJSON_INCLUDE_DIR}")
    include_directories(${RAPIDJSON_INCLUDE_DIR})
else()
    message(WARNING "RapidJSON not found. Install with: brew install rapidjson")
    # Create a directory for RapidJSON headers
    set(RAPIDJSON_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/rapidjson)
    file(MAKE_DIRECTORY ${RAPIDJSON_INCLUDE_DIR})
    include_directories(${RAPIDJSON_INCLUDE_DIR})
endif()

# Add custom module path for finding packages
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Include external dependencies
include(ExternalProject)

# nlohmann/json - Header-only JSON library
ExternalProject_Add(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/external/nlohmann_json
        -DJSON_BuildTests=OFF
        -DJSON_Install=ON
)

# spdlog - Fast C++ logging library
ExternalProject_Add(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/external/spdlog
        -DSPDLOG_BUILD_EXAMPLE=OFF
        -DSPDLOG_BUILD_TESTS=OFF
)

# Catch2 - Modern C++ test framework
ExternalProject_Add(
    catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/external/catch2
        -DCATCH_INSTALL_DOCS=OFF
        -DCATCH_INSTALL_EXTRAS=OFF
)

# Find websocketpp (installed via homebrew)
find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/config/core.hpp
    PATHS /opt/homebrew/include /usr/local/include
    PATH_SUFFIXES ""
)
if(WEBSOCKETPP_INCLUDE_DIR)
    message(STATUS "Found websocketpp: ${WEBSOCKETPP_INCLUDE_DIR}")
else()
    message(FATAL_ERROR "websocketpp not found. Install with: brew install websocketpp")
endif()

# For now, we'll implement our own concurrent containers instead of TBB
# This will be sufficient for Phase 1 testing

# xsimd - SIMD intrinsics wrapper (header-only, manual install)
ExternalProject_Add(
    xsimd
    GIT_REPOSITORY https://github.com/xtensor-stack/xsimd.git
    GIT_TAG 10.0.0
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ${CMAKE_COMMAND} -E copy_directory 
        <SOURCE_DIR>/include 
        ${CMAKE_SOURCE_DIR}/external/xsimd/include
)

# Create interface libraries for external dependencies
add_library(nlohmann_json_lib INTERFACE)
add_dependencies(nlohmann_json_lib nlohmann_json)
target_include_directories(nlohmann_json_lib INTERFACE ${CMAKE_SOURCE_DIR}/external/nlohmann_json/include)

add_library(spdlog_lib INTERFACE)
add_dependencies(spdlog_lib spdlog)
target_include_directories(spdlog_lib INTERFACE ${CMAKE_SOURCE_DIR}/external/spdlog/include)
target_link_directories(spdlog_lib INTERFACE ${CMAKE_SOURCE_DIR}/external/spdlog/lib)

add_library(catch2_lib INTERFACE)
add_dependencies(catch2_lib catch2)
target_include_directories(catch2_lib INTERFACE ${CMAKE_SOURCE_DIR}/external/catch2/include)

add_library(websocketpp_lib INTERFACE)
target_include_directories(websocketpp_lib INTERFACE ${WEBSOCKETPP_INCLUDE_DIR})

add_library(tbb_lib INTERFACE)
# TBB functionality will be implemented using standard library threading for Phase 1

add_library(xsimd_lib INTERFACE)
add_dependencies(xsimd_lib xsimd)
target_include_directories(xsimd_lib INTERFACE ${CMAKE_SOURCE_DIR}/external/xsimd/include)

# Core library
add_subdirectory(src)

# Tests
enable_testing()
add_subdirectory(tests)

# Main executable
add_executable(arbitrage_engine src/main.cpp)
target_link_libraries(arbitrage_engine
    arbitrage_core
    arbitrage_data
    arbitrage_utils
    arbitrage_performance    # Add Phase 7 performance optimization
    arbitrage_network       # Add Phase 7 network optimization
    arbitrage_monitoring    # Add Phase 7 system monitoring
    Threads::Threads
    Boost::system
    Boost::filesystem
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Add LZ4 library if found
if(LZ4_FOUND)
    target_link_libraries(arbitrage_engine ${LZ4_LIBRARIES})
    target_include_directories(arbitrage_engine PRIVATE ${LZ4_INCLUDE_DIRS})
else()
    # Fallback: disable LZ4 compression
    target_compile_definitions(arbitrage_engine PRIVATE -DNO_LZ4_COMPRESSION)
endif()

# Phase 2 demo executable
add_executable(phase2_demo src/phase2_demo.cpp)
target_link_libraries(phase2_demo
    arbitrage_data
    arbitrage_utils
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Phase 3 demo executable
add_executable(phase3_demo src/phase3_demo.cpp)
target_link_libraries(phase3_demo
    arbitrage_core
    arbitrage_data
    arbitrage_utils
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Dashboard demo executable (Phase 9)
add_executable(dashboard_demo src/dashboard_demo.cpp)
target_link_libraries(dashboard_demo
    arbitrage_ui
    arbitrage_core
    arbitrage_data
    arbitrage_utils
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Phase 4 demo executable (Arbitrage Detection Engine)
add_executable(phase4_demo src/phase4_demo.cpp)
target_link_libraries(phase4_demo
    arbitrage_core
    arbitrage_data
    arbitrage_utils
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Phase 5 demo executable (Risk Management & Position Tracking)
add_executable(phase5_demo src/phase5_demo.cpp)
target_link_libraries(phase5_demo
    arbitrage_core
    arbitrage_data
    arbitrage_utils
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Phase 6 demo executable (Arbitrage Ranking & Execution Logic)
add_executable(phase6_demo src/phase6_demo.cpp)
target_link_libraries(phase6_demo
    arbitrage_core
    arbitrage_data
    arbitrage_utils
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Phase 7 Performance Optimization Dependencies

# GPerfTools for CPU and memory profiling
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GPERFTOOLS QUIET libprofiler libunwind)
endif()

# LZ4 for fast compression - TEMPORARILY DISABLED
# find_package(PkgConfig QUIET)
# if(PkgConfig_FOUND)
#     pkg_check_modules(LZ4 QUIET liblz4)
# endif()

# Force disable LZ4 for now to fix build issues
set(LZ4_FOUND FALSE)
message(STATUS "LZ4 compression temporarily disabled")

# Intel MKL for optimized math operations (optional)
find_package(MKL QUIET)

# Add SIMD support detection
include(CheckCXXCompilerFlag)

# Detect processor architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    # x86_64 architecture - check for Intel/AMD SIMD support
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
    
    if(COMPILER_SUPPORTS_AVX2)
        add_definitions(-DHAVE_AVX2)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2")
    elseif(COMPILER_SUPPORTS_SSE42)
        add_definitions(-DHAVE_SSE42)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2")
    endif()
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    # ARM64 architecture - use NEON
    add_definitions(-DHAVE_NEON)
    # ARM64 NEON is enabled by default on Apple Silicon and modern ARM processors
    message(STATUS "ARM64 architecture detected - using NEON SIMD instructions")
endif()

# Enable profiling support if available
if(GPERFTOOLS_FOUND)
    add_definitions(-DHAVE_GOOGLE_PROFILER)
    add_definitions(-DHAVE_GOOGLE_HEAP_PROFILER)
endif()

# Add compression support
if(LZ4_FOUND)
    add_definitions(-DHAVE_LZ4)
    link_directories(/opt/homebrew/lib)  # Add Homebrew lib directory to linker search path
endif()

# Platform-specific optimizations
if(UNIX AND NOT APPLE)
    # Linux-specific optimizations
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
    add_definitions(-D__LINUX__)
elseif(APPLE)
    # macOS-specific optimizations
    add_definitions(-D__APPLE__)
elseif(WIN32)
    # Windows-specific optimizations
    add_definitions(-D_WIN32)
endif()

# Set output directories
set_target_properties(arbitrage_engine phase2_demo phase3_demo phase4_demo phase5_demo phase6_demo dashboard_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install targets
install(TARGETS arbitrage_engine DESTINATION bin)
install(DIRECTORY config/ DESTINATION config)

# Phase 7 Performance Optimization executables and libraries

# Performance monitoring library
add_library(arbitrage_performance 
    src/performance/PerformanceMetrics.cpp
    src/core/MemoryManager.cpp
    src/core/AlgorithmOptimizer.cpp
)

target_link_libraries(arbitrage_performance
    Threads::Threads
    ${CMAKE_DL_LIBS}
)

# Add profiling support if available
if(GPERFTOOLS_FOUND)
    target_link_libraries(arbitrage_performance ${GPERFTOOLS_LIBRARIES})
    target_include_directories(arbitrage_performance PRIVATE ${GPERFTOOLS_INCLUDE_DIRS})
endif()

# Add LZ4 compression support
if(LZ4_FOUND)
    target_link_libraries(arbitrage_performance ${LZ4_LIBRARIES})
    target_include_directories(arbitrage_performance PRIVATE ${LZ4_INCLUDE_DIRS})
else()
    # Fallback: disable LZ4 compression
    target_compile_definitions(arbitrage_performance PRIVATE -DNO_LZ4_COMPRESSION)
endif()

# Network optimization library
add_library(arbitrage_network
    src/network/NetworkOptimizer.cpp
)

target_link_libraries(arbitrage_network
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Add LZ4 compression support to network library - DISABLED FOR NOW
if(LZ4_FOUND)
    target_link_libraries(arbitrage_network ${LZ4_LIBRARIES})
    target_include_directories(arbitrage_network PRIVATE ${LZ4_INCLUDE_DIRS})
else()
    # LZ4 disabled - build without network compression
    target_compile_definitions(arbitrage_network PRIVATE -DNO_LZ4_COMPRESSION)
endif()

# System monitoring library
add_library(arbitrage_monitoring
    src/monitoring/SystemMonitoring.cpp
)

target_link_libraries(arbitrage_monitoring
    arbitrage_performance
    Threads::Threads
)

# Phase 7 performance demo executable
add_executable(phase7_performance_demo src/phase7_performance_demo.cpp)
target_link_libraries(phase7_performance_demo
    arbitrage_performance
    arbitrage_network
    arbitrage_monitoring
    arbitrage_core
    arbitrage_data
    arbitrage_utils
    Threads::Threads
    Boost::system
    Boost::filesystem
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Performance benchmark executable
add_executable(performance_benchmark src/benchmarks/performance_benchmark.cpp)
target_link_libraries(performance_benchmark
    arbitrage_performance
    arbitrage_core
    Threads::Threads
)

# Add LZ4 library if found
if(LZ4_FOUND)
    target_link_libraries(performance_benchmark ${LZ4_LIBRARIES})
else()
    # Fallback: disable LZ4 compression
    target_compile_definitions(performance_benchmark PRIVATE -DNO_LZ4_COMPRESSION)
endif()

# Phase 8: Advanced Features & Integration libraries
add_library(arbitrage_config
    src/config/ConfigurationManager.cpp
)
target_link_libraries(arbitrage_config arbitrage_utils Threads::Threads)

add_library(arbitrage_enhanced_risk
    src/risk/EnhancedRiskModels.cpp
    src/risk/EnhancedRiskModelsImplementation.cpp
)
target_link_libraries(arbitrage_enhanced_risk arbitrage_core arbitrage_utils Threads::Threads)
if(Eigen3_FOUND)
    target_link_libraries(arbitrage_enhanced_risk Eigen3::Eigen)
else()
    target_include_directories(arbitrage_enhanced_risk PRIVATE ${EIGEN3_INCLUDE_DIR})
endif()

add_library(arbitrage_persistence
    src/data/DataPersistence.cpp
)
# Find SQLite for DataPersistence
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SQLite3 QUIET sqlite3)
endif()

if(NOT SQLite3_FOUND)
    # Try to find SQLite manually
    find_path(SQLite3_INCLUDE_DIRS sqlite3.h
        PATHS /opt/homebrew/include /usr/local/include /usr/include
        PATH_SUFFIXES "" sqlite sqlite3
    )
    find_library(SQLite3_LIBRARIES
        NAMES sqlite3
        PATHS /opt/homebrew/lib /usr/local/lib /usr/lib
    )
    if(SQLite3_INCLUDE_DIRS AND SQLite3_LIBRARIES)
        set(SQLite3_FOUND TRUE)
        message(STATUS "Found SQLite3: ${SQLite3_LIBRARIES}")
    else()
        message(WARNING "SQLite3 not found, persistence features may not work")
    endif()
endif()

target_link_libraries(arbitrage_persistence arbitrage_data arbitrage_utils Threads::Threads)
if(SQLite3_FOUND)
    target_include_directories(arbitrage_persistence PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(arbitrage_persistence ${SQLite3_LIBRARIES})
endif()
target_include_directories(arbitrage_persistence PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/xsimd/include)

add_library(arbitrage_api
    src/api/ApiFramework.cpp
    src/api/ApiServerImplementation.cpp
)
target_link_libraries(arbitrage_api arbitrage_core arbitrage_utils Threads::Threads OpenSSL::SSL OpenSSL::Crypto)

# Phase 8 integration demo
add_executable(phase8_integration_demo src/phase8_integration_demo.cpp)
target_link_libraries(phase8_integration_demo 
    arbitrage_config
    arbitrage_enhanced_risk  
    arbitrage_persistence
    arbitrage_api
    arbitrage_core
    arbitrage_data
    arbitrage_utils
    arbitrage_performance
    arbitrage_network
    arbitrage_monitoring
    Threads::Threads
)
if(SQLite3_FOUND)
    target_include_directories(phase8_integration_demo PRIVATE ${SQLite3_INCLUDE_DIRS})
    target_link_libraries(phase8_integration_demo ${SQLite3_LIBRARIES})
endif()
