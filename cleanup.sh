#!/bin/bash

# Project Cleanup Script
# This script cleans up build artifacts, logs, and temporary files

set -e

echo "🧹 Cleaning up Synthetic Arbitrage Detection Engine"
echo "=================================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to remove directory if it exists
remove_dir() {
    if [ -d "$1" ]; then
        echo -e "${YELLOW}Removing directory: $1${NC}"
        rm -rf "$1"
        echo -e "${GREEN}✓ Removed $1${NC}"
    else
        echo -e "${YELLOW}Directory $1 does not exist${NC}"
    fi
}

# Function to remove file if it exists
remove_file() {
    if [ -f "$1" ]; then
        echo -e "${YELLOW}Removing file: $1${NC}"
        rm -f "$1"
        echo -e "${GREEN}✓ Removed $1${NC}"
    else
        echo -e "${YELLOW}File $1 does not exist${NC}"
    fi
}

# Clean build artifacts
echo -e "${YELLOW}Cleaning build artifacts...${NC}"
remove_dir "build"
remove_dir "bin"
remove_dir "lib"
remove_dir "out"

# Clean CMake files
echo -e "${YELLOW}Cleaning CMake files...${NC}"
remove_file "CMakeCache.txt"
remove_dir "CMakeFiles"
remove_file "cmake_install.cmake"
remove_file "Makefile"

# Clean logs
echo -e "${YELLOW}Cleaning log files...${NC}"
remove_dir "logs"
find . -name "*.log" -type f -delete 2>/dev/null || true

# Clean temporary files
echo -e "${YELLOW}Cleaning temporary files...${NC}"
find . -name "*.tmp" -type f -delete 2>/dev/null || true
find . -name "*.temp" -type f -delete 2>/dev/null || true
find . -name "*.bak" -type f -delete 2>/dev/null || true
find . -name "*~" -type f -delete 2>/dev/null || true

# Clean compiled objects
echo -e "${YELLOW}Cleaning compiled objects...${NC}"
find . -name "*.o" -type f -delete 2>/dev/null || true
find . -name "*.obj" -type f -delete 2>/dev/null || true
find . -name "*.so" -type f -delete 2>/dev/null || true
find . -name "*.dll" -type f -delete 2>/dev/null || true
find . -name "*.dylib" -type f -delete 2>/dev/null || true
find . -name "*.a" -type f -delete 2>/dev/null || true
find . -name "*.lib" -type f -delete 2>/dev/null || true

# Clean executables
echo -e "${YELLOW}Cleaning executables...${NC}"
find . -name "*.exe" -type f -delete 2>/dev/null || true
find . -name "*.out" -type f -delete 2>/dev/null || true
find . -name "*.app" -type f -delete 2>/dev/null || true

# Clean IDE files
echo -e "${YELLOW}Cleaning IDE files...${NC}"
find . -name ".DS_Store" -type f -delete 2>/dev/null || true
find . -name "Thumbs.db" -type f -delete 2>/dev/null || true
find . -name "*.swp" -type f -delete 2>/dev/null || true
find . -name "*.swo" -type f -delete 2>/dev/null || true

# Clean node modules (if any)
echo -e "${YELLOW}Cleaning Node.js artifacts...${NC}"
remove_dir "node_modules"
remove_file "package-lock.json"
remove_file "yarn.lock"

# Clean deployment artifacts
echo -e "${YELLOW}Cleaning deployment artifacts...${NC}"
remove_dir "appwrite-deployment"
remove_file "docker-compose.yml"
remove_file ".env"
remove_file ".env.local"

# Clean test artifacts
echo -e "${YELLOW}Cleaning test artifacts...${NC}"
find . -name "test_*.json" -type f -delete 2>/dev/null || true
find . -name "test_*.log" -type f -delete 2>/dev/null || true

# Final summary
echo ""
echo -e "${GREEN}🎉 Cleanup completed successfully!${NC}"
echo ""
echo "Cleaned up:"
echo "- Build artifacts (build/, bin/, lib/)"
echo "- CMake generated files"
echo "- Log files and temporary files"
echo "- Compiled objects and executables"
echo "- IDE-specific files"
echo "- Test artifacts"
echo "- Deployment artifacts"
echo ""
echo "Project is now clean and ready for building!"
echo ""
echo "To rebuild the project, run:"
echo "  ./build.sh"
echo ""
echo "Or manually:"
echo "  mkdir build && cd build"
echo "  cmake .. -DCMAKE_BUILD_TYPE=Release"
echo "  make -j\$(nproc)"
