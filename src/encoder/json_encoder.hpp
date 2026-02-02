/**
 * @file json_encoder.hpp
 * @brief JSON Encoder for E3AP PDUs
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LIBE3_JSON_ENCODER_HPP
#define LIBE3_JSON_ENCODER_HPP

#include "libe3/e3_encoder.hpp"

namespace libe3 {

/**
 * @brief JSON encoder for E3AP PDUs
 *
 * Provides JSON encoding/decoding for development and debugging.
 * Ported from the original C implementation's JSON handling code.
 */
class JsonE3Encoder : public E3Encoder {
public:
    JsonE3Encoder() = default;
    ~JsonE3Encoder() override = default;

    [[nodiscard]] EncodeResult<EncodedMessage> encode(const Pdu& pdu) override;
    [[nodiscard]] EncodeResult<Pdu> decode(const EncodedMessage& encoded) override;
    [[nodiscard]] EncodeResult<Pdu> decode(const uint8_t* data, size_t size) override;
    [[nodiscard]] EncodingFormat format() const noexcept override { return EncodingFormat::JSON; }
};

} // namespace libe3

#endif // LIBE3_JSON_ENCODER_HPP
