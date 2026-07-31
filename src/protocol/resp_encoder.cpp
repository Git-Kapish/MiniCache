#include "protocol/resp_encoder.hpp"

namespace minicache::protocol {

std::string RespEncoder::encode(const RespValue& value) {
    std::string result;
    switch (value.type) {
        case RespType::SimpleString:
            result = "+" + std::get<std::string>(value.data) + "\r\n";
            break;
        case RespType::Error:
            result = "-" + std::get<std::string>(value.data) + "\r\n";
            break;
        case RespType::Integer:
            result = ":" + std::to_string(std::get<int64_t>(value.data)) + "\r\n";
            break;
        case RespType::BulkString: {
            const std::string& str = std::get<std::string>(value.data);
            result = "$" + std::to_string(str.size()) + "\r\n" + str + "\r\n";
            break;
        }
        case RespType::NullBulkString:
            result = "$-1\r\n";
            break;
        case RespType::Array: {
            const auto& vec = std::get<std::vector<RespValue>>(value.data);
            result = "*" + std::to_string(vec.size()) + "\r\n";
            for (const auto& elem : vec) {
                result += encode(elem);
            }
            break;
        }
        case RespType::NullArray:
            result = "*-1\r\n";
            break;
    }
    return result;
}

} // namespace minicache::protocol
