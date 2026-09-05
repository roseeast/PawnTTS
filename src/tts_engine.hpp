#pragma once

#include "common.hpp"
#include "cache_manager.hpp"
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <functional>

namespace pawntts {

enum class TTSProvider {
    Google,      // Free Google Translate TTS over HTTP (zero API key)
    CustomHttp   // Local Piper TTS HTTP or custom TTS server
};

struct TTSRequest {
    int playerid{-1};
    std::string text;
    std::string voice{"id"};
    float speed{1.0f};
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float distance{30.0f};
    bool usepos{false};
    std::string hash;
};

struct TTSResult {
    int playerid{-1};
    std::string hash;
    std::string text;
    std::string voice;
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float distance{30.0f};
    bool usepos{false};
    bool success{false};
};

using AudioReadyCallback = std::function<void(const TTSResult& result)>;

class TTSEngine {
public:
    explicit TTSEngine(std::shared_ptr<CacheManager> cache);
    ~TTSEngine();

    void set_provider(TTSProvider provider) { m_provider = provider; }
    void set_custom_http_url(const std::string& url) { m_custom_url = url; }
    void set_callback(AudioReadyCallback cb) { m_callback = std::move(cb); }

    void start(size_t num_workers = 2);
    void stop();

    // Submit a request. If already cached, will immediately invoke callback or return true.
    bool request_speech(const TTSRequest& req);

    // Pre-cache without playing to any player
    bool precache(const std::string& text, const std::string& voice, float speed = 1.0f);

    // Process completed items (called from main server tick/ProcessTick)
    void process_main_thread_events();

    static std::string calculate_hash(const std::string& text, const std::string& voice, float speed);

private:
    void worker_loop();
    bool synthesize_google(const std::string& text, const std::string& voice, std::vector<uint8_t>& out_mp3);
    bool synthesize_custom_http(const std::string& text, const std::string& voice, std::vector<uint8_t>& out_mp3);

    std::shared_ptr<CacheManager> m_cache;
    TTSProvider m_provider{TTSProvider::Google};
    std::string m_custom_url;
    AudioReadyCallback m_callback;

    std::atomic<bool> m_running{false};
    std::vector<std::thread> m_workers;
    ThreadSafeQueue<TTSRequest> m_request_queue;
    ThreadSafeQueue<TTSResult> m_completed_queue;
};

} // namespace pawntts

