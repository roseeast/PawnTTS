#pragma once

#include "common.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <list>
#include <mutex>

namespace pawntts {

struct CacheItem {
    std::string hash;
    std::vector<uint8_t> data;
    size_t size;
};

class CacheManager {
public:
    explicit CacheManager(std::string cache_dir = "cache/tts", size_t max_memory_items = 256);
    ~CacheManager();

    // Check if audio exists either in memory or on disk
    bool has(const std::string& hash);

    // Retrieve audio data. Reads from disk if not in memory.
    bool get(const std::string& hash, std::vector<uint8_t>& out_data);

    // Save audio to disk and memory cache
    bool put(const std::string& hash, const std::vector<uint8_t>& data);

    // Get absolute or relative disk path for a hash
    std::string get_file_path(const std::string& hash) const;

    // Clear memory and/or disk cache
    void clear(bool clear_disk = false);

    // Stats
    void get_stats(size_t& memory_items, size_t& disk_items, uint64_t& total_disk_bytes);

private:
    void ensure_directory_exists() const;
    bool read_from_disk(const std::string& hash, std::vector<uint8_t>& out_data);
    bool write_to_disk(const std::string& hash, const std::vector<uint8_t>& data);

    std::string m_cache_dir;
    size_t m_max_memory_items;
    mutable std::mutex m_mutex;

    // LRU cache
    std::list<std::string> m_lru_order;
    std::unordered_map<std::string, std::pair<std::vector<uint8_t>, std::list<std::string>::iterator>> m_memory_cache;
};

} // namespace pawntts

