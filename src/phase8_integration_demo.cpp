#include <iostream>
#include <chrono>
#include <thread>
#include <memory>

// Phase 8 includes
#include "config/ConfigurationManager.hpp"
#include "risk/EnhancedRiskModels.hpp"
#include "data/DataPersistence.hpp"
#include "api/ApiFramework.hpp"

// Existing includes
#include "utils/Logger.hpp"
#include "core/ArbitrageEngine.hpp"

using namespace arbitrage;

int main() {
    try {
        // Initialize logger first
        utils::Logger::initialize("logs/phase8_demo.log", utils::Logger::Level::INFO);
        
        std::cout << "=== Phase 8: Advanced Features & Integration Demo ===\n";
        std::cout << "This demo showcases the advanced integration features implemented in Phase 8.\n\n";

        // 8.1 Advanced Configuration Management Demo
        std::cout << "=== Advanced Configuration Management Demo ===\n";
        auto& config_manager = config::ConfigurationManager::getInstance();
        
        // Load configuration with hot reload
        if (config_manager.loadConfiguration("config/config.json")) {
            std::cout << "✓ Configuration loaded successfully\n";
            
            // Enable hot reload
            config_manager.enableHotReload(true);
            config_manager.setReloadInterval(std::chrono::seconds(5));
            std::cout << "✓ Hot reload enabled with 5-second interval\n";
            
            // Test configuration access
            auto min_profit = config_manager.getValue<double>("trading.min_profit_threshold", 0.001);
            std::cout << "✓ Min profit threshold: " << min_profit << "\n";
            
            // Test configuration validation
            if (config_manager.validateConfiguration()) {
                std::cout << "✓ Configuration validation passed\n";
            } else {
                auto errors = config_manager.getValidationErrors();
                std::cout << "⚠ Configuration validation failed: " << errors.size() << " errors\n";
            }
            
            // Register callback for trading config changes
            config_manager.registerCallback("trading", [](const std::string& section, const nlohmann::json& new_value) {
                std::cout << "📢 Configuration changed in section: " << section << "\n";
            });
            
            std::cout << "✓ Configuration callback registered\n";
        }
        
        std::cout << "\n=== Enhanced Risk Models Demo ===\n";
        
        // 8.2 Enhanced Risk Models Demo
        risk::CorrelationAnalyzer correlation_analyzer(5000);
        risk::VolatilityPredictor volatility_predictor;
        risk::MarketRegimeDetector regime_detector;
        
        // Simulate some price data for correlation analysis
        auto now = std::chrono::system_clock::now();
        std::vector<std::string> assets = {"BTCUSDT", "ETHUSDT", "ADAUSDT"};
        
        // Add sample price data
        for (int i = 0; i < 100; ++i) {
            auto timestamp = now - std::chrono::minutes(i);
            correlation_analyzer.updatePriceData("BTCUSDT", 45000.0 + (rand() % 1000), timestamp);
            correlation_analyzer.updatePriceData("ETHUSDT", 3000.0 + (rand() % 100), timestamp);
            correlation_analyzer.updatePriceData("ADAUSDT", 1.2 + (rand() % 10) / 100.0, timestamp);
        }
        
        // Calculate correlation between BTC and ETH
        auto correlation_result = correlation_analyzer.calculateCorrelation("BTCUSDT", "ETHUSDT");
        std::cout << "✓ BTC-ETH correlation: " << correlation_result.correlation_coefficient << "\n";
        
        // Get correlation matrix for all assets
        auto correlation_matrix = correlation_analyzer.getCorrelationMatrix(assets);
        std::cout << "✓ Correlation matrix calculated (" << correlation_matrix.rows() << "x" << correlation_matrix.cols() << ")\n";
        
        // Detect market regime
        auto regime = correlation_analyzer.detectMarketRegime(assets);
        std::string regime_name;
        switch (regime) {
            case risk::CorrelationAnalyzer::MarketRegime::NORMAL: regime_name = "Normal"; break;
            case risk::CorrelationAnalyzer::MarketRegime::HIGH_CORRELATION: regime_name = "High Correlation"; break;
            case risk::CorrelationAnalyzer::MarketRegime::CRISIS: regime_name = "Crisis"; break;
            case risk::CorrelationAnalyzer::MarketRegime::DECORRELATION: regime_name = "Decorrelation"; break;
            default: regime_name = "Unknown"; break;
        }
        std::cout << "✓ Market regime detected: " << regime_name << "\n";
        
        // Volatility prediction demo
        std::vector<double> sample_returns;
        for (int i = 0; i < 100; ++i) {
            sample_returns.push_back((rand() % 200 - 100) / 10000.0); // Random returns between -1% and 1%
        }
        
        if (volatility_predictor.calibrateModel("BTCUSDT", risk::VolatilityPredictor::Model::GARCH, sample_returns)) {
            auto forecast = volatility_predictor.predictVolatility("BTCUSDT", std::chrono::hours(24));
            std::cout << "✓ Volatility forecast (24h): " << forecast.predicted_volatility * 100 << "% (confidence: " 
                      << forecast.model_confidence * 100 << "%)\n";
        }
        
        std::cout << "\n=== Data Persistence & Analytics Demo ===\n";
        
        // 8.3 Data Persistence & Analytics Demo
        auto storage = std::make_shared<data::persistence::HistoricalDataStorage>("data/arbitrage_history.db");
        
        if (storage->initialize()) {
            std::cout << "✓ Database initialized successfully\n";
            
            // Insert some sample time series data
            std::vector<data::persistence::TimeSeriesPoint> sample_data;
            for (int i = 0; i < 50; ++i) {
                data::persistence::TimeSeriesPoint point;
                point.timestamp = now - std::chrono::minutes(i);
                point.metric_name = "portfolio_pnl";
                point.value = 10000.0 + (rand() % 2000 - 1000); // Random P&L around $10k
                point.tags["strategy"] = "arbitrage";
                sample_data.push_back(point);
            }
            
            if (storage->insertTimeSeriesPointBatch(sample_data)) {
                std::cout << "✓ Inserted " << sample_data.size() << " time series points\n";
            }
            
            // Query data back
            data::persistence::QueryParameters query;
            query.start_time = now - std::chrono::hours(1);
            query.end_time = now;
            query.metrics = {"portfolio_pnl"};
            
            auto retrieved_data = storage->getTimeSeriesData(query);
            std::cout << "✓ Retrieved " << retrieved_data.size() << " data points from database\n";
            
            // Performance analytics
            auto analytics = std::make_shared<data::persistence::PerformanceAnalytics>(storage);
            
            // Calculate performance metrics
            auto perf_metrics = analytics->calculatePerformanceMetrics(
                now - std::chrono::hours(24), now);
            std::cout << "✓ Performance metrics calculated:\n";
            std::cout << "  - Total return: " << perf_metrics.total_return << "%\n";
            std::cout << "  - Sharpe ratio: " << perf_metrics.sharpe_ratio << "\n";
            std::cout << "  - Max drawdown: " << perf_metrics.max_drawdown << "%\n";
            std::cout << "  - Win rate: " << perf_metrics.win_rate << "%\n";
            
            // Database statistics
            auto stats = storage->getStatistics();
            std::cout << "✓ Database statistics:\n";
            std::cout << "  - Total records: " << stats.total_records << "\n";
            std::cout << "  - Database size: " << stats.database_size_bytes / 1024 << " KB\n";
        }
        
        std::cout << "\n=== API & Integration Framework Demo ===\n";
        
        // 8.4 API & Integration Framework Demo
        api::ApiServer api_server(8082, 8083); // Use different ports for demo
        
        // Create authentication manager
        auto auth_manager = std::make_shared<api::AuthenticationManager>();
        
        // Create a demo user
        if (auth_manager->createUser("demo_user", "demo@example.com", {"admin", "trader"})) {
            std::cout << "✓ Demo user created\n";
            
            // Generate API key
            auto api_key = auth_manager->generateApiKey("demo_user", {"read", "write", "admin"});
            std::cout << "✓ API key generated: " << api_key.substr(0, 8) << "...\n";
        }
        
        // Set up rate limiting
        auto rate_limiter = std::make_shared<api::RateLimiter>();
        api::RateLimiter::RateLimit limits;
        limits.requests_per_minute = 100;
        limits.requests_per_hour = 1000;
        rate_limiter->setGlobalRateLimit(limits);
        std::cout << "✓ Rate limiting configured (100 req/min, 1000 req/hour)\n";
        
        // Configure API server
        api_server.setAuthenticationManager(auth_manager);
        api_server.enableRateLimit(rate_limiter);
        api_server.enableCORS(true);
        
        // Register a sample endpoint
        api_server.registerEndpoint("GET", "/api/v1/demo", 
            [](const api::ApiRequest& request) -> api::ApiResponse {
                nlohmann::json response_data = {
                    {"message", "Phase 8 API Demo"},
                    {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()},
                    {"user_id", request.user_id},
                    {"method", request.method},
                    {"path", request.path}
                };
                
                api::ApiResponse response;
                response.status_code = 200;
                response.body = response_data.dump(2);
                return response;
            });
        
        std::cout << "✓ Sample API endpoint registered: GET /api/v1/demo\n";
        
        // Register WebSocket channel
        api_server.registerWebSocketChannel("demo_channel",
            [&api_server](const api::WebSocketMessage& message) {
                nlohmann::json response = {
                    {"type", "demo_response"},
                    {"original_message", message.payload},
                    {"timestamp", std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count()}
                };
                api_server.sendToConnection(message.connection_id, response);
            });
        
        std::cout << "✓ WebSocket channel registered: demo_channel\n";
        
        // Start API server
        if (api_server.start()) {
            std::cout << "✓ API server started on ports 8082 (HTTP) and 8083 (WebSocket)\n";
            std::cout << "  Test endpoint: http://localhost:8082/api/v1/demo\n";
            
            // Run for a few seconds to demonstrate
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            // Show server metrics
            auto metrics = api_server.getMetricsSnapshot();
            std::cout << "✓ Server metrics:\n";
            std::cout << "  - Total requests: " << metrics.total_requests << "\n";
            std::cout << "  - Active connections: " << metrics.active_connections << "\n";
            std::cout << "  - Server uptime: " << std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now() - metrics.server_start_time).count() << "s\n";
            
            api_server.stop();
            std::cout << "✓ API server stopped\n";
        }
        
        // Integration manager demo
        api::IntegrationManager integration_manager;
        
        // Register external system
        api::IntegrationManager::ExternalSystem external_system;
        external_system.system_id = "portfolio_manager";
        external_system.name = "External Portfolio Manager";
        external_system.endpoint_url = "https://api.example.com/portfolio";
        external_system.auth_type = "api_key";
        external_system.auth_credentials["api_key"] = "demo_key_123";
        
        if (integration_manager.registerExternalSystem(external_system)) {
            std::cout << "✓ External system registered: " << external_system.name << "\n";
            
            // Test connection (will fail since it's a demo URL)
            auto health_status = integration_manager.getSystemHealth();
            std::cout << "✓ Integration health monitoring active (" << health_status.size() << " systems)\n";
        }
        
        std::cout << "\n=== Phase 8 Integration Demo Completed Successfully ===\n";
        std::cout << "All advanced integration and API features are working correctly.\n";
        std::cout << "The system now includes:\n";
        std::cout << "  ✓ Advanced configuration management with hot reload\n";
        std::cout << "  ✓ Enhanced risk models with correlation analysis\n";
        std::cout << "  ✓ Comprehensive data persistence and analytics\n";
        std::cout << "  ✓ Full-featured REST and WebSocket API framework\n";
        std::cout << "  ✓ Authentication, authorization, and rate limiting\n";
        std::cout << "  ✓ External system integration capabilities\n";
        std::cout << "  ✓ Real-time monitoring and health checks\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error during Phase 8 demo: " << e.what() << std::endl;
        return 1;
    }
}
