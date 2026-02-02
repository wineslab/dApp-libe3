/**
 * @file json_encoder.cpp
 * @brief JSON Encoder implementation
 *
 * Ported from the original C implementation's e3ap_handler.c JSON handling.
 * Uses a simple, header-only JSON approach to avoid external dependencies.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "json_encoder.hpp"
#include "libe3/logger.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cctype>

namespace libe3 {

namespace {

constexpr const char* LOG_TAG = "JsonEnc";

// Simple JSON builder helper
class JsonBuilder {
public:
    void start_object() { ss_ << '{'; first_ = true; }
    void end_object() { ss_ << '}'; }
    void start_array() { ss_ << '['; first_ = true; }
    void end_array() { ss_ << ']'; }
    
    void key(const char* k) {
        if (!first_) ss_ << ',';
        first_ = false;
        ss_ << '"' << k << "\":";
    }
    
    void value_string(const std::string& v) {
        ss_ << '"';
        for (char c : v) {
            switch (c) {
                case '"': ss_ << "\\\""; break;
                case '\\': ss_ << "\\\\"; break;
                case '\n': ss_ << "\\n"; break;
                case '\r': ss_ << "\\r"; break;
                case '\t': ss_ << "\\t"; break;
                default: ss_ << c;
            }
        }
        ss_ << '"';
    }
    
    void value_int(int64_t v) { ss_ << v; }
    void value_uint(uint64_t v) { ss_ << v; }
    
    void value_binary(const std::vector<uint8_t>& data) {
        // Encode as base64-like hex string for readability
        ss_ << '"';
        for (uint8_t b : data) {
            ss_ << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(b);
        }
        ss_ << '"';
        ss_ << std::dec;
    }
    
    void array_value_int(int64_t v) {
        if (!first_) ss_ << ',';
        first_ = false;
        ss_ << v;
    }
    
    std::string str() const { return ss_.str(); }
    
private:
    std::ostringstream ss_;
    bool first_{true};
};

// Simple JSON parser helper
class JsonParser {
public:
    explicit JsonParser(const std::string& json) : json_(json), pos_(0) {}
    
    bool parse_object(std::function<void(const std::string& key)> handler) {
        skip_whitespace();
        if (!expect('{')) return false;
        
        skip_whitespace();
        if (peek() == '}') {
            pos_++;
            return true;
        }
        
        while (true) {
            skip_whitespace();
            std::string key;
            if (!parse_string(key)) return false;
            
            skip_whitespace();
            if (!expect(':')) return false;
            
            handler(key);
            
            skip_whitespace();
            if (peek() == '}') {
                pos_++;
                return true;
            }
            if (!expect(',')) return false;
        }
    }
    
    bool parse_string(std::string& out) {
        skip_whitespace();
        if (!expect('"')) return false;
        
        out.clear();
        while (pos_ < json_.size() && json_[pos_] != '"') {
            if (json_[pos_] == '\\' && pos_ + 1 < json_.size()) {
                pos_++;
                switch (json_[pos_]) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    default: out += json_[pos_];
                }
            } else {
                out += json_[pos_];
            }
            pos_++;
        }
        return expect('"');
    }
    
    bool parse_int(int64_t& out) {
        skip_whitespace();
        size_t start = pos_;
        if (pos_ < json_.size() && json_[pos_] == '-') pos_++;
        while (pos_ < json_.size() && std::isdigit(json_[pos_])) pos_++;
        if (pos_ == start) return false;
        out = std::stoll(json_.substr(start, pos_ - start));
        return true;
    }
    
    bool parse_uint(uint64_t& out) {
        skip_whitespace();
        size_t start = pos_;
        while (pos_ < json_.size() && std::isdigit(json_[pos_])) pos_++;
        if (pos_ == start) return false;
        out = std::stoull(json_.substr(start, pos_ - start));
        return true;
    }
    
    bool parse_array_of_ints(std::vector<uint32_t>& out) {
        skip_whitespace();
        if (!expect('[')) return false;
        
        out.clear();
        skip_whitespace();
        if (peek() == ']') {
            pos_++;
            return true;
        }
        
        while (true) {
            uint64_t val;
            if (!parse_uint(val)) return false;
            out.push_back(static_cast<uint32_t>(val));
            
            skip_whitespace();
            if (peek() == ']') {
                pos_++;
                return true;
            }
            if (!expect(',')) return false;
        }
    }
    
    bool parse_binary(std::vector<uint8_t>& out) {
        std::string hex;
        if (!parse_string(hex)) return false;
        
        out.clear();
        for (size_t i = 0; i + 1 < hex.size(); i += 2) {
            out.push_back(static_cast<uint8_t>(
                std::stoi(hex.substr(i, 2), nullptr, 16)));
        }
        return true;
    }
    
    void skip_value() {
        skip_whitespace();
        char c = peek();
        if (c == '"') {
            std::string dummy;
            parse_string(dummy);
        } else if (c == '[') {
            pos_++;
            int depth = 1;
            while (pos_ < json_.size() && depth > 0) {
                if (json_[pos_] == '[') depth++;
                else if (json_[pos_] == ']') depth--;
                pos_++;
            }
        } else if (c == '{') {
            pos_++;
            int depth = 1;
            while (pos_ < json_.size() && depth > 0) {
                if (json_[pos_] == '{') depth++;
                else if (json_[pos_] == '}') depth--;
                pos_++;
            }
        } else {
            // Number or literal
            while (pos_ < json_.size() && 
                   json_[pos_] != ',' && json_[pos_] != '}' && json_[pos_] != ']') {
                pos_++;
            }
        }
    }
    
private:
    void skip_whitespace() {
        while (pos_ < json_.size() && std::isspace(json_[pos_])) pos_++;
    }
    
    char peek() const {
        return pos_ < json_.size() ? json_[pos_] : '\0';
    }
    
    bool expect(char c) {
        skip_whitespace();
        if (pos_ < json_.size() && json_[pos_] == c) {
            pos_++;
            return true;
        }
        return false;
    }
    
    const std::string& json_;
    size_t pos_;
};

PduType string_to_pdu_type(const std::string& s) {
    if (s == "SetupRequest") return PduType::SETUP_REQUEST;
    if (s == "SetupResponse") return PduType::SETUP_RESPONSE;
    if (s == "SubscriptionRequest") return PduType::SUBSCRIPTION_REQUEST;
    if (s == "SubscriptionResponse") return PduType::SUBSCRIPTION_RESPONSE;
    if (s == "IndicationMessage") return PduType::INDICATION_MESSAGE;
    if (s == "ControlAction") return PduType::CONTROL_ACTION;
    if (s == "DAppReport") return PduType::DAPP_REPORT;
    if (s == "XAppControlAction") return PduType::XAPP_CONTROL_ACTION;
    if (s == "MessageAck") return PduType::MESSAGE_ACK;
    return PduType::SETUP_REQUEST; // Default
}

ActionType string_to_action_type(const std::string& s) {
    if (s == "insert") return ActionType::INSERT;
    if (s == "update") return ActionType::UPDATE;
    if (s == "delete") return ActionType::DELETE;
    return ActionType::INSERT;
}

ResponseCode string_to_response_code(const std::string& s) {
    if (s == "positive") return ResponseCode::POSITIVE;
    if (s == "negative") return ResponseCode::NEGATIVE;
    return ResponseCode::NEGATIVE;
}

} // anonymous namespace

EncodeResult<EncodedMessage> JsonE3Encoder::encode(const Pdu& pdu) {
    JsonBuilder json;
    json.start_object();
    
    json.key("pdu_type");
    json.value_string(pdu_type_to_string(pdu.type));
    
    json.key("data");
    json.start_object();
    
    std::visit([&json](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        
        if constexpr (std::is_same_v<T, SetupRequest>) {
            json.key("id"); json.value_uint(arg.id);
            json.key("dapp_identifier"); json.value_uint(arg.dapp_identifier);
            json.key("action_type"); json.value_string(action_type_to_string(arg.type));
            json.key("ran_function_list");
            json.start_array();
            for (uint32_t rf : arg.ran_function_list) {
                json.array_value_int(rf);
            }
            json.end_array();
        }
        else if constexpr (std::is_same_v<T, SetupResponse>) {
            json.key("id"); json.value_uint(arg.id);
            json.key("request_id"); json.value_uint(arg.request_id);
            json.key("response_code"); json.value_string(response_code_to_string(arg.response_code));
            json.key("ran_function_list");
            json.start_array();
            for (uint32_t rf : arg.ran_function_list) {
                json.array_value_int(rf);
            }
            json.end_array();
        }
        else if constexpr (std::is_same_v<T, SubscriptionRequest>) {
            json.key("id"); json.value_uint(arg.id);
            json.key("dapp_identifier"); json.value_uint(arg.dapp_identifier);
            json.key("action_type"); json.value_string(action_type_to_string(arg.type));
            json.key("ran_function_identifier"); json.value_uint(arg.ran_function_identifier);
        }
        else if constexpr (std::is_same_v<T, SubscriptionResponse>) {
            json.key("id"); json.value_uint(arg.id);
            json.key("request_id"); json.value_uint(arg.request_id);
            json.key("response_code"); json.value_string(response_code_to_string(arg.response_code));
        }
        else if constexpr (std::is_same_v<T, IndicationMessage>) {
            json.key("id"); json.value_uint(arg.id);
            json.key("dapp_identifier"); json.value_uint(arg.dapp_identifier);
            json.key("protocol_data"); json.value_binary(arg.protocol_data);
        }
        else if constexpr (std::is_same_v<T, ControlAction>) {
            json.key("id"); json.value_uint(arg.id);
            json.key("dapp_identifier"); json.value_uint(arg.dapp_identifier);
            json.key("ran_function_identifier"); json.value_uint(arg.ran_function_identifier);
            json.key("action_data"); json.value_binary(arg.action_data);
        }
        else if constexpr (std::is_same_v<T, DAppReport>) {
            json.key("id"); json.value_uint(arg.id);
            json.key("dapp_identifier"); json.value_uint(arg.dapp_identifier);
            json.key("ran_function_identifier"); json.value_uint(arg.ran_function_identifier);
            json.key("report_data"); json.value_binary(arg.report_data);
        }
        else if constexpr (std::is_same_v<T, XAppControlAction>) {
            json.key("id"); json.value_uint(arg.id);
            json.key("dapp_identifier"); json.value_uint(arg.dapp_identifier);
            json.key("ran_function_identifier"); json.value_uint(arg.ran_function_identifier);
            json.key("xapp_control_data"); json.value_binary(arg.xapp_control_data);
        }
        else if constexpr (std::is_same_v<T, MessageAck>) {
            json.key("id"); json.value_uint(arg.id);
            json.key("request_id"); json.value_uint(arg.request_id);
            json.key("response_code"); json.value_string(response_code_to_string(arg.response_code));
        }
    }, pdu.choice);
    
    json.end_object(); // data
    json.end_object(); // root
    
    std::string json_str = json.str();
    EncodedMessage msg;
    msg.buffer.assign(json_str.begin(), json_str.end());
    msg.format = EncodingFormat::JSON;
    
    E3_LOG_TRACE(LOG_TAG) << "Encoded " << pdu_type_to_string(pdu.type) 
                          << " (" << msg.size() << " bytes)";
    
    return msg;
}

EncodeResult<Pdu> JsonE3Encoder::decode(const EncodedMessage& encoded) {
    return decode(encoded.data(), encoded.size());
}

EncodeResult<Pdu> JsonE3Encoder::decode(const uint8_t* data, size_t size) {
    std::string json_str(reinterpret_cast<const char*>(data), size);
    
    E3_LOG_TRACE(LOG_TAG) << "Decoding JSON: " << json_str.substr(0, 100) << "...";
    
    JsonParser parser(json_str);
    
    Pdu pdu;
    std::string pdu_type_str;
    bool has_pdu_type = false;
    bool has_data = false;
    
    bool success = parser.parse_object([&](const std::string& key) {
        if (key == "pdu_type") {
            parser.parse_string(pdu_type_str);
            pdu.type = string_to_pdu_type(pdu_type_str);
            has_pdu_type = true;
        }
        else if (key == "data") {
            has_data = true;
            
            // Parse data object based on PDU type
            parser.parse_object([&](const std::string& data_key) {
                switch (pdu.type) {
                    case PduType::SETUP_REQUEST: {
                        auto& req = pdu.choice.emplace<SetupRequest>();
                        if (data_key == "id") {
                            uint64_t val; parser.parse_uint(val); req.id = static_cast<uint32_t>(val);
                        } else if (data_key == "dapp_identifier") {
                            uint64_t val; parser.parse_uint(val); req.dapp_identifier = static_cast<uint32_t>(val);
                        } else if (data_key == "action_type") {
                            std::string s; parser.parse_string(s); req.type = string_to_action_type(s);
                        } else if (data_key == "ran_function_list") {
                            parser.parse_array_of_ints(req.ran_function_list);
                        } else {
                            parser.skip_value();
                        }
                        break;
                    }
                    case PduType::SETUP_RESPONSE: {
                        auto& resp = pdu.choice.emplace<SetupResponse>();
                        if (data_key == "id") {
                            uint64_t val; parser.parse_uint(val); resp.id = static_cast<uint32_t>(val);
                        } else if (data_key == "request_id") {
                            uint64_t val; parser.parse_uint(val); resp.request_id = static_cast<uint32_t>(val);
                        } else if (data_key == "response_code") {
                            std::string s; parser.parse_string(s); resp.response_code = string_to_response_code(s);
                        } else if (data_key == "ran_function_list") {
                            parser.parse_array_of_ints(resp.ran_function_list);
                        } else {
                            parser.skip_value();
                        }
                        break;
                    }
                    case PduType::SUBSCRIPTION_REQUEST: {
                        auto& req = pdu.choice.emplace<SubscriptionRequest>();
                        if (data_key == "id") {
                            uint64_t val; parser.parse_uint(val); req.id = static_cast<uint32_t>(val);
                        } else if (data_key == "dapp_identifier") {
                            uint64_t val; parser.parse_uint(val); req.dapp_identifier = static_cast<uint32_t>(val);
                        } else if (data_key == "action_type") {
                            std::string s; parser.parse_string(s); req.type = string_to_action_type(s);
                        } else if (data_key == "ran_function_identifier") {
                            uint64_t val; parser.parse_uint(val); req.ran_function_identifier = static_cast<uint32_t>(val);
                        } else {
                            parser.skip_value();
                        }
                        break;
                    }
                    case PduType::SUBSCRIPTION_RESPONSE: {
                        auto& resp = pdu.choice.emplace<SubscriptionResponse>();
                        if (data_key == "id") {
                            uint64_t val; parser.parse_uint(val); resp.id = static_cast<uint32_t>(val);
                        } else if (data_key == "request_id") {
                            uint64_t val; parser.parse_uint(val); resp.request_id = static_cast<uint32_t>(val);
                        } else if (data_key == "response_code") {
                            std::string s; parser.parse_string(s); resp.response_code = string_to_response_code(s);
                        } else {
                            parser.skip_value();
                        }
                        break;
                    }
                    case PduType::INDICATION_MESSAGE: {
                        auto& msg = pdu.choice.emplace<IndicationMessage>();
                        if (data_key == "id") {
                            uint64_t val; parser.parse_uint(val); msg.id = static_cast<uint32_t>(val);
                        } else if (data_key == "dapp_identifier") {
                            uint64_t val; parser.parse_uint(val); msg.dapp_identifier = static_cast<uint32_t>(val);
                        } else if (data_key == "protocol_data") {
                            parser.parse_binary(msg.protocol_data);
                        } else {
                            parser.skip_value();
                        }
                        break;
                    }
                    case PduType::CONTROL_ACTION: {
                        auto& action = pdu.choice.emplace<ControlAction>();
                        if (data_key == "id") {
                            uint64_t val; parser.parse_uint(val); action.id = static_cast<uint32_t>(val);
                        } else if (data_key == "dapp_identifier") {
                            uint64_t val; parser.parse_uint(val); action.dapp_identifier = static_cast<uint32_t>(val);
                        } else if (data_key == "ran_function_identifier") {
                            uint64_t val; parser.parse_uint(val); action.ran_function_identifier = static_cast<uint32_t>(val);
                        } else if (data_key == "action_data") {
                            parser.parse_binary(action.action_data);
                        } else {
                            parser.skip_value();
                        }
                        break;
                    }
                    case PduType::DAPP_REPORT: {
                        auto& report = pdu.choice.emplace<DAppReport>();
                        if (data_key == "id") {
                            uint64_t val; parser.parse_uint(val); report.id = static_cast<uint32_t>(val);
                        } else if (data_key == "dapp_identifier") {
                            uint64_t val; parser.parse_uint(val); report.dapp_identifier = static_cast<uint32_t>(val);
                        } else if (data_key == "ran_function_identifier") {
                            uint64_t val; parser.parse_uint(val); report.ran_function_identifier = static_cast<uint32_t>(val);
                        } else if (data_key == "report_data") {
                            parser.parse_binary(report.report_data);
                        } else {
                            parser.skip_value();
                        }
                        break;
                    }
                    case PduType::XAPP_CONTROL_ACTION: {
                        auto& action = pdu.choice.emplace<XAppControlAction>();
                        if (data_key == "id") {
                            uint64_t val; parser.parse_uint(val); action.id = static_cast<uint32_t>(val);
                        } else if (data_key == "dapp_identifier") {
                            uint64_t val; parser.parse_uint(val); action.dapp_identifier = static_cast<uint32_t>(val);
                        } else if (data_key == "ran_function_identifier") {
                            uint64_t val; parser.parse_uint(val); action.ran_function_identifier = static_cast<uint32_t>(val);
                        } else if (data_key == "xapp_control_data") {
                            parser.parse_binary(action.xapp_control_data);
                        } else {
                            parser.skip_value();
                        }
                        break;
                    }
                    case PduType::MESSAGE_ACK: {
                        auto& ack = pdu.choice.emplace<MessageAck>();
                        if (data_key == "id") {
                            uint64_t val; parser.parse_uint(val); ack.id = static_cast<uint32_t>(val);
                        } else if (data_key == "request_id") {
                            uint64_t val; parser.parse_uint(val); ack.request_id = static_cast<uint32_t>(val);
                        } else if (data_key == "response_code") {
                            std::string s; parser.parse_string(s); ack.response_code = string_to_response_code(s);
                        } else {
                            parser.skip_value();
                        }
                        break;
                    }
                }
            });
        }
        else {
            parser.skip_value();
        }
    });
    
    if (!success || !has_pdu_type || !has_data) {
        E3_LOG_ERROR(LOG_TAG) << "Failed to decode JSON PDU";
        return std::unexpected(ErrorCode::DECODE_FAILED);
    }
    
    E3_LOG_TRACE(LOG_TAG) << "Decoded " << pdu_type_to_string(pdu.type);
    return pdu;
}

} // namespace libe3
