#include "tts_engine.hpp"
#include <iostream>
#include <sstream>

namespace pawntts {

TTSEngine::TTSEngine(std::shared_ptr<CacheManager> cache)
    : m_cache(cache)
{
}

TTSEngine::~TTSEngine() {
    stop();
}

std::string TTSEngine::calculate_hash(const std::string& text, const std::string& voice, float speed) {
    std::ostringstream ss;
    ss << text << "|" << voice << "|" << std::fixed << std::setprecision(2) << speed;
    return SHA256::hash_string(ss.str());
}

void TTSEngine::start(size_t num_workers) {
    if (m_running.load()) {
        return;
    }
    m_running.store(true);

    for (size_t i = 0; i < num_workers; ++i) {
        m_workers.emplace_back(&TTSEngine::worker_loop, this);
    }
}

void TTSEngine::stop() {
    if (!m_running.load()) {
        return;
    }
    m_running.store(false);

    for (auto& w : m_workers) {
        if (w.joinable()) {
            w.join();
        }
    }
    m_workers.clear();
}

bool TTSEngine::request_speech(const TTSRequest& req) {
    TTSRequest r = req;
    if (r.hash.empty()) {
        r.hash = calculate_hash(r.text, r.voice, r.speed);
    }

    // Check if already in cache
    if (m_cache && m_cache->has(r.hash)) {
        TTSResult res;
        res.playerid = r.playerid;
        res.hash = r.hash;
        res.text = r.text;
        res.voice = r.voice;
        res.x = r.x;
        res.y = r.y;
        res.z = r.z;
        res.distance = r.distance;
        res.usepos = r.usepos;
        res.success = true;

        m_completed_queue.push(res);
        return true;
    }

    // Push to background synthesis queue
    m_request_queue.push(r);
    return true;
}

bool TTSEngine::precache(const std::string& text, const std::string& voice, float speed) {
    std::string hash = calculate_hash(text, voice, speed);
    if (m_cache && m_cache->has(hash)) {
        return true;
    }

    TTSRequest req;
    req.playerid = -999; // Precache flag
    req.text = text;
    req.voice = voice;
    req.speed = speed;
    req.hash = hash;
    m_request_queue.push(req);
    return true;
}

void TTSEngine::process_main_thread_events() {
    TTSResult res;
    while (m_completed_queue.pop(res, std::chrono::milliseconds(0))) {
        if (m_callback) {
            m_callback(res);
        }
    }
}

void TTSEngine::worker_loop() {
    while (m_running.load()) {
        TTSRequest req;
        if (!m_request_queue.pop(req, std::chrono::milliseconds(100))) {
            continue;
        }

        if (req.hash.empty()) {
            req.hash = calculate_hash(req.text, req.voice, req.speed);
        }

        TTSResult res;
        res.playerid = req.playerid;
        res.hash = req.hash;
        res.text = req.text;
        res.voice = req.voice;
        res.x = req.x;
        res.y = req.y;
        res.z = req.z;
        res.distance = req.distance;
        res.usepos = req.usepos;

        // If already cached, succeed immediately
        if (m_cache && m_cache->has(req.hash)) {
            res.success = true;
            if (req.playerid != -999) {
                m_completed_queue.push(res);
            }
            continue;
        }

        std::vector<uint8_t> mp3_data;
        bool ok = false;
        if (m_provider == TTSProvider::CustomHttp && !m_custom_url.empty()) {
            ok = synthesize_custom_http(req.text, req.voice, mp3_data);
        } else {
            ok = synthesize_google(req.text, req.voice, mp3_data);
        }

        if (ok && !mp3_data.empty()) {
            if (m_cache) {
                m_cache->put(req.hash, mp3_data);
            }
            res.success = true;
        } else {
            res.success = false;
            std::cerr << "[PawnTTS] Failed to synthesize audio for: \"" << req.text << "\"" << std::endl;
        }

        if (req.playerid != -999) {
            m_completed_queue.push(res);
        }
    }
}

// Simple HTTP GET socket client helper
static bool http_get_raw(const std::string& host, uint16_t port, const std::string& path,
                         const std::string& user_agent, std::vector<uint8_t>& out_body)
{
    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);
    if (getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res) != 0 || !res) {
        return false;
    }

    SOCKET sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == INVALID_SOCKET) {
        freeaddrinfo(res);
        return false;
    }

    // 5-second socket timeout
#ifdef _WIN32
    DWORD timeout_ms = 5000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout_ms, sizeof(timeout_ms));
#else
    struct timeval tv{};
    tv.tv_sec = 5;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif

    if (connect(sock, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen)) == SOCKET_ERROR) {
        PAWN_TTS_CLOSE_SOCKET(sock);
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);

    std::ostringstream req;
    req << "GET " << path << " HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "User-Agent: " << user_agent << "\r\n"
        << "Accept: */*\r\n"
        << "Connection: close\r\n\r\n";

    std::string req_str = req.str();
    if (send(sock, req_str.c_str(), static_cast<int>(req_str.size()), 0) <= 0) {
        PAWN_TTS_CLOSE_SOCKET(sock);
        return false;
    }

    std::vector<uint8_t> response_buffer;
    std::vector<uint8_t> chunk(8192);
    int bytes = 0;
    while ((bytes = recv(sock, reinterpret_cast<char*>(chunk.data()), static_cast<int>(chunk.size()), 0)) > 0) {
        response_buffer.insert(response_buffer.end(), chunk.begin(), chunk.begin() + bytes);
    }
    PAWN_TTS_CLOSE_SOCKET(sock);

    if (response_buffer.empty()) {
        return false;
    }

    // Split headers and body
    const char* pattern = "\r\n\r\n";
    auto it = std::search(response_buffer.begin(), response_buffer.end(), pattern, pattern + 4);
    if (it == response_buffer.end()) {
        return false;
    }

    std::string headers(response_buffer.begin(), it);
    if (headers.find("200 OK") == std::string::npos && headers.find("200 ok") == std::string::npos) {
        return false;
    }

    out_body.assign(it + 4, response_buffer.end());
    return !out_body.empty();
}

bool TTSEngine::synthesize_google(const std::string& text, const std::string& voice, std::vector<uint8_t>& out_mp3) {
    std::string lang = voice.empty() ? "id" : voice;
    std::string encoded_text = url_encode(text);
    std::string path = "/translate_tts?ie=UTF-8&tl=" + lang + "&client=tw-ob&q=" + encoded_text;
    std::string user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";

    return http_get_raw("translate.google.com", 80, path, user_agent, out_mp3);
}

bool TTSEngine::synthesize_custom_http(const std::string& text, const std::string& voice, std::vector<uint8_t>& out_mp3) {
    // Parse custom URL (e.g. http://127.0.0.1:5000/tts?text=...&voice=...)
    if (m_custom_url.rfind("http://", 0) != 0) {
        return false;
    }

    std::string without_scheme = m_custom_url.substr(7);
    size_t slash = without_scheme.find('/');
    std::string host_port = (slash != std::string::npos) ? without_scheme.substr(0, slash) : without_scheme;
    std::string base_path = (slash != std::string::npos) ? without_scheme.substr(slash) : "/";

    std::string host = host_port;
    uint16_t port = 80;
    size_t colon = host_port.find(':');
    if (colon != std::string::npos) {
        host = host_port.substr(0, colon);
        try {
            port = static_cast<uint16_t>(std::stoi(host_port.substr(colon + 1)));
        } catch (...) {}
    }

    std::string sep = (base_path.find('?') != std::string::npos) ? "&" : "?";
    std::string full_path = base_path + sep + "text=" + url_encode(text) + "&voice=" + url_encode(voice);
    std::string user_agent = "PawnTTS/0.1.0";

    return http_get_raw(host, port, full_path, user_agent, out_mp3);
}

} // namespace pawntts

