#include "http_server.hpp"
#include <iostream>
#include <sstream>
#include <regex>

namespace pawntts {

HttpServer::HttpServer() {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

HttpServer::~HttpServer() {
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool HttpServer::start(uint16_t port, const std::string& bind_ip, const std::string& public_url) {
    if (m_running.load()) {
        return true;
    }

    m_port = port;
    m_bind_ip = bind_ip;
    m_public_url = public_url;

    m_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen_socket == INVALID_SOCKET) {
        std::cerr << "[PawnTTS] Failed to create socket." << std::endl;
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(m_listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(m_port);
    if (m_bind_ip == "0.0.0.0" || m_bind_ip.empty()) {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        inet_pton(AF_INET, m_bind_ip.c_str(), &server_addr.sin_addr);
    }

    if (bind(m_listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "[PawnTTS] Failed to bind HTTP server to " << m_bind_ip << ":" << m_port << std::endl;
        PAWN_TTS_CLOSE_SOCKET(m_listen_socket);
        m_listen_socket = INVALID_SOCKET;
        return false;
    }

    if (listen(m_listen_socket, 128) == SOCKET_ERROR) {
        std::cerr << "[PawnTTS] Failed to listen on HTTP socket." << std::endl;
        PAWN_TTS_CLOSE_SOCKET(m_listen_socket);
        m_listen_socket = INVALID_SOCKET;
        return false;
    }

    m_running.store(true);
    m_accept_thread = std::thread(&HttpServer::accept_loop, this);

    std::cout << "[PawnTTS] Audio HTTP streaming server listening on " 
              << m_bind_ip << ":" << m_port 
              << " (Base URL: " << get_base_url() << ")" << std::endl;

    return true;
}

void HttpServer::stop() {
    if (!m_running.load()) {
        return;
    }

    m_running.store(false);

    if (m_listen_socket != INVALID_SOCKET) {
        PAWN_TTS_CLOSE_SOCKET(m_listen_socket);
        m_listen_socket = INVALID_SOCKET;
    }

    if (m_accept_thread.joinable()) {
        m_accept_thread.join();
    }

    for (auto& t : m_worker_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    m_worker_threads.clear();
}

std::string HttpServer::get_base_url() const {
    if (!m_public_url.empty()) {
        // Strip trailing slash if present
        if (m_public_url.back() == '/') {
            return m_public_url.substr(0, m_public_url.size() - 1);
        }
        return m_public_url;
    }
    return "http://127.0.0.1:" + std::to_string(m_port);
}

void HttpServer::set_public_url(const std::string& url) {
    m_public_url = url;
}

std::string HttpServer::build_audio_url(const std::string& hash) const {
    return get_base_url() + "/audio/" + hash + ".mp3";
}

void HttpServer::accept_loop() {
    while (m_running.load()) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        SOCKET client_sock = accept(m_listen_socket, (struct sockaddr*)&client_addr, &client_len);

        if (client_sock == INVALID_SOCKET) {
            if (!m_running.load()) {
                break;
            }
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        // Spawn detached or pooled worker for client HTTP connection
        std::thread([this, client_sock, ip = std::string(client_ip)]() {
            handle_client(client_sock, ip);
        }).detach();
    }
}

void HttpServer::handle_client(SOCKET client_socket, std::string client_ip) {
    std::vector<char> buffer(4096);
    int bytes_received = recv(client_socket, buffer.data(), static_cast<int>(buffer.size()) - 1, 0);
    if (bytes_received <= 0) {
        PAWN_TTS_CLOSE_SOCKET(client_socket);
        return;
    }
    buffer[bytes_received] = '\0';
    std::string request_str(buffer.data());

    std::istringstream request_stream(request_str);
    std::string method, path, protocol;
    request_stream >> method >> path >> protocol;

    if (method != "GET" && method != "HEAD") {
        std::string response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        send(client_socket, response.c_str(), static_cast<int>(response.size()), 0);
        PAWN_TTS_CLOSE_SOCKET(client_socket);
        return;
    }

    // Parse Range header if present
    int64_t range_start = -1;
    int64_t range_end = -1;
    std::string line;
    while (std::getline(request_stream, line) && line != "\r" && !line.empty()) {
        if (line.rfind("Range:", 0) == 0 || line.rfind("range:", 0) == 0) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string range_val = line.substr(eq + 1);
                size_t dash = range_val.find('-');
                if (dash != std::string::npos) {
                    try {
                        std::string s_start = range_val.substr(0, dash);
                        std::string s_end = range_val.substr(dash + 1);
                        if (!s_start.empty()) range_start = std::stoll(s_start);
                        if (!s_end.empty()) range_end = std::stoll(s_end);
                    } catch (...) {}
                }
            }
        }
    }

    // Health / status endpoint
    if (path == "/health" || path == "/status") {
        size_t mem_items = 0, disk_items = 0;
        uint64_t total_bytes = 0;
        if (m_cache) {
            m_cache->get_stats(mem_items, disk_items, total_bytes);
        }
        std::ostringstream json;
        json << "{\"status\":\"ok\",\"service\":\"PawnTTS\",\"cached_memory\":" << mem_items
             << ",\"cached_disk\":" << disk_items << ",\"cache_bytes\":" << total_bytes << "}";
        std::string body = json.str();

        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\n"
             << "Content-Type: application/json\r\n"
             << "Content-Length: " << body.size() << "\r\n"
             << "Access-Control-Allow-Origin: *\r\n"
             << "Connection: close\r\n\r\n"
             << body;
        std::string resp_str = resp.str();
        send(client_socket, resp_str.c_str(), static_cast<int>(resp_str.size()), 0);
        PAWN_TTS_CLOSE_SOCKET(client_socket);
        return;
    }

    // Audio endpoint: /audio/<hash>.mp3 or /audio/<hash>
    if (path.rfind("/audio/", 0) == 0) {
        std::string hash = path.substr(7);
        // Strip .mp3 or query parameters if present
        size_t qmark = hash.find('?');
        if (qmark != std::string::npos) {
            hash = hash.substr(0, qmark);
        }
        if (hash.size() > 4 && hash.substr(hash.size() - 4) == ".mp3") {
            hash = hash.substr(0, hash.size() - 4);
        }

        std::vector<uint8_t> audio_data;
        if (m_cache && m_cache->get(hash, audio_data) && !audio_data.empty()) {
            int64_t total_size = static_cast<int64_t>(audio_data.size());
            int64_t start = (range_start >= 0) ? range_start : 0;
            int64_t end = (range_end >= 0 && range_end < total_size) ? range_end : (total_size - 1);
            if (start > end || start >= total_size) {
                std::string resp = "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */" +
                                   std::to_string(total_size) + "\r\nConnection: close\r\n\r\n";
                send(client_socket, resp.c_str(), static_cast<int>(resp.size()), 0);
                PAWN_TTS_CLOSE_SOCKET(client_socket);
                return;
            }

            int64_t content_len = (end - start) + 1;
            bool is_range = (range_start >= 0 || range_end >= 0);

            std::ostringstream resp;
            if (is_range) {
                resp << "HTTP/1.1 206 Partial Content\r\n"
                     << "Content-Range: bytes " << start << "-" << end << "/" << total_size << "\r\n";
            } else {
                resp << "HTTP/1.1 200 OK\r\n";
            }

            resp << "Content-Type: audio/mpeg\r\n"
                 << "Accept-Ranges: bytes\r\n"
                 << "Content-Length: " << content_len << "\r\n"
                 << "Access-Control-Allow-Origin: *\r\n"
                 << "Connection: close\r\n\r\n";

            std::string header_str = resp.str();
            send(client_socket, header_str.c_str(), static_cast<int>(header_str.size()), 0);

            if (method != "HEAD") {
                const char* send_ptr = reinterpret_cast<const char*>(audio_data.data() + start);
                int64_t remaining = content_len;
                while (remaining > 0) {
                    int chunk = static_cast<int>(std::min<int64_t>(remaining, 16384));
                    int sent = send(client_socket, send_ptr, chunk, 0);
                    if (sent <= 0) break;
                    send_ptr += sent;
                    remaining -= sent;
                }
            }

            PAWN_TTS_CLOSE_SOCKET(client_socket);
            return;
        }

        // Audio not found
        std::string resp = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\nContent-Length: 15\r\nConnection: close\r\n\r\nAudio Not Found";
        send(client_socket, resp.c_str(), static_cast<int>(resp.size()), 0);
        PAWN_TTS_CLOSE_SOCKET(client_socket);
        return;
    }

    // Default 200 OK for root
    std::string welcome = "PawnTTS Audio Streaming Server is active.\n";
    std::ostringstream resp;
    resp << "HTTP/1.1 200 OK\r\n"
         << "Content-Type: text/plain\r\n"
         << "Content-Length: " << welcome.size() << "\r\n"
         << "Connection: close\r\n\r\n"
         << welcome;
    std::string resp_str = resp.str();
    send(client_socket, resp_str.c_str(), static_cast<int>(resp_str.size()), 0);
    PAWN_TTS_CLOSE_SOCKET(client_socket);
}

} // namespace pawntts

