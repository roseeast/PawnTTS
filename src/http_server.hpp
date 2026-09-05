#pragma once

#include "common.hpp"
#include "cache_manager.hpp"
#include <string>
#include <atomic>
#include <thread>
#include <vector>
#include <memory>

namespace pawntts {

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    bool start(uint16_t port = 7788, const std::string& bind_ip = "0.0.0.0", const std::string& public_url = "");
    void stop();

    bool is_running() const { return m_running.load(); }
    uint16_t get_port() const { return m_port; }
    std::string get_base_url() const;
    void set_public_url(const std::string& url);

    void set_cache_manager(std::shared_ptr<CacheManager> cache) { m_cache = cache; }

    std::string build_audio_url(const std::string& hash) const;

private:
    void accept_loop();
    void handle_client(SOCKET client_socket, std::string client_ip);

    std::atomic<bool> m_running{false};
    SOCKET m_listen_socket{INVALID_SOCKET};
    uint16_t m_port{7788};
    std::string m_bind_ip{"0.0.0.0"};
    std::string m_public_url;
    std::thread m_accept_thread;
    std::vector<std::thread> m_worker_threads;
    std::shared_ptr<CacheManager> m_cache;
};

} // namespace pawntts

