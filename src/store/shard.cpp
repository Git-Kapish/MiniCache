#include "store/shard.hpp"
#include "eviction/no_eviction_policy.hpp"
#include <system_error>
#include <algorithm>
#include <random>

namespace minicache::store {

Shard::Shard() {
    evictionPolicy_ = std::make_unique<eviction::NoEvictionPolicy>();
}

void Shard::setEvictionPolicy(std::unique_ptr<eviction::EvictionPolicy> policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (policy) {
        evictionPolicy_ = std::move(policy);
        for (const auto& [k, v] : data_) {
            evictionPolicy_->onInsert(k);
        }
    }
}

void Shard::setMaxMemory(size_t maxMemoryBytes) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxMemory_ = maxMemoryBytes;
}

size_t Shard::estimateEntryMemory(const std::string& key, const Entry& entry) {
    size_t mem = key.size() + sizeof(Entry);
    if (std::holds_alternative<std::string>(entry.value)) {
        mem += std::get<std::string>(entry.value).capacity();
    } else if (std::holds_alternative<HashMap>(entry.value)) {
        const auto& map = std::get<HashMap>(entry.value);
        for (const auto& [fk, fv] : map) {
            mem += fk.capacity() + fv.capacity() + 32;
        }
    } else if (std::holds_alternative<List>(entry.value)) {
        const auto& list = std::get<List>(entry.value);
        for (const auto& item : list) {
            mem += item.capacity() + 16;
        }
    } else if (std::holds_alternative<Set>(entry.value)) {
        const auto& set = std::get<Set>(entry.value);
        for (const auto& item : set) {
            mem += item.capacity() + 32;
        }
    }
    return mem;
}

bool Shard::ensureMemoryUnlocked(size_t neededBytes) {
    if (maxMemory_ == 0) {
        return true;
    }

    while (currentMemory_ + neededBytes > maxMemory_) {
        std::string victim = evictionPolicy_->selectVictim();
        if (victim.empty()) {
            return false;
        }
        purgeExpiredUnlocked(victim);
    }
    return true;
}

bool Shard::isExpiredUnlocked(const std::string& key, uint64_t nowMs) {
    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }
    return it->second.isExpired(nowMs);
}

void Shard::purgeExpiredUnlocked(const std::string& key) {
    auto it = data_.find(key);
    if (it != data_.end()) {
        currentMemory_ -= estimateEntryMemory(key, it->second);
        evictionPolicy_->onDelete(key);
        data_.erase(it);
    }
}

bool Shard::set(const std::string& key, Value value, std::optional<uint64_t> ttlMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto existingIt = data_.find(key);
    if (existingIt != data_.end()) {
        purgeExpiredUnlocked(key);
    }

    Entry newEntry;
    newEntry.value = std::move(value);
    newEntry.lastAccessMs = now;
    newEntry.accessFreq = 1;
    if (ttlMs.has_value()) {
        newEntry.expiresAtMs = now + ttlMs.value();
    }

    size_t entryMem = estimateEntryMemory(key, newEntry);
    if (!ensureMemoryUnlocked(entryMem)) {
        return false;
    }

    currentMemory_ += entryMem;
    evictionPolicy_->onInsert(key);
    data_[key] = std::move(newEntry);
    return true;
}

std::optional<Value> Shard::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return std::nullopt;
    }

    it->second.lastAccessMs = now;
    it->second.accessFreq++;
    evictionPolicy_->onAccess(key);

    return it->second.value;
}

bool Shard::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return false;
    }

    purgeExpiredUnlocked(key);
    return true;
}

bool Shard::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return false;
    }

    evictionPolicy_->onAccess(key);
    return true;
}

bool Shard::expire(const std::string& key, uint64_t ttlMs) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return false;
    }

    it->second.expiresAtMs = now + ttlMs;
    return true;
}

int64_t Shard::ttl(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return -2;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return -2;
    }

    if (!it->second.expiresAtMs.has_value()) {
        return -1;
    }

    uint64_t expiresAt = it->second.expiresAtMs.value();
    if (expiresAt <= now) {
        purgeExpiredUnlocked(key);
        return -2;
    }

    return static_cast<int64_t>((expiresAt - now) / 1000);
}

bool Shard::persist(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return false;
    }

    if (!it->second.expiresAtMs.has_value()) {
        return false;
    }

    it->second.expiresAtMs = std::nullopt;
    return true;
}

std::optional<int64_t> Shard::incrBy(const std::string& key, int64_t delta) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    int64_t currentVal = 0;
    std::optional<uint64_t> existingTtl = std::nullopt;

    if (it != data_.end()) {
        if (it->second.isExpired(now)) {
            purgeExpiredUnlocked(key);
        } else {
            if (!std::holds_alternative<std::string>(it->second.value)) {
                return std::nullopt;
            }
            const std::string& s = std::get<std::string>(it->second.value);
            try {
                size_t pos = 0;
                currentVal = std::stoll(s, &pos);
                if (pos != s.size()) {
                    return std::nullopt;
                }
            } catch (...) {
                return std::nullopt;
            }
            existingTtl = it->second.expiresAtMs;
            purgeExpiredUnlocked(key);
        }
    }

    int64_t newVal = currentVal + delta;
    Entry entry;
    entry.value = std::to_string(newVal);
    entry.lastAccessMs = now;
    entry.accessFreq = 1;
    entry.expiresAtMs = existingTtl;

    size_t mem = estimateEntryMemory(key, entry);
    if (!ensureMemoryUnlocked(mem)) {
        return std::nullopt;
    }

    currentMemory_ += mem;
    evictionPolicy_->onInsert(key);
    data_[key] = std::move(entry);
    return newVal;
}

// Hash Operations
std::optional<int64_t> Shard::hset(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fieldValues) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it != data_.end()) {
        if (it->second.isExpired(now)) {
            purgeExpiredUnlocked(key);
            it = data_.end();
        } else if (!std::holds_alternative<HashMap>(it->second.value)) {
            return std::nullopt; // Wrong type
        }
    }

    HashMap map;
    if (it != data_.end()) {
        map = std::get<HashMap>(it->second.value);
    }

    int64_t newFieldsAdded = 0;
    for (const auto& [field, value] : fieldValues) {
        if (map.find(field) == map.end()) {
            newFieldsAdded++;
        }
        map[field] = value;
    }
    
    Entry entry;
    entry.value = std::move(map);
    entry.lastAccessMs = now;
    entry.accessFreq = (it != data_.end()) ? it->second.accessFreq + 1 : 1;
    entry.expiresAtMs = (it != data_.end()) ? it->second.expiresAtMs : std::nullopt;

    if (it != data_.end()) {
        purgeExpiredUnlocked(key);
    }

    size_t mem = estimateEntryMemory(key, entry);
    if (!ensureMemoryUnlocked(mem)) {
        return std::nullopt;
    }

    currentMemory_ += mem;
    evictionPolicy_->onInsert(key);
    data_[key] = std::move(entry);
    return newFieldsAdded;
}

std::optional<std::string> Shard::hget(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return std::nullopt;
    }

    if (!std::holds_alternative<HashMap>(it->second.value)) {
        return std::nullopt;
    }

    evictionPolicy_->onAccess(key);
    const auto& map = std::get<HashMap>(it->second.value);
    auto fit = map.find(field);
    if (fit == map.end()) {
        return std::nullopt;
    }
    return fit->second;
}

std::optional<int64_t> Shard::hdel(const std::string& key, const std::vector<std::string>& fields) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return 0;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return 0;
    }

    if (!std::holds_alternative<HashMap>(it->second.value)) {
        return std::nullopt;
    }

    auto map = std::get<HashMap>(it->second.value);
    int64_t deleted = 0;
    for (const auto& field : fields) {
        if (map.erase(field) > 0) {
            deleted++;
        }
    }

    if (map.empty()) {
        purgeExpiredUnlocked(key);
    } else {
        it->second.value = std::move(map);
    }

    return deleted;
}

std::optional<std::vector<std::pair<std::string, std::string>>> Shard::hgetall(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::vector<std::pair<std::string, std::string>>{};
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return std::vector<std::pair<std::string, std::string>>{};
    }

    if (!std::holds_alternative<HashMap>(it->second.value)) {
        return std::nullopt;
    }

    evictionPolicy_->onAccess(key);
    const auto& map = std::get<HashMap>(it->second.value);
    std::vector<std::pair<std::string, std::string>> result;
    result.reserve(map.size());
    for (const auto& [k, v] : map) {
        result.emplace_back(k, v);
    }
    return result;
}

std::optional<bool> Shard::hexists(const std::string& key, const std::string& field) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return false;
    }

    if (!std::holds_alternative<HashMap>(it->second.value)) {
        return std::nullopt;
    }

    evictionPolicy_->onAccess(key);
    const auto& map = std::get<HashMap>(it->second.value);
    return map.find(field) != map.end();
}

// List Operations
std::optional<int64_t> Shard::lpush(const std::string& key, const std::vector<std::string>& values) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    List list;
    if (it != data_.end()) {
        if (it->second.isExpired(now)) {
            purgeExpiredUnlocked(key);
            it = data_.end();
        } else if (!std::holds_alternative<List>(it->second.value)) {
            return std::nullopt;
        } else {
            list = std::get<List>(it->second.value);
        }
    }

    for (const auto& val : values) {
        list.push_front(val);
    }

    Entry entry;
    entry.value = std::move(list);
    entry.lastAccessMs = now;
    entry.accessFreq = (it != data_.end()) ? it->second.accessFreq + 1 : 1;

    if (it != data_.end()) {
        purgeExpiredUnlocked(key);
    }

    size_t mem = estimateEntryMemory(key, entry);
    if (!ensureMemoryUnlocked(mem)) {
        return std::nullopt;
    }

    currentMemory_ += mem;
    evictionPolicy_->onInsert(key);
    int64_t listSize = static_cast<int64_t>(std::get<List>(entry.value).size());
    data_[key] = std::move(entry);
    return listSize;
}

std::optional<int64_t> Shard::rpush(const std::string& key, const std::vector<std::string>& values) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    List list;
    if (it != data_.end()) {
        if (it->second.isExpired(now)) {
            purgeExpiredUnlocked(key);
            it = data_.end();
        } else if (!std::holds_alternative<List>(it->second.value)) {
            return std::nullopt;
        } else {
            list = std::get<List>(it->second.value);
        }
    }

    for (const auto& val : values) {
        list.push_back(val);
    }

    Entry entry;
    entry.value = std::move(list);
    entry.lastAccessMs = now;
    entry.accessFreq = (it != data_.end()) ? it->second.accessFreq + 1 : 1;

    if (it != data_.end()) {
        purgeExpiredUnlocked(key);
    }

    size_t mem = estimateEntryMemory(key, entry);
    if (!ensureMemoryUnlocked(mem)) {
        return std::nullopt;
    }

    currentMemory_ += mem;
    evictionPolicy_->onInsert(key);
    int64_t listSize = static_cast<int64_t>(std::get<List>(entry.value).size());
    data_[key] = std::move(entry);
    return listSize;
}

std::optional<std::string> Shard::lpop(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return std::nullopt;
    }

    if (!std::holds_alternative<List>(it->second.value)) {
        return std::nullopt;
    }

    auto list = std::get<List>(it->second.value);
    if (list.empty()) {
        return std::nullopt;
    }

    std::string val = std::move(list.front());
    list.pop_front();

    if (list.empty()) {
        purgeExpiredUnlocked(key);
    } else {
        it->second.value = std::move(list);
    }

    return val;
}

std::optional<std::string> Shard::rpop(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::nullopt;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return std::nullopt;
    }

    if (!std::holds_alternative<List>(it->second.value)) {
        return std::nullopt;
    }

    auto list = std::get<List>(it->second.value);
    if (list.empty()) {
        return std::nullopt;
    }

    std::string val = std::move(list.back());
    list.pop_back();

    if (list.empty()) {
        purgeExpiredUnlocked(key);
    } else {
        it->second.value = std::move(list);
    }

    return val;
}

std::optional<std::vector<std::string>> Shard::lrange(const std::string& key, int64_t start, int64_t stop) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::vector<std::string>{};
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return std::vector<std::string>{};
    }

    if (!std::holds_alternative<List>(it->second.value)) {
        return std::nullopt;
    }

    evictionPolicy_->onAccess(key);
    const auto& list = std::get<List>(it->second.value);
    int64_t len = static_cast<int64_t>(list.size());

    if (start < 0) start += len;
    if (stop < 0) stop += len;

    if (start < 0) start = 0;
    if (stop >= len) stop = len - 1;

    std::vector<std::string> result;
    if (start > stop || start >= len) {
        return result;
    }

    for (int64_t i = start; i <= stop; ++i) {
        result.push_back(list[static_cast<size_t>(i)]);
    }

    return result;
}

std::optional<int64_t> Shard::llen(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return 0;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return 0;
    }

    if (!std::holds_alternative<List>(it->second.value)) {
        return std::nullopt;
    }

    return static_cast<int64_t>(std::get<List>(it->second.value).size());
}

// Set Operations
std::optional<int64_t> Shard::sadd(const std::string& key, const std::vector<std::string>& members) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    Set set;
    if (it != data_.end()) {
        if (it->second.isExpired(now)) {
            purgeExpiredUnlocked(key);
            it = data_.end();
        } else if (!std::holds_alternative<Set>(it->second.value)) {
            return std::nullopt;
        } else {
            set = std::get<Set>(it->second.value);
        }
    }

    int64_t added = 0;
    for (const auto& member : members) {
        if (set.insert(member).second) {
            added++;
        }
    }

    Entry entry;
    entry.value = std::move(set);
    entry.lastAccessMs = now;
    entry.accessFreq = (it != data_.end()) ? it->second.accessFreq + 1 : 1;

    if (it != data_.end()) {
        purgeExpiredUnlocked(key);
    }

    size_t mem = estimateEntryMemory(key, entry);
    if (!ensureMemoryUnlocked(mem)) {
        return std::nullopt;
    }

    currentMemory_ += mem;
    evictionPolicy_->onInsert(key);
    data_[key] = std::move(entry);
    return added;
}

std::optional<int64_t> Shard::srem(const std::string& key, const std::vector<std::string>& members) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return 0;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return 0;
    }

    if (!std::holds_alternative<Set>(it->second.value)) {
        return std::nullopt;
    }

    auto set = std::get<Set>(it->second.value);
    int64_t removed = 0;
    for (const auto& member : members) {
        if (set.erase(member) > 0) {
            removed++;
        }
    }

    if (set.empty()) {
        purgeExpiredUnlocked(key);
    } else {
        it->second.value = std::move(set);
    }

    return removed;
}

std::optional<bool> Shard::sismember(const std::string& key, const std::string& member) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return false;
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return false;
    }

    if (!std::holds_alternative<Set>(it->second.value)) {
        return std::nullopt;
    }

    evictionPolicy_->onAccess(key);
    const auto& set = std::get<Set>(it->second.value);
    return set.find(member) != set.end();
}

std::optional<std::vector<std::string>> Shard::smembers(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint64_t now = getCurrentTimeMs();

    auto it = data_.find(key);
    if (it == data_.end()) {
        return std::vector<std::string>{};
    }

    if (it->second.isExpired(now)) {
        purgeExpiredUnlocked(key);
        return std::vector<std::string>{};
    }

    if (!std::holds_alternative<Set>(it->second.value)) {
        return std::nullopt;
    }

    evictionPolicy_->onAccess(key);
    const auto& set = std::get<Set>(it->second.value);
    return std::vector<std::string>(set.begin(), set.end());
}

size_t Shard::size() {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
}

size_t Shard::getMemoryUsage() {
    std::lock_guard<std::mutex> lock(mutex_);
    return currentMemory_;
}

void Shard::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    currentMemory_ = 0;
}

size_t Shard::activeExpireCycle(size_t sampleCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (data_.empty()) {
        return 0;
    }

    uint64_t now = getCurrentTimeMs();
    size_t expiredCount = 0;
    size_t checkedCount = 0;

    auto it = data_.begin();
    while (it != data_.end() && checkedCount < sampleCount) {
        checkedCount++;
        if (it->second.isExpired(now)) {
            std::string keyToPurge = it->first;
            ++it;
            purgeExpiredUnlocked(keyToPurge);
            expiredCount++;
        } else {
            ++it;
        }
    }

    return expiredCount;
}

} // namespace minicache::store
