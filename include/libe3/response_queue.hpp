/**
 * @file response_queue.hpp
 * @brief Thread-safe queue for E3AP response messages
 *
 * Provides a thread-safe, blocking queue for queuing outbound E3AP PDUs.
 * Ported from the original C implementation's e3_response_queue.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_RESPONSE_QUEUE_HPP
#define LIBE3_RESPONSE_QUEUE_HPP

#include "types.hpp"
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <chrono>

namespace libe3 {

/**
 * @brief Thread-safe queue for E3AP PDUs
 *
 * This class provides a bounded, thread-safe queue for communication
 * between threads in the E3 agent. It supports blocking pop with
 * optional timeout.
 */
class ResponseQueue {
public:
    /**
     * @brief Construct a new Response Queue
     * @param max_size Maximum queue capacity (default: 100)
     */
    explicit ResponseQueue(size_t max_size = 100);

    /**
     * @brief Destructor
     */
    ~ResponseQueue();

    // Non-copyable, non-movable
    ResponseQueue(const ResponseQueue&) = delete;
    ResponseQueue& operator=(const ResponseQueue&) = delete;
    ResponseQueue(ResponseQueue&&) = delete;
    ResponseQueue& operator=(ResponseQueue&&) = delete;

    /**
     * @brief Push a PDU to the queue
     *
     * @param pdu PDU to push
     * @return ErrorCode::SUCCESS on success
     * @return ErrorCode::BUFFER_TOO_SMALL if queue is full
     */
    [[nodiscard]] ErrorCode push(Pdu pdu);

    /**
     * @brief Push a PDU to the queue (move semantics)
     */
    [[nodiscard]] ErrorCode push(Pdu&& pdu);

    /**
     * @brief Pop a PDU from the queue (blocking)
     *
     * Blocks until a PDU is available.
     *
     * @return The popped PDU
     */
    [[nodiscard]] Pdu pop();

    /**
     * @brief Pop a PDU from the queue with timeout
     *
     * @param timeout Maximum time to wait
     * @return The PDU if available, std::nullopt on timeout
     */
    [[nodiscard]] std::optional<Pdu> pop(std::chrono::milliseconds timeout);

    /**
     * @brief Try to pop without blocking
     *
     * @return The PDU if available, std::nullopt if queue is empty
     */
    [[nodiscard]] std::optional<Pdu> try_pop();

    /**
     * @brief Check if queue is empty
     */
    [[nodiscard]] bool empty() const;

    /**
     * @brief Get current queue size
     */
    [[nodiscard]] size_t size() const;

    /**
     * @brief Get maximum queue capacity
     */
    [[nodiscard]] size_t capacity() const noexcept { return max_size_; }

    /**
     * @brief Clear all items from the queue
     */
    void clear();

    /**
     * @brief Wake up all waiting threads (for shutdown)
     */
    void shutdown();

    /**
     * @brief Check if queue is shut down
     */
    [[nodiscard]] bool is_shutdown() const;

private:
    std::queue<Pdu> queue_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    size_t max_size_;
    bool shutdown_{false};
};

} // namespace libe3

#endif // LIBE3_RESPONSE_QUEUE_HPP
