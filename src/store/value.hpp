#pragma once

#include <string>
#include <unordered_map>
#include <deque>
#include <unordered_set>
#include <variant>

namespace minicache::store {

using HashMap = std::unordered_map<std::string, std::string>;
using List = std::deque<std::string>;
using Set = std::unordered_set<std::string>;

using Value = std::variant<
    std::string,
    HashMap,
    List,
    Set
>;

} // namespace minicache::store
