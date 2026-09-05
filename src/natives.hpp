#pragma once

#include "common.hpp"
#include "cache_manager.hpp"
#include "http_server.hpp"
#include "tts_engine.hpp"
#include <amx/amx.h>
#include <set>
#include <mutex>
#include <memory>

namespace pawntts {

struct Config {
    uint16_t port{7788};
    std::string bind_ip{"0.0.0.0"};
    std::string public_url;
    std::string default_voice{"id"};
    std::string cache_dir{"cache/tts"};
    size_t cache_memory_limit{256};
    std::string provider{"google"};
    std::string custom_http_url;
};

// Global shared instances
extern std::shared_ptr<CacheManager> g_cache;
extern std::shared_ptr<HttpServer> g_http;
extern std::shared_ptr<TTSEngine> g_tts;
extern Config g_config;
extern std::set<AMX*> g_active_amx;
extern std::mutex g_amx_mutex;

// Lifecycle
bool initialize_pawn_tts();
void shutdown_pawn_tts();
void process_tick();

// AMX management
void register_amx(AMX* amx);
void unregister_amx(AMX* amx);
void dispatch_tts_ready(const TTSResult& res);

// AMX native table getter
const AMX_NATIVE_INFO* get_pawn_tts_natives();

} // namespace pawntts

