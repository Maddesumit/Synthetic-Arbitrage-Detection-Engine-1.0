# Data module for market data handling
add_library(arbitrage_data
    MarketData.cpp
    WebSocketClient.cpp
    BinanceClient.cpp
    RealTimeDataManager.cpp
    OKXClient.cpp
    BybitClient.cpp
)

target_link_libraries(arbitrage_data 
    PRIVATE 
    arbitrage_utils
    nlohmann_json_lib
    spdlog_lib
    ${CMAKE_THREAD_LIBS_INIT}
    Boost::system
    Boost::filesystem
    Boost::thread
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(arbitrage_data 
    PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
    ${WEBSOCKETPP_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/external/nlohmann_json/include
    ${CMAKE_SOURCE_DIR}/external/spdlog/include
    ${Boost_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Add dependencies for external projects
add_dependencies(arbitrage_data nlohmann_json spdlog)
