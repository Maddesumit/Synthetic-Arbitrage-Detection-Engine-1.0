#!/bin/bash

# VS Code Performance Optimization Script for Large C++ Projects
# This script provides a permanent solution for VS Code hanging issues

echo "🚀 Starting VS Code Performance Optimization..."

# 1. Kill VS Code processes
echo "1. Stopping VS Code processes..."
pkill -f "Visual Studio Code" 2>/dev/null || true
pkill -f "Code" 2>/dev/null || true
sleep 2

# 2. Clear VS Code caches and databases
echo "2. Clearing VS Code caches..."
rm -rf .vscode/browse.vc.db*
rm -rf .vscode/ipch
rm -rf .vscode/.ropeproject
rm -rf .vscode/settings.json.bak

# 3. Clean build artifacts that can cause indexing issues
echo "3. Cleaning build artifacts..."
rm -rf build/
rm -rf logs/*.log
rm -rf test_logs/

# 4. Create optimized .vscode/settings.json
echo "4. Creating optimized VS Code settings..."
cat > .vscode/settings.json << 'EOF'
{
    "C_Cpp.intelliSenseEngine": "Default",
    "C_Cpp.intelliSenseUpdateDelay": 2000,
    "C_Cpp.workspaceParsingPriority": "medium",
    "C_Cpp.maxCachedProcesses": 2,
    "C_Cpp.maxMemory": 2048,
    "C_Cpp.loggingLevel": "Error",
    "C_Cpp.enhancedColorization": "Disabled",
    "C_Cpp.autocomplete": "Disabled",
    "C_Cpp.errorSquiggles": "Disabled",
    "C_Cpp.dimInactiveRegions": false,
    "C_Cpp.experimentalFeatures": "Disabled",
    
    "files.watcherExclude": {
        "**/build/**": true,
        "**/logs/**": true,
        "**/test_logs/**": true,
        "**/external/**": true,
        "**/.git/**": true,
        "**/node_modules/**": true,
        "**/*.log": true,
        "**/.vscode/browse.vc.db*": true,
        "**/.vscode/ipch/**": true
    },
    
    "search.exclude": {
        "**/build": true,
        "**/logs": true,
        "**/test_logs": true,
        "**/external": true,
        "**/.git": true,
        "**/node_modules": true,
        "**/*.log": true
    },
    
    "files.exclude": {
        "**/build": false,
        "**/logs": true,
        "**/test_logs": true,
        "**/external": false,
        "**/.git": false,
        "**/*.log": true,
        "**/.vscode/browse.vc.db*": true,
        "**/.vscode/ipch": true
    },
    
    "editor.quickSuggestions": {
        "other": false,
        "comments": false,
        "strings": false
    },
    "editor.suggestOnTriggerCharacters": false,
    "editor.acceptSuggestionOnEnter": "off",
    "editor.tabCompletion": "off",
    "editor.parameterHints.enabled": false,
    "editor.hover.enabled": false,
    "editor.lightbulb.enabled": false,
    
    "extensions.autoUpdate": false,
    "extensions.autoCheckUpdates": false,
    
    "telemetry.telemetryLevel": "off",
    "workbench.enableExperiments": false,
    "workbench.settings.enableNaturalLanguageSearch": false,
    
    "git.enabled": true,
    "git.autorefresh": false,
    "git.autoRepositoryDetection": false,
    
    "typescript.suggest.enabled": false,
    "javascript.suggest.enabled": false,
    
    "cmake.configureOnOpen": false,
    "cmake.configureOnEdit": false,
    "cmake.autoSelectActiveFolder": false,
    
    "files.associations": {
        "*.hpp": "cpp",
        "*.cpp": "cpp",
        "*.h": "c",
        "*.c": "c"
    }
}
EOF

# 5. Create optimized .vscode/c_cpp_properties.json
echo "5. Creating optimized C++ IntelliSense configuration..."
cat > .vscode/c_cpp_properties.json << 'EOF'
{
    "configurations": [
        {
            "name": "Mac",
            "includePath": [
                "${workspaceFolder}/**",
                "${workspaceFolder}/src/**",
                "${workspaceFolder}/external/**",
                "/usr/local/include",
                "/opt/homebrew/include"
            ],
            "defines": [],
            "macFrameworkPath": [
                "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks"
            ],
            "compilerPath": "/usr/bin/clang",
            "cStandard": "c17",
            "cppStandard": "c++17",
            "intelliSenseMode": "macos-clang-x64",
            "configurationProvider": "ms-vscode.cmake-tools",
            "mergeConfigurations": false,
            "browse": {
                "path": [
                    "${workspaceFolder}/src",
                    "/usr/local/include",
                    "/opt/homebrew/include"
                ],
                "limitSymbolsToIncludedHeaders": true,
                "databaseFilename": "${workspaceFolder}/.vscode/browse.vc.db"
            }
        }
    ],
    "version": 4
}
EOF

# 6. Create .vscodeignore to exclude unnecessary files
echo "6. Creating .vscodeignore file..."
cat > .vscodeignore << 'EOF'
# Build directories
build/**
logs/**
test_logs/**

# VS Code databases
.vscode/browse.vc.db*
.vscode/ipch/**

# Log files
*.log

# Git
.git/**

# External dependencies (already built)
external/**

# Temporary files
*.tmp
*.temp
*~

# OS files
.DS_Store
Thumbs.db

# Node modules (if any)
node_modules/**

# CMake build files
CMakeFiles/**
CMakeCache.txt
cmake_install.cmake
Makefile

# Object files
*.o
*.obj
*.so
*.dylib
*.dll
*.a
*.lib

# Executables
bin/**
*.exe

# Documentation build
docs/build/**
docs/_build/**
EOF

# 7. Update workspace settings for better performance
echo "7. Creating workspace configuration..."
cat > arbitrage-engine.code-workspace << 'EOF'
{
    "folders": [
        {
            "name": "Arbitrage Engine",
            "path": "."
        }
    ],
    "settings": {
        "C_Cpp.intelliSenseEngine": "Default",
        "C_Cpp.intelliSenseUpdateDelay": 2000,
        "C_Cpp.workspaceParsingPriority": "medium",
        "C_Cpp.maxCachedProcesses": 1,
        "C_Cpp.maxMemory": 1024,
        "C_Cpp.loggingLevel": "Error",
        "C_Cpp.enhancedColorization": "Disabled",
        "C_Cpp.autocomplete": "Disabled",
        "C_Cpp.errorSquiggles": "Disabled",
        
        "files.watcherExclude": {
            "**/build/**": true,
            "**/logs/**": true,
            "**/test_logs/**": true,
            "**/external/**": true,
            "**/.git/**": true,
            "**/*.log": true,
            "**/.vscode/browse.vc.db*": true
        },
        
        "editor.quickSuggestions": false,
        "editor.suggestOnTriggerCharacters": false,
        "editor.parameterHints.enabled": false,
        "editor.hover.enabled": false,
        
        "git.autorefresh": false,
        "extensions.autoUpdate": false,
        "telemetry.telemetryLevel": "off"
    },
    "extensions": {
        "recommendations": [
            "ms-vscode.cpptools",
            "ms-vscode.cmake-tools"
        ]
    }
}
EOF

# 8. Clean git index for performance
echo "8. Optimizing git configuration..."
git config core.preloadindex true
git config core.fscache true
git config gc.auto 256

# 9. Set macOS file system optimizations
echo "9. Applying macOS optimizations..."
# Disable spotlight indexing for this directory (requires admin)
if command -v mdutil &> /dev/null; then
    echo "Consider running: sudo mdutil -i off '/Users/sumitmadde/Desktop/Synthetic Pair Deviation Engine'"
fi

echo "✅ VS Code Performance Optimization Complete!"
echo ""
echo "📋 NEXT STEPS:"
echo "1. Restart VS Code completely"
echo "2. Open ONLY the workspace file: arbitrage-engine.code-workspace"
echo "3. When prompted, allow IntelliSense to configure (one-time only)"
echo "4. Disable unnecessary extensions for this workspace"
echo ""
echo "🔧 MANUAL OPTIMIZATIONS TO APPLY:"
echo "1. VS Code → Preferences → Settings → Search 'telemetry' → Disable all"
echo "2. VS Code → Preferences → Settings → Search 'experiments' → Disable"
echo "3. Extensions → Disable unused extensions for this workspace"
echo "4. View → Command Palette → 'Developer: Reload Window' if issues persist"
echo ""
echo "⚡ PERFORMANCE TIPS:"
echo "- Use 'Go to Symbol' (Cmd+Shift+O) instead of file explorer"
echo "- Use 'Go to File' (Cmd+P) for quick file navigation"
echo "- Avoid opening too many files simultaneously"
echo "- Use terminal for building instead of VS Code tasks"
echo ""
echo "🚨 IF STILL HANGING:"
echo "1. Run: pkill -f 'Visual Studio Code'"
echo "2. Delete .vscode/browse.vc.db*"
echo "3. Restart VS Code and open workspace file"
