# Core pricing engine library
add_library(arbitrage_core
    MathUtils.cpp
    PricingEngine.cpp
    ArbitrageEngine.cpp
    RiskManager.cpp
    PositionManager.cpp
    OpportunityRanker.cpp
    ExecutionPlanner.cpp
    PnLTracker.cpp
    ExecutionSimulator.cpp
)

target_link_libraries(arbitrage_core 
    PRIVATE 
    arbitrage_utils
    arbitrage_data
    ${CMAKE_THREAD_LIBS_INIT}
)

target_include_directories(arbitrage_core 
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/external/xsimd/include
    PRIVATE
    ${CMAKE_SOURCE_DIR}/external/spdlog/include
)

# Enable C++20 features for std::invoke_result and other modern features
target_compile_features(arbitrage_core PRIVATE cxx_std_20)
