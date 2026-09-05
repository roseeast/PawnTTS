#include "cache_manager.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace pawntts {

CacheManager::CacheManager(std::string cache_dir, size_t max_memory_items)
    : m_cache_dir(std::move(cache_dir))
    , m_max_memory_items(max_memory_items)
{
    ensure_directory_exists();
}

CacheManager::~CacheManager() = default;

void CacheManager::ensure_directory_exists() const {
    try {
        if (!fs::exists(m_cache_dir)) {
            fs::create_directories(m_cache_dir);
        }
    } catch (const std::exception& e) {
        std::cerr << "[PawnTTS] Failed to create cache directory " << m_cache_dir << ": " << e.what() << std::endl;
    }
}

std::string CacheManager::get_file_path(const std::string& hash) const {
    return (fs::path(m_cache_dir) / (hash + ".mp3")).string();
}

bool CacheManager::has(const std::string& hash) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_memory_cache.find(hash) != m_memory_cache.end()) {
        return true;
    }
    std::string path = get_file_path(hash);
    std::error_code ec;
    return fs::exists(path, ec) && fs::file_size(path, ec) > 0;
}

bool CacheManager::get(const std::string& hash, std::vector<uint8_t>& out_data) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check memory cache
    auto it = m_memory_cache.find(hash);
    if (it != m_memory_cache.end()) {
        // Move to front of LRU
        m_lru_order.erase(it->second.second);
        m_lru_order.push_front(hash);
        it->second.second = m_lru_order.begin();
        out_data = it->second.first;
        return true;
    }

    // Check disk cache
    if (read_from_disk(hash, out_data)) {
        // Add to memory cache with LRU eviction
        if (m_memory_cache.size() >= m_max_memory_items && !m_lru_order.empty()) {
            std::string oldest = m_lru_order.back();
            m_lru_order.pop_back();
            m_memory_cache.erase(oldest);
        }
        m_lru_order.push_front(hash);
        m_memory_cache[hash] = {out_data, m_lru_order.begin()};
        return true;
    }

    return false;
}

bool CacheManager::put(const std::string& hash, const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    // Write to disk
    if (!write_to_disk(hash, data)) {
        return false;
    }

    // Insert into memory cache
    auto it = m_memory_cache.find(hash);
    if (it != m_memory_cache.end()) {
        m_lru_order.erase(it->second.second);
    } else if (m_memory_cache.size() >= m_max_memory_items && !m_lru_order.empty()) {
        std::string oldest = m_lru_order.back();
        m_lru_order.pop_back();
        m_memory_cache.erase(oldest);
    }

    m_lru_order.push_front(hash);
    m_memory_cache[hash] = {data, m_lru_order.begin()};
    return true;
}

bool CacheManager::read_from_disk(const std::string& hash, std::vector<uint8_t>& out_data) {
    std::string path = get_file_path(hash);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    std::streamsize size = file.tellg();
    if (size <= 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    out_data.resize(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(out_data.data()), size)) {
        out_data.clear();
        return false;
    }
    return true;
}

bool CacheManager::write_to_disk(const std::string& hash, const std::vector<uint8_t>& data) {
    ensure_directory_exists();
    std::string path = get_file_path(hash);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return file.good();
}

void CacheManager::clear(bool clear_disk) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_memory_cache.clear();
    m_lru_order.clear();

    if (clear_disk) {
        try {
            if (fs::exists(m_cache_dir)) {
                for (const auto& entry : fs::directory_iterator(m_cache_dir)) {
                    if (entry.is_regular_file()) {
                        fs::remove(entry.path());
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[PawnTTS] Failed to clear disk cache: " << e.what() << std::endl;
        }
    }
}

void CacheManager::get_stats(size_t& memory_items, size_t& disk_items, uint64_t& total_disk_bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    memory_items = m_memory_cache.size();
    disk_items = 0;
    total_disk_bytes = 0;

    try {
        if (fs::exists(m_cache_dir)) {
            for (const auto& entry : fs::directory_iterator(m_cache_dir)) {
                if (entry.is_regular_file()) {
                    disk_items++;
                    std::error_code ec;
                    total_disk_bytes += fs::file_size(entry.path(), ec);
                }
            }
        }
    } catch (...) {}
}

} // namespace pawntts

