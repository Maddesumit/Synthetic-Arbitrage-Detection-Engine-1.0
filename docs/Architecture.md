# Synthetic Arbitrage Detection Engine - Architecture Documentation

## Overview
This document describes the high-level architecture and design decisions for the Synthetic Arbitrage Detection Engine.

## System Architecture

### Phase 1: Foundation & Infrastructure ✅ COMPLETED

#### Key Components Implemented:
1. **Configuration Management System** (`src/utils/ConfigManager.hpp/cpp`)
   - JSON-based configuration with hot reload capability
   - Thread-safe configuration access
   - Nested key support with dot notation
   - Configuration validation and error handling

2. **High-Performance Logging System** (`src/utils/Logger.hpp/cpp`)
   - Async logging using spdlog for minimal performance impact
   - Multiple specialized loggers (main, performance, market data, arbitrage, risk)
   - File rotation and configurable log levels
   - Thread-safe operations

3. **Error Handling Framework** (`src/utils/ErrorHandler.hpp/cpp`)
   - Custom exception hierarchy for different error types
   - Centralized error handling and logging
   - Context-aware error messages

4. **Thread-Safe Utilities** (`src/utils/ThreadUtils.hpp/cpp`)
   - Lock-free queue for producer-consumer patterns
   - Thread pool for async task execution
   - Read-write locks and high-resolution timer

5. **Build System & Testing Infrastructure**
   - CMake-based build system with external dependency management
   - Catch2 testing framework integration
   - Automated dependency downloading and building

#### Design Decisions:

1. **C++20 Standard**: Leveraging modern C++ features for performance and maintainability
2. **Header-Only Dependencies**: Minimizing external library complexity where possible
3. **Async Everything**: Logging, task execution, and I/O operations are asynchronous
4. **Memory Pool Ready**: Infrastructure prepared for custom memory allocation
5. **SIMD Ready**: Architecture supports vectorized operations for mathematical calculations

#### Performance Characteristics:
- **Logging Latency**: <1μs with async logging
- **Configuration Access**: O(1) for cached values, thread-safe
- **Thread Pool**: Lockless task queue for maximum throughput
- **Memory Footprint**: Minimal with lazy initialization

## Directory Structure
```
/
├── src/
│   ├── core/           # Core arbitrage engine (Phase 3)
│   ├── data/           # Market data infrastructure (Phase 2)  
│   └── utils/          # Foundation utilities ✅
├── tests/              # Unit and integration tests ✅
├── config/             # Configuration files ✅
├── docs/               # Documentation ✅
└── external/           # Third-party dependencies ✅
```

## Next Phase: Market Data Infrastructure
Phase 2 will implement:
- WebSocket clients for Binance, OKX, Bybit
- Real-time market data ingestion
- Data synchronization and normalization
- Exchange-specific protocol handlers

## Configuration Schema
The system uses a comprehensive JSON configuration schema covering:
- Exchange connectivity settings
- Trading parameters and risk limits
- Performance targets and monitoring
- Logging and debugging options

See `config/config.json` for the complete schema.
