#pragma once

#include "store/entry.hpp"
#include "eviction/eviction_policy.hpp"
#include <unordered_map>
#include <string>
#include <mutex>
#include <optional>
#include <memory>
#include <vector>
#include <cstdint>

namespace minicache::store {

class Shard {
public:
    Shard();
    ~Shard() = default;

    Shard(const Shard&) = delete;
    Shard& operator=(const Shard&) = delete;

    void setEvictionPolicy(std::unique_ptr<eviction::EvictionPolicy> policy);
    void setMaxMemory(size_t maxMemoryBytes);

    // Core Key-Value
    bool set(const std::string& key, Value value, std::optional<uint64_t> ttlMs = std::nullopt);
    std::optional<Value> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    bool expire(const std::string& key, uint64_t ttlMs);
    int64_t ttl(const std::string& key);
    bool persist(const std::string& key);
    std::optional<int64_t> incrBy(const std::string& key, int64_t delta);

    // Hash Operations
    std::optional<int64_t> hset(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fieldValues);
    std::optional<std::string> hget(const std::string& key, const std::string& field);
    std::optional<int64_t> hdel(const std::string& key, const std::vector<std::string>& fields);
    std::optional<std::vector<std::pair<std::string, std::string>>> hgetall(const std::string& key);
    std::optional<bool> hexists(const std::string& key, const std::string& field);

    // List Operations
    std::optional<int64_t> lpush(const std::string& key, const std::vector<std::string>& values);
    std::optional<int64_t> rpush(const std::string& key, const std::vector<std::string>& values);
    std::optional<std::string> lpop(const std::string& key);
    std::optional<std::string> rpop(const std::string& key);
    std::optional<std::vector<std::string>> lrange(const std::string& key, int64_t start, int64_t stop);
    std::optional<int64_t> llen(const std::string& key);

    // Set Operations
    std::optional<int64_t> sadd(const std::string& key, const std::vector<std::string>& members);
    std::optional<int64_t> srem(const std::string& key, const std::vector<std::string>& members);
    std::optional<bool> sismember(const std::string& key, const std::string& member);
    std::optional<std::vector<std::string>> smembers(const std::string& key);

    // Memory & Maintenance
    size_t size();
    size_t getMemoryUsage();
    void clear();

    // Active Expiry Cycle
    size_t activeExpireCycle(size_t sampleCount = 20);

private:
    bool isExpiredUnlocked(const std::string& key, uint64_t nowMs);
    void purgeExpiredUnlocked(const std::string& key);
    size_t estimateEntryMemory(const std::string& key, const Entry& entry);
    bool ensureMemoryUnlocked(size_t neededBytes);

    std::unordered_map<std::string, Entry> data_;
    std::mutex mutex_;
    std::unique_ptr<eviction::EvictionPolicy> evictionPolicy_;
    size_t maxMemory_{0}; // 0 = unlimited
    size_t currentMemory_{0};
};

} // namespace minicache::store
