#!/bin/bash

# Build script for Synthetic Arbitrage Detection Engine

set -e  # Exit on any error

echo "=== Building Synthetic Arbitrage Detection Engine ==="

# Create build directory
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build the project
echo "Building project..."
if command -v nproc > /dev/null; then
    make -j$(nproc)
else
    # macOS uses sysctl instead of nproc
    make -j$(sysctl -n hw.ncpu)
fi

echo "Build completed successfully!"

# Run tests if requested
if [ "$1" = "test" ]; then
    echo "Running tests..."
    make test
fi

echo "Build script finished."
