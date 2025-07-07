#!/bin/bash

# Phase 12 Integration Test Script
# Tests all Phase 12 API endpoints and UI integration

echo "🚀 Phase 12 Integration Test Suite"
echo "=================================="

# Configuration
DASHBOARD_PORT=8080
BASE_URL="http://localhost:$DASHBOARD_PORT"
TIMEOUT=30
TEST_RESULTS=()

# Start dashboard in background
echo "📡 Starting dashboard server on port $DASHBOARD_PORT..."
cd /Users/sumitmadde/Desktop/Synthetic\ Pair\ Deviation\ Engine

# Kill any existing dashboard processes
pkill -f dashboard_demo 2>/dev/null || true
sleep 2

# Start dashboard
./build/bin/dashboard_demo --port $DASHBOARD_PORT &
DASHBOARD_PID=$!

echo "⏳ Waiting for dashboard to initialize..."
sleep 10

# Function to test API endpoint
test_endpoint() {
    local endpoint=$1
    local method=$2
    local description=$3
    
    echo -n "Testing $description... "
    
    if [[ "$method" == "POST" ]]; then
        response=$(curl -s -X POST -H "Content-Type: application/json" \
                   -d '{"test": true}' \
                   --max-time 10 \
                   "$BASE_URL$endpoint" 2>/dev/null)
    else
        response=$(curl -s --max-time 10 "$BASE_URL$endpoint" 2>/dev/null)
    fi
    
    if [[ $? -eq 0 && -n "$response" ]]; then
        echo "✅ PASS"
        TEST_RESULTS+=("✅ $description: PASS")
        return 0
    else
        echo "❌ FAIL"
        TEST_RESULTS+=("❌ $description: FAIL")
        return 1
    fi
}

# Test basic endpoints
echo ""
echo "🔍 Testing Basic API Endpoints..."
test_endpoint "/api/health" "GET" "Health Check"
test_endpoint "/api/arbitrage" "GET" "Arbitrage Data"
test_endpoint "/api/performance" "GET" "Performance Metrics"

# Test Phase 12 specific endpoints
echo ""
echo "⚡ Testing Phase 12 Performance Optimization Endpoints..."
test_endpoint "/api/performance/advanced" "GET" "Advanced Performance Metrics"
test_endpoint "/api/performance/memory" "GET" "Memory Optimization Status"
test_endpoint "/api/performance/gpu" "GET" "GPU Acceleration Status"
test_endpoint "/api/performance/network" "GET" "Network Optimization Status"
test_endpoint "/api/performance/profiling" "GET" "Performance Profiling Data"
test_endpoint "/api/performance/hardware" "GET" "Hardware Monitoring"

# Test Phase 12 configuration endpoints
echo ""
echo "⚙️ Testing Phase 12 Configuration Endpoints..."
test_endpoint "/api/performance/memory/optimize" "POST" "Memory Optimization Configuration"
test_endpoint "/api/performance/gpu/enable" "POST" "GPU Acceleration Enable"
test_endpoint "/api/performance/network/optimize" "POST" "Network Optimization Configuration"

# Test UI component loading
echo ""
echo "🎨 Testing UI Component Loading..."
test_endpoint "/" "GET" "Dashboard HTML"
test_endpoint "/components/Phase12Dashboard.js" "GET" "Phase 12 Dashboard Component"
test_endpoint "/components/MemoryOptimizationDashboard.js" "GET" "Memory Optimization Component"
test_endpoint "/components/GPUAccelerationInterface.js" "GET" "GPU Acceleration Component"
test_endpoint "/components/NetworkOptimizationDisplay.js" "GET" "Network Optimization Component"
test_endpoint "/components/HardwareMonitoring.js" "GET" "Hardware Monitoring Component"
test_endpoint "/components/AdvancedPerformanceMonitoring.js" "GET" "Advanced Performance Component"
test_endpoint "/styles/Phase12Dashboard.css" "GET" "Phase 12 Styles"

# Cleanup
echo ""
echo "🧹 Cleaning up..."
kill $DASHBOARD_PID 2>/dev/null || true
sleep 2

# Display results
echo ""
echo "📊 Test Results Summary:"
echo "========================"
passed=0
failed=0

for result in "${TEST_RESULTS[@]}"; do
    echo "$result"
    if [[ $result == ✅* ]]; then
        ((passed++))
    else
        ((failed++))
    fi
done

echo ""
echo "📈 Final Score: $passed passed, $failed failed"
echo "Success Rate: $(( passed * 100 / (passed + failed) ))%"

if [[ $failed -eq 0 ]]; then
    echo "🎉 All tests passed! Phase 12 integration is complete."
    exit 0
else
    echo "⚠️  Some tests failed. Please check the implementation."
    exit 1
fi
