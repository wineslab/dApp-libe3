/**
 * @file custom_service_model.cpp
 * @brief Example of implementing a custom Service Model
 *
 * Demonstrates how to create a custom ServiceModel implementation
 * that can provide RAN-specific functionality to dApps.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libe3/libe3.hpp>
#include <iostream>
#include <atomic>
#include <thread>
#include <csignal>
#include <cstring>

static std::atomic<bool> g_running{true};

void signal_handler(int) {
    g_running = false;
}

/**
 * @brief Example Key Performance Metrics (KPM) Service Model
 *
 * This service model simulates collecting and reporting KPM data
 * such as throughput, latency, and cell load.
 */
class KpmServiceModel : public libe3::ServiceModel {
public:
    static constexpr uint32_t RAN_FUNCTION_ID = 100;
    
    KpmServiceModel() = default;
    
    uint32_t id() const override { return RAN_FUNCTION_ID; }
    std::string name() const override { return "KPM"; }
    std::string version() const override { return "2.0.0"; }
    
    std::vector<uint32_t> ran_function_ids() const override {
        return {RAN_FUNCTION_ID};
    }
    
    libe3::ErrorCode start() override {
        std::cout << "[KPM] Starting KPM Service Model\n";
        running_ = true;
        
        // Start background data collection thread
        collector_thread_ = std::thread([this]() {
            collect_metrics();
        });
        
        return libe3::ErrorCode::SUCCESS;
    }
    
    libe3::ErrorCode stop() override {
        std::cout << "[KPM] Stopping KPM Service Model\n";
        running_ = false;
        
        if (collector_thread_.joinable()) {
            collector_thread_.join();
        }
        
        return libe3::ErrorCode::SUCCESS;
    }
    
    bool is_running() const override {
        return running_;
    }
    
    std::optional<std::vector<uint8_t>> poll_indication_data() override {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (!pending_data_.empty()) {
            auto data = std::move(pending_data_);
            pending_data_.clear();
            return data;
        }
        return std::nullopt;
    }
    
    libe3::ErrorCode handle_control_action(const std::vector<uint8_t>& data) override {
        std::cout << "[KPM] Received control action (" << data.size() << " bytes)\n";
        
        // Parse and apply control action
        // In a real implementation, this would modify data collection parameters
        
        return libe3::ErrorCode::SUCCESS;
    }
    
    void set_indication_callback(IndicationCallback callback) override {
        indication_callback_ = std::move(callback);
    }

private:
    void collect_metrics() {
        uint64_t sequence = 0;
        
        while (running_) {
            // Simulate collecting metrics every second
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            if (!running_) break;
            
            // Create simulated KPM data
            std::vector<uint8_t> kpm_data = create_kpm_report(sequence++);
            
            // Store for polling
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                pending_data_ = kpm_data;
            }
            
            // Also trigger callback if set
            if (indication_callback_) {
                auto now = std::chrono::system_clock::now();
                auto timestamp = static_cast<uint64_t>(
                    now.time_since_epoch().count());
                indication_callback_(RAN_FUNCTION_ID, kpm_data, timestamp);
            }
        }
    }
    
    std::vector<uint8_t> create_kpm_report(uint64_t sequence) {
        // Create a simple KPM report structure
        // In a real implementation, this would follow the O-RAN KPM ASN.1 schema
        
        struct KpmReport {
            uint64_t sequence_number;
            uint32_t dl_throughput_kbps;
            uint32_t ul_throughput_kbps;
            uint32_t latency_ms;
            uint32_t cell_load_percent;
        };
        
        KpmReport report;
        report.sequence_number = sequence;
        report.dl_throughput_kbps = 50000 + (sequence % 10000);  // Simulate varying throughput
        report.ul_throughput_kbps = 25000 + (sequence % 5000);
        report.latency_ms = 10 + (sequence % 5);
        report.cell_load_percent = 30 + (sequence % 40);
        
        std::vector<uint8_t> data(sizeof(report));
        std::memcpy(data.data(), &report, sizeof(report));
        
        return data;
    }
    
    std::atomic<bool> running_{false};
    std::thread collector_thread_;
    std::mutex data_mutex_;
    std::vector<uint8_t> pending_data_;
    IndicationCallback indication_callback_;
};

/**
 * @brief Example Radio Resource Control (RC) Service Model
 *
 * This service model simulates radio resource control capabilities
 * like handover management and cell configuration.
 */
class RcServiceModel : public libe3::ServiceModel {
public:
    static constexpr uint32_t RAN_FUNCTION_ID = 200;
    
    uint32_t id() const override { return RAN_FUNCTION_ID; }
    std::string name() const override { return "RC"; }
    std::string version() const override { return "1.0.0"; }
    
    std::vector<uint32_t> ran_function_ids() const override {
        return {RAN_FUNCTION_ID};
    }
    
    libe3::ErrorCode start() override {
        std::cout << "[RC] Starting RC Service Model\n";
        running_ = true;
        return libe3::ErrorCode::SUCCESS;
    }
    
    libe3::ErrorCode stop() override {
        std::cout << "[RC] Stopping RC Service Model\n";
        running_ = false;
        return libe3::ErrorCode::SUCCESS;
    }
    
    bool is_running() const override { return running_; }
    
    std::optional<std::vector<uint8_t>> poll_indication_data() override {
        return std::nullopt;  // RC typically responds to control actions only
    }
    
    libe3::ErrorCode handle_control_action(const std::vector<uint8_t>& data) override {
        std::cout << "[RC] Executing control action (" << data.size() << " bytes)\n";
        
        // Parse control action type and execute
        // Examples: trigger handover, modify cell parameters, etc.
        
        return libe3::ErrorCode::SUCCESS;
    }
    
    void set_indication_callback(IndicationCallback callback) override {
        indication_callback_ = std::move(callback);
    }

private:
    std::atomic<bool> running_{false};
    IndicationCallback indication_callback_;
};

int main() {
    std::cout << "libe3 Custom Service Model Example\n";
    std::cout << "Version: " << LIBE3_VERSION_STRING << "\n\n";
    
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Configure the E3 agent
    libe3::E3Config config;
    config.ran_identifier = "custom-sm-ran";
    config.simulation_mode = true;
    
    // Create the agent
    libe3::E3Agent agent(std::move(config));
    
    // Register custom service models
    std::cout << "Registering service models...\n";
    
    auto kpm_result = agent.register_sm(std::make_unique<KpmServiceModel>());
    if (kpm_result != libe3::ErrorCode::SUCCESS) {
        std::cerr << "Failed to register KPM SM: " 
                  << libe3::error_code_to_string(kpm_result) << "\n";
        return 1;
    }
    std::cout << "  Registered KPM Service Model (ID: " 
              << KpmServiceModel::RAN_FUNCTION_ID << ")\n";
    
    auto rc_result = agent.register_sm(std::make_unique<RcServiceModel>());
    if (rc_result != libe3::ErrorCode::SUCCESS) {
        std::cerr << "Failed to register RC SM: "
                  << libe3::error_code_to_string(rc_result) << "\n";
        return 1;
    }
    std::cout << "  Registered RC Service Model (ID: "
              << RcServiceModel::RAN_FUNCTION_ID << ")\n";
    
    // List available RAN functions
    std::cout << "\nAvailable RAN functions:\n";
    for (auto func_id : agent.get_available_ran_functions()) {
        std::cout << "  - " << func_id << "\n";
    }
    
    // Set up callbacks
    agent.set_control_callback([](uint32_t dapp_id, uint32_t ran_func_id,
                                  const std::vector<uint8_t>& data) {
        std::cout << "Control action: dApp=" << dapp_id 
                  << " ran_func=" << ran_func_id
                  << " size=" << data.size() << "\n";
    });
    
    agent.set_indication_callback([](uint32_t dapp_id, uint32_t ran_func_id,
                                     const std::vector<uint8_t>& data) {
        std::cout << "Indication: dApp=" << dapp_id
                  << " ran_func=" << ran_func_id
                  << " size=" << data.size() << "\n";
    });
    
    // Initialize and start
    auto init_result = agent.init();
    if (init_result != libe3::ErrorCode::SUCCESS) {
        std::cerr << "Failed to initialize: "
                  << libe3::error_code_to_string(init_result) << "\n";
        return 1;
    }
    
    auto start_result = agent.start();
    if (start_result != libe3::ErrorCode::SUCCESS) {
        std::cerr << "Failed to start: "
                  << libe3::error_code_to_string(start_result) << "\n";
        return 1;
    }
    
    std::cout << "\nAgent running. Press Ctrl+C to stop...\n";
    
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "[Stats] dApps: " << agent.dapp_count()
                  << ", Subscriptions: " << agent.subscription_count() << "\n";
    }
    
    std::cout << "\nShutting down...\n";
    agent.stop();
    std::cout << "Done.\n";
    
    return 0;
}
