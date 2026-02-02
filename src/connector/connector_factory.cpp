/**
 * @file connector_factory.cpp
 * @brief Factory for creating connector instances
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "libe3/e3_connector.hpp"
#include "zmq_connector.hpp"
#include "posix_connector.hpp"
#include "libe3/logger.hpp"

namespace libe3 {

std::unique_ptr<E3Connector> create_connector(
    TransportType transport,
    const std::string& setup_endpoint,
    const std::string& inbound_endpoint,
    const std::string& outbound_endpoint,
    size_t io_threads
) {
    switch (transport) {
        case TransportType::ZMQ_TCP:
        case TransportType::ZMQ_IPC:
            return std::make_unique<ZmqE3Connector>(
                transport, setup_endpoint, inbound_endpoint, outbound_endpoint, io_threads
            );
        
        case TransportType::POSIX_TCP:
        case TransportType::POSIX_SCTP:
        case TransportType::POSIX_IPC:
            return std::make_unique<PosixE3Connector>(
                transport, setup_endpoint, inbound_endpoint, outbound_endpoint
            );
        
        default:
            E3_LOG_ERROR("ConnFactory") << "Unsupported transport type: " 
                                        << static_cast<int>(transport);
            return nullptr;
    }
}

} // namespace libe3
