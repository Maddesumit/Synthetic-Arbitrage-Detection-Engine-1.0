# Synthetic Pair Deviation Engine - Project Structure

## 📁 Project Organization

```
Synthetic Pair Deviation Engine/
├── README.md                          # Main project documentation
├── CMakeLists.txt                     # CMake build configuration
├── build.sh                          # Build script
├── .gitignore                         # Git ignore rules
├── .vscode/                           # VS Code configuration
├── arbitrage-engine.code-workspace    # VS Code workspace file
│
├── src/                               # Source code
│   ├── main.cpp                       # Main application entry point
│   ├── core/                          # Core engine components
│   │   ├── ArbitrageEngine.cpp/.hpp   # Main arbitrage detection engine
│   │   ├── OrderBookManager.cpp/.hpp  # Order book management
│   │   ├── OpportunityDetector.cpp/.hpp # Opportunity detection logic
│   │   └── RiskManager.cpp/.hpp       # Risk management
│   ├── data/                          # Data handling
│   │   ├── MarketData.hpp             # Market data structures
│   │   ├── WebSocketClient.hpp        # WebSocket client base
│   │   ├── BinanceClient.cpp/.hpp     # Binance exchange client
│   │   ├── BybitClient.cpp/.hpp       # Bybit exchange client
│   │   └── OKXClient.cpp/.hpp         # OKX exchange client
│   ├── strategy/                      # Trading strategies
│   │   ├── BaseStrategy.hpp           # Base strategy interface
│   │   ├── PairTradingStrategy.cpp/.hpp # Pair trading implementation
│   │   └── ArbitrageStrategy.cpp/.hpp # Arbitrage strategy
│   └── utils/                         # Utility functions
│       ├── Logger.cpp/.hpp            # Logging utilities
│       ├── Config.cpp/.hpp            # Configuration management
│       └── MathUtils.cpp/.hpp         # Mathematical utilities
│
├── config/                            # Configuration files
│   ├── config.json                    # Main configuration
│   └── README.md                      # Configuration documentation
│
├── docs/                              # Documentation
│   ├── API.md                         # API documentation
│   ├── ARCHITECTURE.md               # Architecture documentation
│   ├── GETTING_STARTED.md            # Getting started guide
│   └── EXCHANGES.md                  # Exchange integration guide
│
├── tests/                             # Test files
│   ├── unit/                          # Unit tests
│   ├── integration/                   # Integration tests
│   └── mocks/                         # Mock objects
│
└── external/                          # External dependencies
    ├── catch2/                        # Testing framework
    ├── nlohmann_json/                 # JSON library
    ├── rapidjson/                     # Fast JSON parser
    ├── spdlog/                        # Logging library
    ├── websocketpp/                   # WebSocket library
    └── xsimd/                         # SIMD library
```

## 🔧 Core Components

### **1. Engine Core (`src/core/`)**
- **ArbitrageEngine**: Main orchestrator for arbitrage detection
- **OrderBookManager**: Manages order book data from multiple exchanges
- **OpportunityDetector**: Detects profitable arbitrage opportunities
- **RiskManager**: Manages risk and position sizing

### **2. Data Layer (`src/data/`)**
- **MarketData**: Structures for market data (order books, trades, tickers)
- **WebSocketClient**: Base class for exchange connections
- **Exchange Clients**: Specific implementations for Binance, Bybit, OKX

### **3. Strategy Layer (`src/strategy/`)**
- **BaseStrategy**: Abstract base for trading strategies
- **PairTradingStrategy**: Pair trading implementation
- **ArbitrageStrategy**: Arbitrage strategy implementation

### **4. Utilities (`src/utils/`)**
- **Logger**: Comprehensive logging system
- **Config**: Configuration management
- **MathUtils**: Mathematical utilities and calculations

## 📊 Key Features

- **Multi-Exchange Support**: Binance, Bybit, OKX integration
- **Real-time Data**: WebSocket connections for live market data
- **Risk Management**: Built-in risk controls and position sizing
- **High Performance**: C++ implementation with SIMD optimizations
- **Comprehensive Logging**: Detailed logging for monitoring and debugging
- **Modular Design**: Clean separation of concerns

## 🚀 Getting Started

1. **Prerequisites**: C++17, CMake 3.15+, WebSocket++ dependencies
2. **Build**: Run `./build.sh` or use CMake directly
3. **Configure**: Edit `config/config.json` with your API keys
4. **Run**: Execute the built binary

## 🔧 Configuration

Configuration is managed through JSON files in the `config/` directory:
- `config.json`: Main configuration file
- Templates available for different environments

## 📚 Documentation

Comprehensive documentation is available in the `docs/` directory:
- **API.md**: API reference and usage
- **ARCHITECTURE.md**: System architecture and design
- **GETTING_STARTED.md**: Setup and usage guide
- **EXCHANGES.md**: Exchange integration details

## 🧪 Testing

Testing infrastructure includes:
- **Unit Tests**: Component-level testing
- **Integration Tests**: End-to-end testing
- **Mock Objects**: For isolated testing

## 📈 Performance

- **C++ Implementation**: High-performance native code
- **SIMD Optimizations**: Vectorized mathematical operations
- **Efficient Data Structures**: Optimized for real-time processing
- **Minimal Latency**: Direct WebSocket connections

## 🔒 Security

- **API Key Management**: Secure credential handling
- **Risk Controls**: Built-in position and exposure limits
- **Audit Trail**: Comprehensive logging for compliance

---

*This project structure provides a clean, maintainable, and scalable foundation for the Synthetic Pair Deviation Engine.*
