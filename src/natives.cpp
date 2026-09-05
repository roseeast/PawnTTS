#include "natives.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

namespace pawntts {

std::shared_ptr<CacheManager> g_cache;
std::shared_ptr<HttpServer> g_http;
std::shared_ptr<TTSEngine> g_tts;
Config g_config;
std::set<AMX*> g_active_amx;
std::mutex g_amx_mutex;

static bool load_config(const std::string& path, Config& cfg) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    auto extract_string = [&](const std::string& key, std::string& out) {
        std::string search = "\"" + key + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return;
        pos = content.find('\"', pos);
        if (pos == std::string::npos) return;
        size_t end = content.find('\"', pos + 1);
        if (end != std::string::npos) {
            out = content.substr(pos + 1, end - pos - 1);
        }
    };

    auto extract_int = [&](const std::string& key, int& out) {
        std::string search = "\"" + key + "\"";
        size_t pos = content.find(search);
        if (pos == std::string::npos) return;
        pos = content.find(':', pos);
        if (pos == std::string::npos) return;
        while (pos < content.size() && (content[pos] == ':' || content[pos] == ' ' || content[pos] == '\t')) pos++;
        size_t end = pos;
        while (end < content.size() && (isdigit(content[end]) || content[end] == '-')) end++;
        if (end > pos) {
            try { out = std::stoi(content.substr(pos, end - pos)); } catch (...) {}
        }
    };

    int port = cfg.port;
    extract_int("port", port);
    if (port > 0 && port <= 65535) cfg.port = static_cast<uint16_t>(port);

    extract_string("bind_ip", cfg.bind_ip);
    extract_string("public_url", cfg.public_url);
    extract_string("default_voice", cfg.default_voice);
    extract_string("cache_dir", cfg.cache_dir);
    extract_string("provider", cfg.provider);
    extract_string("custom_http_url", cfg.custom_http_url);

    int mem_limit = static_cast<int>(cfg.cache_memory_limit);
    extract_int("cache_memory_limit", mem_limit);
    if (mem_limit > 0) cfg.cache_memory_limit = static_cast<size_t>(mem_limit);

    return true;
}

bool initialize_pawn_tts() {
    load_config("pawn_tts.json", g_config);

    g_cache = std::make_shared<CacheManager>(g_config.cache_dir, g_config.cache_memory_limit);
    g_http = std::make_shared<HttpServer>();
    g_http->set_cache_manager(g_cache);

    if (!g_http->start(g_config.port, g_config.bind_ip, g_config.public_url)) {
        std::cerr << "[PawnTTS] Failed to start HTTP audio server on port " << g_config.port << std::endl;
        return false;
    }

    g_tts = std::make_shared<TTSEngine>(g_cache);
    if (g_config.provider == "custom_http") {
        g_tts->set_provider(TTSProvider::CustomHttp);
        g_tts->set_custom_http_url(g_config.custom_http_url);
    } else {
        g_tts->set_provider(TTSProvider::Google);
    }

    g_tts->set_callback([](const TTSResult& res) {
        dispatch_tts_ready(res);
    });

    g_tts->start(2);

    std::cout << "[PawnTTS] v0.1.0 initialized successfully. Provider: " 
              << g_config.provider << " (Default voice: " << g_config.default_voice << ")" << std::endl;
    return true;
}

void shutdown_pawn_tts() {
    if (g_tts) {
        g_tts->stop();
        g_tts.reset();
    }
    if (g_http) {
        g_http->stop();
        g_http.reset();
    }
    g_cache.reset();
    std::cout << "[PawnTTS] Shutdown complete." << std::endl;
}

void process_tick() {
    if (g_tts) {
        g_tts->process_main_thread_events();
    }
}

void register_amx(AMX* amx) {
    std::lock_guard<std::mutex> lock(g_amx_mutex);
    g_active_amx.insert(amx);
}

void unregister_amx(AMX* amx) {
    std::lock_guard<std::mutex> lock(g_amx_mutex);
    g_active_amx.erase(amx);
}

// AMX String Helpers
static std::string amx_read_string(AMX* amx, cell param) {
    cell* addr = nullptr;
    if (amx_GetAddr(amx, param, &addr) != AMX_ERR_NONE || !addr) {
        return "";
    }
    int len = 0;
    amx_StrLen(addr, &len);
    if (len <= 0) return "";
    std::vector<char> buf(len + 1, 0);
    amx_GetString(buf.data(), addr, 0, len + 1);
    return std::string(buf.data());
}

static void amx_write_string(AMX* amx, cell param, const std::string& str, cell maxlen) {
    cell* addr = nullptr;
    if (amx_GetAddr(amx, param, &addr) == AMX_ERR_NONE && addr && maxlen > 0) {
        amx_SetString(addr, str.c_str(), 0, 0, maxlen);
    }
}

void dispatch_tts_ready(const TTSResult& res) {
    if (!g_http) return;
    std::string audio_url = g_http->build_audio_url(res.hash);

    std::lock_guard<std::mutex> lock(g_amx_mutex);
    for (AMX* amx : g_active_amx) {
        if (!amx) continue;

        // Callback 1: OnTTSGenerated(const hash[], const text[], const voice[], bool:success)
        int idx_gen = -1;
        if (amx_FindPublic(amx, "OnTTSGenerated", &idx_gen) == AMX_ERR_NONE && idx_gen >= 0) {
            cell amx_addr_hash = 0, amx_addr_text = 0, amx_addr_voice = 0;
            cell *phys_hash = nullptr, *phys_text = nullptr, *phys_voice = nullptr;

            amx_Allot(amx, res.hash.size() + 1, &amx_addr_hash, &phys_hash);
            amx_Allot(amx, res.text.size() + 1, &amx_addr_text, &phys_text);
            amx_Allot(amx, res.voice.size() + 1, &amx_addr_voice, &phys_voice);

            if (phys_hash) amx_SetString(phys_hash, res.hash.c_str(), 0, 0, res.hash.size() + 1);
            if (phys_text) amx_SetString(phys_text, res.text.c_str(), 0, 0, res.text.size() + 1);
            if (phys_voice) amx_SetString(phys_voice, res.voice.c_str(), 0, 0, res.voice.size() + 1);

            amx_Push(amx, res.success ? 1 : 0);
            amx_Push(amx, amx_addr_voice);
            amx_Push(amx, amx_addr_text);
            amx_Push(amx, amx_addr_hash);

            cell retval = 0;
            amx_Exec(amx, &retval, idx_gen);

            amx_Release(amx, amx_addr_voice);
            amx_Release(amx, amx_addr_text);
            amx_Release(amx, amx_addr_hash);
        }

        // If synthesis failed, do not attempt to play
        if (!res.success) continue;

        // Callback 2: OnTTSReady(playerid, const hash[], const url[], Float:x, Float:y, Float:z, Float:distance, usepos)
        int idx_ready = -1;
        if (amx_FindPublic(amx, "OnTTSReady", &idx_ready) == AMX_ERR_NONE && idx_ready >= 0) {
            cell amx_addr_hash = 0, amx_addr_url = 0;
            cell *phys_hash = nullptr, *phys_url = nullptr;

            amx_Allot(amx, res.hash.size() + 1, &amx_addr_hash, &phys_hash);
            amx_Allot(amx, audio_url.size() + 1, &amx_addr_url, &phys_url);

            if (phys_hash) amx_SetString(phys_hash, res.hash.c_str(), 0, 0, res.hash.size() + 1);
            if (phys_url) amx_SetString(phys_url, audio_url.c_str(), 0, 0, audio_url.size() + 1);

            amx_Push(amx, res.usepos ? 1 : 0);
            amx_Push(amx, amx_ftoc(res.distance));
            amx_Push(amx, amx_ftoc(res.z));
            amx_Push(amx, amx_ftoc(res.y));
            amx_Push(amx, amx_ftoc(res.x));
            amx_Push(amx, amx_addr_url);
            amx_Push(amx, amx_addr_hash);
            amx_Push(amx, res.playerid);

            cell retval = 0;
            amx_Exec(amx, &retval, idx_ready);

            amx_Release(amx, amx_addr_url);
            amx_Release(amx, amx_addr_hash);
        }
    }
}

// ----------------------------------------------------------------------------
// AMX Natives
// ----------------------------------------------------------------------------

// native bool:TTS_IsReady();
static cell AMX_NATIVE_CALL n_TTS_IsReady(AMX* /*amx*/, cell* /*params*/) {
    return (g_http && g_http->is_running() && g_tts) ? 1 : 0;
}

// native TTS_GetBaseURL(output[], maxlen = sizeof(output));
static cell AMX_NATIVE_CALL n_TTS_GetBaseURL(AMX* amx, cell* params) {
    if (!g_http) return 0;
    std::string base = g_http->get_base_url();
    amx_write_string(amx, params[1], base, params[2]);
    return 1;
}

// native TTS_SetPublicURL(const url[]);
static cell AMX_NATIVE_CALL n_TTS_SetPublicURL(AMX* amx, cell* params) {
    if (!g_http) return 0;
    std::string url = amx_read_string(amx, params[1]);
    g_http->set_public_url(url);
    return 1;
}

// native bool:TTS_Precache(const text[], const voice[] = "id", Float:speed = 1.0);
static cell AMX_NATIVE_CALL n_TTS_Precache(AMX* amx, cell* params) {
    if (!g_tts) return 0;
    std::string text = amx_read_string(amx, params[1]);
    std::string voice = amx_read_string(amx, params[2]);
    if (voice.empty()) voice = g_config.default_voice;
    float speed = amx_ctof(params[3]);
    if (speed <= 0.0f) speed = 1.0f;

    return g_tts->precache(text, voice, speed) ? 1 : 0;
}

// native bool:TTS_IsCached(const text[], const voice[] = "id", Float:speed = 1.0);
static cell AMX_NATIVE_CALL n_TTS_IsCached(AMX* amx, cell* params) {
    if (!g_cache) return 0;
    std::string text = amx_read_string(amx, params[1]);
    std::string voice = amx_read_string(amx, params[2]);
    if (voice.empty()) voice = g_config.default_voice;
    float speed = amx_ctof(params[3]);
    if (speed <= 0.0f) speed = 1.0f;

    std::string hash = TTSEngine::calculate_hash(text, voice, speed);
    return g_cache->has(hash) ? 1 : 0;
}

// native bool:TTS_Speak(playerid, const text[], const voice[] = "id", Float:speed = 1.0);
static cell AMX_NATIVE_CALL n_TTS_Speak(AMX* amx, cell* params) {
    if (!g_tts) return 0;
    TTSRequest req;
    req.playerid = static_cast<int>(params[1]);
    req.text = amx_read_string(amx, params[2]);
    req.voice = amx_read_string(amx, params[3]);
    if (req.voice.empty()) req.voice = g_config.default_voice;
    req.speed = amx_ctof(params[4]);
    if (req.speed <= 0.0f) req.speed = 1.0f;
    req.usepos = false;

    return g_tts->request_speech(req) ? 1 : 0;
}

// native bool:TTS_SpeakToAll(const text[], const voice[] = "id", Float:speed = 1.0);
static cell AMX_NATIVE_CALL n_TTS_SpeakToAll(AMX* amx, cell* params) {
    if (!g_tts) return 0;
    TTSRequest req;
    req.playerid = -1; // All players
    req.text = amx_read_string(amx, params[1]);
    req.voice = amx_read_string(amx, params[2]);
    if (req.voice.empty()) req.voice = g_config.default_voice;
    req.speed = amx_ctof(params[3]);
    if (req.speed <= 0.0f) req.speed = 1.0f;
    req.usepos = false;

    return g_tts->request_speech(req) ? 1 : 0;
}

// native bool:TTS_SpeakAtPos(const text[], const voice[] = "id", Float:x, Float:y, Float:z, Float:distance = 30.0, Float:speed = 1.0);
static cell AMX_NATIVE_CALL n_TTS_SpeakAtPos(AMX* amx, cell* params) {
    if (!g_tts) return 0;
    TTSRequest req;
    req.playerid = -1; // Broadcast to nearby players
    req.text = amx_read_string(amx, params[1]);
    req.voice = amx_read_string(amx, params[2]);
    if (req.voice.empty()) req.voice = g_config.default_voice;
    req.x = amx_ctof(params[3]);
    req.y = amx_ctof(params[4]);
    req.z = amx_ctof(params[5]);
    req.distance = amx_ctof(params[6]);
    req.speed = amx_ctof(params[7]);
    if (req.speed <= 0.0f) req.speed = 1.0f;
    req.usepos = true;

    return g_tts->request_speech(req) ? 1 : 0;
}

// native bool:TTS_SpeakAtPosForPlayer(playerid, const text[], const voice[] = "id", Float:x, Float:y, Float:z, Float:distance = 30.0, Float:speed = 1.0);
static cell AMX_NATIVE_CALL n_TTS_SpeakAtPosForPlayer(AMX* amx, cell* params) {
    if (!g_tts) return 0;
    TTSRequest req;
    req.playerid = static_cast<int>(params[1]);
    req.text = amx_read_string(amx, params[2]);
    req.voice = amx_read_string(amx, params[3]);
    if (req.voice.empty()) req.voice = g_config.default_voice;
    req.x = amx_ctof(params[4]);
    req.y = amx_ctof(params[5]);
    req.z = amx_ctof(params[6]);
    req.distance = amx_ctof(params[7]);
    req.speed = amx_ctof(params[8]);
    if (req.speed <= 0.0f) req.speed = 1.0f;
    req.usepos = true;

    return g_tts->request_speech(req) ? 1 : 0;
}

// native TTS_ClearCache(bool:clear_disk = false);
static cell AMX_NATIVE_CALL n_TTS_ClearCache(AMX* /*amx*/, cell* params) {
    if (!g_cache) return 0;
    g_cache->clear(params[1] != 0);
    return 1;
}

// native TTS_GetStats(&cached_mem, &cached_disk, &total_bytes_kb);
static cell AMX_NATIVE_CALL n_TTS_GetStats(AMX* amx, cell* params) {
    if (!g_cache) return 0;
    size_t mem_items = 0, disk_items = 0;
    uint64_t total_bytes = 0;
    g_cache->get_stats(mem_items, disk_items, total_bytes);

    cell* addr = nullptr;
    if (amx_GetAddr(amx, params[1], &addr) == AMX_ERR_NONE && addr) *addr = static_cast<cell>(mem_items);
    if (amx_GetAddr(amx, params[2], &addr) == AMX_ERR_NONE && addr) *addr = static_cast<cell>(disk_items);
    if (amx_GetAddr(amx, params[3], &addr) == AMX_ERR_NONE && addr) *addr = static_cast<cell>(total_bytes / 1024);
    return 1;
}

static const AMX_NATIVE_INFO s_natives[] = {
    {"TTS_IsReady", n_TTS_IsReady},
    {"TTS_GetBaseURL", n_TTS_GetBaseURL},
    {"TTS_SetPublicURL", n_TTS_SetPublicURL},
    {"TTS_Precache", n_TTS_Precache},
    {"TTS_IsCached", n_TTS_IsCached},
    {"TTS_Speak", n_TTS_Speak},
    {"TTS_SpeakToAll", n_TTS_SpeakToAll},
    {"TTS_SpeakAtPos", n_TTS_SpeakAtPos},
    {"TTS_SpeakAtPosForPlayer", n_TTS_SpeakAtPosForPlayer},
    {"TTS_ClearCache", n_TTS_ClearCache},
    {"TTS_GetStats", n_TTS_GetStats},
    {nullptr, nullptr}
};

const AMX_NATIVE_INFO* get_pawn_tts_natives() {
    return s_natives;
}

} // namespace pawntts

