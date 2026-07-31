#include "protocol/resp_parser.hpp"
#include "protocol/resp_encoder.hpp"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

using namespace minicache::protocol;

void testSimpleString() {
    std::string input = "+OK\r\n";
    RespValue val;
    size_t consumed = 0;
    ParseResult res = RespParser::parse(input, val, consumed);

    assert(res == ParseResult::Success);
    assert(consumed == input.size());
    assert(val.type == RespType::SimpleString);
    assert(std::get<std::string>(val.data) == "OK");

    std::string encoded = RespEncoder::encode(val);
    assert(encoded == input);
}

void testError() {
    std::string input = "-ERR unknown command\r\n";
    RespValue val;
    size_t consumed = 0;
    ParseResult res = RespParser::parse(input, val, consumed);

    assert(res == ParseResult::Success);
    assert(consumed == input.size());
    assert(val.type == RespType::Error);
    assert(std::get<std::string>(val.data) == "ERR unknown command");

    std::string encoded = RespEncoder::encode(val);
    assert(encoded == input);
}

void testInteger() {
    std::string input = ":1000\r\n";
    RespValue val;
    size_t consumed = 0;
    ParseResult res = RespParser::parse(input, val, consumed);

    assert(res == ParseResult::Success);
    assert(consumed == input.size());
    assert(val.type == RespType::Integer);
    assert(std::get<int64_t>(val.data) == 1000);

    std::string encoded = RespEncoder::encode(val);
    assert(encoded == input);
}

void testBulkString() {
    std::string input = "$5\r\nhello\r\n";
    RespValue val;
    size_t consumed = 0;
    ParseResult res = RespParser::parse(input, val, consumed);

    assert(res == ParseResult::Success);
    assert(consumed == input.size());
    assert(val.type == RespType::BulkString);
    assert(std::get<std::string>(val.data) == "hello");

    std::string encoded = RespEncoder::encode(val);
    assert(encoded == input);
}

void testNullBulkString() {
    std::string input = "$-1\r\n";
    RespValue val;
    size_t consumed = 0;
    ParseResult res = RespParser::parse(input, val, consumed);

    assert(res == ParseResult::Success);
    assert(consumed == input.size());
    assert(val.type == RespType::NullBulkString);

    std::string encoded = RespEncoder::encode(val);
    assert(encoded == input);
}

void testArray() {
    std::string input = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    RespValue val;
    size_t consumed = 0;
    ParseResult res = RespParser::parse(input, val, consumed);

    assert(res == ParseResult::Success);
    assert(consumed == input.size());
    assert(val.type == RespType::Array);

    const auto& elems = std::get<std::vector<RespValue>>(val.data);
    assert(elems.size() == 2);
    assert(elems[0].type == RespType::BulkString);
    assert(std::get<std::string>(elems[0].data) == "GET");
    assert(elems[1].type == RespType::BulkString);
    assert(std::get<std::string>(elems[1].data) == "foo");

    std::string encoded = RespEncoder::encode(val);
    assert(encoded == input);
}

void testIncompleteInput() {
    std::string partialInput = "*2\r\n$3\r\nGET\r\n$3\r\nfo"; // missing end of second bulk string
    RespValue val;
    size_t consumed = 0;
    ParseResult res = RespParser::parse(partialInput, val, consumed);

    assert(res == ParseResult::Incomplete);
}

int main() {
    std::cout << "Running test_resp_protocol..." << std::endl;
    testSimpleString();
    testError();
    testInteger();
    testBulkString();
    testNullBulkString();
    testArray();
    testIncompleteInput();
    std::cout << "ALL RESP PROTOCOL TESTS PASSED!" << std::endl;
    return 0;
}
