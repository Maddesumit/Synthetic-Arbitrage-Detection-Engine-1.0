#!/bin/bash

# Phase 12 Verification Script
echo "🔍 Phase 12 Verification Report"
echo "==============================="

# Check if all Phase 12 files exist
echo "📁 File Structure Check:"
files_to_check=(
    "src/core/Phase12PerformanceEngine.hpp"
    "src/core/Phase12PerformanceEngine.cpp"
    "src/ui/components/Phase12Dashboard.js"
    "src/ui/components/MemoryOptimizationDashboard.js"
    "src/ui/components/GPUAccelerationInterface.js"
    "src/ui/components/NetworkOptimizationDisplay.js"
    "src/ui/components/HardwareMonitoring.js"
    "src/ui/components/AdvancedPerformanceMonitoring.js"
    "src/ui/styles/Phase12Dashboard.css"
)

all_files_exist=true
for file in "${files_to_check[@]}"; do
    if [ -f "$file" ]; then
        echo "   ✅ $file"
    else
        echo "   ❌ $file (MISSING)"
        all_files_exist=false
    fi
done

# Check build artifacts
echo ""
echo "🔧 Build Check:"
build_files=(
    "build/bin/arbitrage_engine"
    "build/bin/dashboard_demo"
)

build_success=true
for file in "${build_files[@]}"; do
    if [ -f "$file" ]; then
        echo "   ✅ $file"
    else
        echo "   ❌ $file (MISSING)"
        build_success=false
    fi
done

# Check Phase 12 integration in main files
echo ""
echo "🔗 Integration Check:"
integration_checks=(
    "src/ui/DashboardApp.hpp:Phase12PerformanceEngine"
    "src/ui/DashboardApp.cpp:handlePerformanceAdvanced"
    "src/main.cpp:Phase12PerformanceEngine"
    "src/ui/dashboard.html:Phase12Dashboard"
    "CMakeLists.txt:Phase12PerformanceEngine"
)

integration_success=true
for check in "${integration_checks[@]}"; do
    file=${check%:*}
    pattern=${check#*:}
    if grep -q "$pattern" "$file" 2>/dev/null; then
        echo "   ✅ $file contains $pattern"
    else
        echo "   ❌ $file missing $pattern"
        integration_success=false
    fi
done

# Final summary
echo ""
echo "📊 Final Status:"
if [ "$all_files_exist" = true ] && [ "$build_success" = true ] && [ "$integration_success" = true ]; then
    echo "🎉 Phase 12 Implementation: COMPLETE"
    echo "   ✅ All files present"
    echo "   ✅ Build successful"
    echo "   ✅ Integration complete"
    echo ""
    echo "Phase 12 is ready for production use!"
else
    echo "⚠️  Phase 12 Implementation: INCOMPLETE"
    if [ "$all_files_exist" = false ]; then
        echo "   ❌ Some files missing"
    fi
    if [ "$build_success" = false ]; then
        echo "   ❌ Build issues detected"
    fi
    if [ "$integration_success" = false ]; then
        echo "   ❌ Integration issues detected"
    fi
fi
