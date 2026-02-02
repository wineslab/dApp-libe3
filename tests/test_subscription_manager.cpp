/**
 * @file test_subscription_manager.cpp
 * @brief Unit tests for SubscriptionManager
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "test_framework.hpp"
#include "libe3/subscription_manager.hpp"
#include <thread>
#include <atomic>

using namespace libe3;

TEST(SubscriptionManager_initial_state) {
    SubscriptionManager mgr;
    ASSERT_EQ(mgr.dapp_count(), 0u);
    ASSERT_EQ(mgr.subscription_count(), 0u);
    ASSERT_TRUE(mgr.get_registered_dapps().empty());
}

TEST(SubscriptionManager_register_dapp) {
    SubscriptionManager mgr;
    
    auto result = mgr.register_dapp(100);
    ASSERT_EQ(result, ErrorCode::SUCCESS);
    ASSERT_EQ(mgr.dapp_count(), 1u);
    ASSERT_TRUE(mgr.is_dapp_registered(100));
    ASSERT_FALSE(mgr.is_dapp_registered(200));
}

TEST(SubscriptionManager_register_duplicate) {
    SubscriptionManager mgr;
    
    mgr.register_dapp(100);
    auto result = mgr.register_dapp(100);
    ASSERT_EQ(result, ErrorCode::SUBSCRIPTION_EXISTS);
    ASSERT_EQ(mgr.dapp_count(), 1u);
}

TEST(SubscriptionManager_unregister_dapp) {
    SubscriptionManager mgr;
    
    mgr.register_dapp(100);
    auto result = mgr.unregister_dapp(100);
    ASSERT_EQ(result, ErrorCode::SUCCESS);
    ASSERT_EQ(mgr.dapp_count(), 0u);
    ASSERT_FALSE(mgr.is_dapp_registered(100));
}

TEST(SubscriptionManager_unregister_nonexistent) {
    SubscriptionManager mgr;
    auto result = mgr.unregister_dapp(999);
    ASSERT_EQ(result, ErrorCode::NOT_FOUND);
}

TEST(SubscriptionManager_add_subscription) {
    SubscriptionManager mgr;
    
    mgr.register_dapp(100);
    auto result = mgr.add_subscription(100, 200);
    ASSERT_EQ(result, ErrorCode::SUCCESS);
    ASSERT_EQ(mgr.subscription_count(), 1u);
    ASSERT_TRUE(mgr.is_subscribed(100, 200));
}

TEST(SubscriptionManager_add_subscription_unregistered_dapp) {
    SubscriptionManager mgr;
    auto result = mgr.add_subscription(100, 200);
    ASSERT_EQ(result, ErrorCode::NOT_FOUND);
}

TEST(SubscriptionManager_add_subscription_duplicate) {
    SubscriptionManager mgr;
    mgr.register_dapp(100);
    mgr.add_subscription(100, 200);
    auto result = mgr.add_subscription(100, 200);
    ASSERT_EQ(result, ErrorCode::SUBSCRIPTION_EXISTS);
}

TEST(SubscriptionManager_remove_subscription) {
    SubscriptionManager mgr;
    mgr.register_dapp(100);
    mgr.add_subscription(100, 200);
    
    auto result = mgr.remove_subscription(100, 200);
    ASSERT_EQ(result, ErrorCode::SUCCESS);
    ASSERT_FALSE(mgr.is_subscribed(100, 200));
    ASSERT_EQ(mgr.subscription_count(), 0u);
}

TEST(SubscriptionManager_remove_subscription_not_found) {
    SubscriptionManager mgr;
    mgr.register_dapp(100);
    auto result = mgr.remove_subscription(100, 999);
    ASSERT_EQ(result, ErrorCode::NOT_FOUND);
}

TEST(SubscriptionManager_get_subscribed_dapps) {
    SubscriptionManager mgr;
    mgr.register_dapp(100);
    mgr.register_dapp(101);
    mgr.register_dapp(102);
    
    mgr.add_subscription(100, 500);
    mgr.add_subscription(101, 500);
    mgr.add_subscription(102, 600);
    
    auto subscribers = mgr.get_subscribed_dapps(500);
    ASSERT_EQ(subscribers.size(), 2u);
    
    auto sub600 = mgr.get_subscribed_dapps(600);
    ASSERT_EQ(sub600.size(), 1u);
    
    auto sub999 = mgr.get_subscribed_dapps(999);
    ASSERT_TRUE(sub999.empty());
}

TEST(SubscriptionManager_get_dapp_subscriptions) {
    SubscriptionManager mgr;
    mgr.register_dapp(100);
    mgr.add_subscription(100, 200);
    mgr.add_subscription(100, 300);
    mgr.add_subscription(100, 400);
    
    auto subs = mgr.get_dapp_subscriptions(100);
    ASSERT_EQ(subs.size(), 3u);
}

TEST(SubscriptionManager_unregister_clears_subscriptions) {
    SubscriptionManager mgr;
    mgr.register_dapp(100);
    mgr.add_subscription(100, 200);
    mgr.add_subscription(100, 300);
    
    mgr.unregister_dapp(100);
    ASSERT_EQ(mgr.subscription_count(), 0u);
    
    // Re-register and verify subscriptions are gone
    mgr.register_dapp(100);
    ASSERT_TRUE(mgr.get_dapp_subscriptions(100).empty());
}

TEST(SubscriptionManager_multiple_dapps) {
    SubscriptionManager mgr;
    
    for (uint32_t i = 0; i < 10; ++i) {
        mgr.register_dapp(i);
        mgr.add_subscription(i, 100);
        mgr.add_subscription(i, 200);
    }
    
    ASSERT_EQ(mgr.dapp_count(), 10u);
    ASSERT_EQ(mgr.subscription_count(), 20u);
    ASSERT_EQ(mgr.get_subscribed_dapps(100).size(), 10u);
}

TEST(SubscriptionManager_has_subscribers) {
    SubscriptionManager mgr;
    
    ASSERT_FALSE(mgr.has_subscribers(100));
    
    mgr.register_dapp(1);
    ASSERT_FALSE(mgr.has_subscribers(100));
    
    mgr.add_subscription(1, 100);
    ASSERT_TRUE(mgr.has_subscribers(100));
}

TEST(SubscriptionManager_sm_lifecycle_callback) {
    SubscriptionManager mgr;
    
    std::atomic<int> start_count{0};
    std::atomic<int> stop_count{0};
    uint32_t last_started_sm = 0;
    uint32_t last_stopped_sm = 0;
    
    mgr.set_sm_lifecycle_callback(
        [&](uint32_t sm_id, bool should_start) {
            if (should_start) {
                ++start_count;
                last_started_sm = sm_id;
            } else {
                ++stop_count;
                last_stopped_sm = sm_id;
            }
        }
    );
    
    mgr.register_dapp(1);
    
    // First subscription should trigger start
    mgr.add_subscription(1, 100);
    ASSERT_EQ(start_count.load(), 1);
    ASSERT_EQ(last_started_sm, 100u);
    
    // Second subscription to same SM should not trigger start
    mgr.register_dapp(2);
    mgr.add_subscription(2, 100);
    ASSERT_EQ(start_count.load(), 1);
    
    // Remove one subscription - SM still has subscribers
    mgr.remove_subscription(1, 100);
    ASSERT_EQ(stop_count.load(), 0);
    
    // Remove last subscription - should trigger stop
    mgr.remove_subscription(2, 100);
    ASSERT_EQ(stop_count.load(), 1);
    ASSERT_EQ(last_stopped_sm, 100u);
}

TEST(SubscriptionManager_thread_safety) {
    SubscriptionManager mgr;
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};
    
    // Pre-register some dApps
    for (uint32_t i = 0; i < 10; ++i) {
        mgr.register_dapp(i);
    }
    
    // Concurrent operations
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < 100; ++i) {
                uint32_t dapp_id = static_cast<uint32_t>(t);
                uint32_t sm_id = static_cast<uint32_t>(i % 5);
                
                auto add_result = mgr.add_subscription(dapp_id, sm_id);
                if (add_result == ErrorCode::SUCCESS || add_result == ErrorCode::ALREADY_EXISTS) {
                    ++success_count;
                } else {
                    ++error_count;
                }
                
                mgr.get_subscribed_dapps(sm_id);
                mgr.get_dapp_subscriptions(dapp_id);
                
                mgr.remove_subscription(dapp_id, sm_id);
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // No crashes or deadlocks means success
    ASSERT_GT(success_count.load(), 0);
}

TEST(SubscriptionManager_clear) {
    SubscriptionManager mgr;
    mgr.register_dapp(1);
    mgr.register_dapp(2);
    mgr.add_subscription(1, 100);
    mgr.add_subscription(2, 200);
    
    mgr.clear();
    
    ASSERT_EQ(mgr.dapp_count(), 0u);
    ASSERT_EQ(mgr.subscription_count(), 0u);
}

int main() {
    return RUN_ALL_TESTS();
}
