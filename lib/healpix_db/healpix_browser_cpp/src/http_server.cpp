// http_server.cpp - HTTP 服务器实现 (healpix_browser_cpp)
// 功能: 用 winsock2 实现简化的 HTTP 服务器, 提供静态文件服务和 JSON API
// 用途: 为前端 (healpix_browser_web) 提供:
//   - 静态文件服务 (index.html / js / css / shaders)
//   - JSON API: /api/file_info, /api/required_leaves, /api/leaf, /api/all_data
// 依赖: winsock2 (Windows 系统库, -lws2_32)

#include "http_server.h"
#include "browser_backend.h"
#include "healpix_io.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <chrono>

// ============================================================================
// 辅助函数
// ============================================================================

// base64 编码: 将任意字节缓冲区编码为 base64 字符串
// 用途: 将 float32 像素数组 / uint64 ipix 数组以二进制形式传输, 避免 JSON 文本数组过大
// 编码后体积约为原始数据的 4/3 倍, 远小于 JSON 文本数组 (每个 float 平均 ~12 字符)
static std::string base64_encode(const uint8_t* data, size_t len) {
    static const char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < len) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len) n |= (uint32_t)data[i + 2];
        out += table[(n >> 18) & 0x3F];
        out += table[(n >> 12) & 0x3F];
        out += (i + 1 < len) ? table[(n >> 6) & 0x3F] : '=';
        out += (i + 2 < len) ? table[n & 0x3F] : '=';
    }
    return out;
}

// JSON 字符串转义
static std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c;
        }
    }
    return out;
}

// URL 解码
static std::string url_decode(const std::string& s) {
    std::string result;
    result.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size()) {
            auto hex_val = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            int hi = hex_val(s[i + 1]);
            int lo = hex_val(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                result += (char)((hi << 4) | lo);
                i += 2;
            } else {
                result += s[i];
            }
        } else if (s[i] == '+') {
            result += ' ';
        } else {
            result += s[i];
        }
    }
    return result;
}

// 从查询字符串提取参数值
static std::string get_query_param(const std::string& query, const std::string& key) {
    std::string needle = key + "=";
    size_t pos = query.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    size_t end = query.find('&', pos);
    if (end == std::string::npos) end = query.size();
    return url_decode(query.substr(pos, end - pos));
}

// 获取可执行文件所在目录 (用于定位 ../healpix_browser_web/)
static std::string get_module_dir() {
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len == 0) return ".";
    std::string p(path, len);
    size_t pos = p.find_last_of("\\/");
    if (pos != std::string::npos) return p.substr(0, pos);
    return ".";
}

// 拼接路径 (用反斜杠, Windows 风格)
static std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    char last = a.back();
    if (last == '\\' || last == '/') {
        return a + b;
    }
    return a + "\\" + b;
}

// ============================================================================
// HttpServer 构造 / 析构
// ============================================================================

HttpServer::HttpServer(BrowserBackend& backend, int port)
    : backend_(backend), port_(port) {}

HttpServer::~HttpServer() {}

// ============================================================================
// 构造完整 HTTP 响应
// ============================================================================

std::string HttpServer::build_response(int status, const std::string& content_type,
                                       const std::string& body) {
    std::string status_text;
    switch (status) {
        case 200: status_text = "OK"; break;
        case 400: status_text = "Bad Request"; break;
        case 404: status_text = "Not Found"; break;
        case 405: status_text = "Method Not Allowed"; break;
        case 500: status_text = "Internal Server Error"; break;
        default:  status_text = "OK"; break;
    }

    std::ostringstream resp;
    resp << "HTTP/1.1 " << status << " " << status_text << "\r\n";
    resp << "Content-Type: " << content_type << "\r\n";
    resp << "Content-Length: " << body.size() << "\r\n";
    resp << "Access-Control-Allow-Origin: *\r\n";
    resp << "Access-Control-Allow-Methods: GET, OPTIONS\r\n";
    resp << "Cache-Control: no-cache\r\n";
    resp << "Connection: close\r\n";
    resp << "\r\n";
    resp << body;
    return resp.str();
}

// ============================================================================
// 静态文件服务
// ============================================================================

std::string HttpServer::serve_static_file(const std::string& path) {
    // 路径安全检查: 防止路径遍历
    if (path.find("..") != std::string::npos) {
        return build_response(404, "text/plain; charset=utf-8", "403 Forbidden");
    }

    // 静态文件根目录: ../healpix_browser_web/ (相对于可执行文件)
    std::string web_dir = join_path(get_module_dir(), "..\\healpix_browser_web");
    std::string rel_path = path;
    // 去掉前导 '/'
    if (!rel_path.empty() && rel_path[0] == '/') rel_path = rel_path.substr(1);
    std::string full_path = join_path(web_dir, rel_path);

    std::ifstream f(full_path, std::ios::binary);
    if (!f.is_open()) {
        std::string body = "404 Not Found: " + path;
        return build_response(404, "text/plain; charset=utf-8", body);
    }

    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
    f.close();

    // 根据扩展名决定 Content-Type
    std::string content_type = "application/octet-stream";
    auto ends_with = [](const std::string& s, const std::string& suffix) {
        return s.size() >= suffix.size() &&
               s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    if (ends_with(path, ".html")) content_type = "text/html; charset=utf-8";
    else if (ends_with(path, ".js"))   content_type = "application/javascript; charset=utf-8";
    else if (ends_with(path, ".css"))  content_type = "text/css; charset=utf-8";
    else if (ends_with(path, ".glsl")) content_type = "text/plain; charset=utf-8";
    else if (ends_with(path, ".json")) content_type = "application/json; charset=utf-8";

    return build_response(200, content_type, content);
}

// ============================================================================
// JSON API: /api/file_info
// ============================================================================

std::string HttpServer::json_file_info() {
    std::ostringstream ss;
    ss << "{";
    ss << "\"is_hiss\": " << (backend_.is_hiss() ? "true" : "false") << ",";
    ss << "\"is_hcsd\": " << (backend_.is_hcsd() ? "true" : "false") << ",";
    ss << "\"nside\": " << backend_.get_nside() << ",";
    ss << "\"n_pix\": " << backend_.get_n_pix() << ",";
    ss << "\"file_path\": \"" << json_escape(backend_.get_file_path()) << "\"";
    ss << "}";
    return build_response(200, "application/json; charset=utf-8", ss.str());
}

// ============================================================================
// JSON API: /api/required_leaves?ra=X&dec=Y&zoom=Z&fov=W
// ============================================================================

std::string HttpServer::json_required_leaves(double ra, double dec,
                                             double zoom, double fov) {
    ViewParams view;
    view.center_ra = ra;
    view.center_dec = dec;
    view.zoom = zoom;
    view.fov_deg = fov;

    std::vector<uint64_t> leaves = backend_.get_required_leaves(view);

    std::ostringstream ss;
    ss << "{\"leaves\":[";
    for (size_t i = 0; i < leaves.size(); i++) {
        if (i > 0) ss << ",";
        ss << leaves[i];
    }
    ss << "],\"n_leaves\":" << leaves.size() << ",\"target_nside\":" << backend_.get_nside() << "}";
    return build_response(200, "application/json; charset=utf-8", ss.str());
}

// ============================================================================
// JSON API: /api/leaf?ipix=X&nside=Y
// ============================================================================

std::string HttpServer::json_leaf(uint64_t ipix, uint32_t nside) {
    // 如果指定了 nside > 0, 用指定值; 否则用 backend 默认 nside (不降采样)
    uint32_t target_nside = (nside > 0) ? nside : backend_.get_nside();

    LeafData leaf = backend_.load_leaf(ipix, target_nside);

    // 使用 base64 二进制编码 ipix (uint64) 和 pixels (float32), 避免 JSON 文本数组过大
    // 前端 api-client.js 通过 _base64ToFloat32Array / BigInt64Array 解码
    std::string ipix_b64 = base64_encode(
        reinterpret_cast<const uint8_t*>(leaf.ipix),
        leaf.n_pix * sizeof(uint64_t));
    std::string pixel_b64 = base64_encode(
        reinterpret_cast<const uint8_t*>(leaf.pixel),
        leaf.n_pix * sizeof(float));

    std::ostringstream ss;
    ss << "{";
    ss << "\"leaf_ipix\":" << leaf.leaf_ipix << ",";
    ss << "\"n_pix\":" << leaf.n_pix << ",";
    ss << "\"nside\":" << leaf.nside << ",";
    ss << "\"ipix_base64\":\"" << ipix_b64 << "\",";
    ss << "\"pixels_base64\":\"" << pixel_b64 << "\"";
    ss << "}";

    std::string body = ss.str();

    // 释放子叶数据 (注意: get_all_data 返回的指针由 backend 持有, 不应释放)
    // load_leaf 返回的是新分配的 (或 hcsd_read_leaf 分配的), 可以释放
    if (leaf.ipix) {
        // 用 hio_free (兼容 healpix_io 的 malloc) 释放
        hio_free(leaf.ipix);
        leaf.ipix = nullptr;
    }
    if (leaf.pixel) {
        hio_free(leaf.pixel);
        leaf.pixel = nullptr;
    }

    return build_response(200, "application/json; charset=utf-8", body);
}

// ============================================================================
// JSON API: /api/all_data (仅 .hiss 模式)
// ============================================================================

std::string HttpServer::json_all_data() {
    if (!backend_.is_hiss()) {
        std::string body = "{\"error\":\"not in hiss mode\"}";
        return build_response(400, "application/json; charset=utf-8", body);
    }

    auto t0 = std::chrono::steady_clock::now();

    LeafData data = backend_.get_all_data();

    auto t1 = std::chrono::steady_clock::now();

    // 使用 base64 二进制编码 ipix (uint64) 和 pixels (float32), 避免 JSON 文本数组过大
    std::string ipix_b64 = base64_encode(
        reinterpret_cast<const uint8_t*>(data.ipix),
        data.n_pix * sizeof(uint64_t));

    auto t2 = std::chrono::steady_clock::now();

    std::string pixel_b64 = base64_encode(
        reinterpret_cast<const uint8_t*>(data.pixel),
        data.n_pix * sizeof(float));

    auto t3 = std::chrono::steady_clock::now();

    std::ostringstream ss;
    ss << "{";
    ss << "\"n_pix\":" << data.n_pix << ",";
    ss << "\"nside\":" << data.nside << ",";
    ss << "\"nested\":true,";
    ss << "\"ipix_base64\":\"" << ipix_b64 << "\",";
    ss << "\"pixels_base64\":\"" << pixel_b64 << "\"";
    ss << "}";

    std::string body = ss.str();

    auto t4 = std::chrono::steady_clock::now();

    // 释放中间大字符串, 减少内存峰值
    ipix_b64.clear();
    ipix_b64.shrink_to_fit();
    pixel_b64.clear();
    pixel_b64.shrink_to_fit();

    std::string resp = build_response(200, "application/json; charset=utf-8", body);

    auto t5 = std::chrono::steady_clock::now();

    auto ms = [](auto a, auto b) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(b - a).count();
    };
    std::cerr << "[all_data] n_pix=" << data.n_pix
              << " get=" << ms(t0, t1) << "ms"
              << " b64_ipix=" << ms(t1, t2) << "ms"
              << " b64_pixel=" << ms(t2, t3) << "ms"
              << " json=" << ms(t3, t4) << "ms"
              << " build=" << ms(t4, t5) << "ms"
              << " body=" << (body.size() / 1024) << "KB"
              << " resp=" << (resp.size() / 1024) << "KB"
              << std::endl;

    // 注意: data.ipix/pixel 由 backend_ 持有, 不释放
    return resp;
}

// ============================================================================
// 请求路由
// ============================================================================

std::string HttpServer::handle_request(const std::string& method,
                                       const std::string& full_path) {
    // 处理 OPTIONS 预检请求
    if (method == "OPTIONS") {
        return build_response(200, "text/plain", "");
    }

    if (method != "GET") {
        return build_response(405, "text/plain; charset=utf-8", "Method Not Allowed");
    }

    // 分离 path 和 query
    std::string path = full_path;
    std::string query;
    size_t qpos = full_path.find('?');
    if (qpos != std::string::npos) {
        path = full_path.substr(0, qpos);
        query = full_path.substr(qpos + 1);
    }

    try {
        if (path == "/" || path == "/index.html") {
            return serve_static_file("/index.html");
        } else if (path == "/api/ping") {
            return build_response(200, "application/json; charset=utf-8", "{\"ok\":true}");
        } else if (path == "/api/file_info") {
            return json_file_info();
        } else if (path == "/api/required_leaves") {
            std::string ra_str = get_query_param(query, "ra");
            std::string dec_str = get_query_param(query, "dec");
            std::string zoom_str = get_query_param(query, "zoom");
            std::string fov_str = get_query_param(query, "fov");
            if (ra_str.empty() || dec_str.empty() || fov_str.empty()) {
                return build_response(400, "application/json; charset=utf-8",
                                      "{\"error\":\"missing ra/dec/fov\"}");
            }
            double ra = std::stod(ra_str);
            double dec = std::stod(dec_str);
            double zoom = zoom_str.empty() ? 1.0 : std::stod(zoom_str);
            double fov = std::stod(fov_str);
            return json_required_leaves(ra, dec, zoom, fov);
        } else if (path == "/api/leaf") {
            std::string ipix_str = get_query_param(query, "ipix");
            std::string nside_str = get_query_param(query, "nside");
            if (ipix_str.empty()) {
                return build_response(400, "application/json; charset=utf-8",
                                      "{\"error\":\"missing ipix\"}");
            }
            uint64_t ipix = std::stoull(ipix_str);
            uint32_t nside = nside_str.empty() ? 0 : (uint32_t)std::stoul(nside_str);
            return json_leaf(ipix, nside);
        } else if (path == "/api/all_data") {
            return json_all_data();
        } else {
            // 静态文件 (js, css, shaders 等)
            return serve_static_file(path);
        }
    } catch (const std::exception& e) {
        std::cerr << "[HttpServer] 请求处理异常: " << e.what() << std::endl;
        std::string body = std::string("{\"error\":\"") + e.what() + "\"}";
        return build_response(400, "application/json; charset=utf-8", body);
    }
}

// ============================================================================
// 服务器主循环
// ============================================================================

void HttpServer::run() {
    WSADATA wsaData;
    int ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (ret != 0) {
        std::cerr << "[HttpServer] WSAStartup 失败: " << ret << std::endl;
        return;
    }

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        std::cerr << "[HttpServer] socket() 失败: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return;
    }

    // 允许端口重用 (避免 TIME_WAIT 导致 bind 失败)
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR,
               (const char*)&opt, sizeof(opt));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons((u_short)port_);

    if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] bind() 失败: " << WSAGetLastError()
                  << " (port=" << port_ << ")" << std::endl;
        closesocket(listen_sock);
        WSACleanup();
        return;
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[HttpServer] listen() 失败: " << WSAGetLastError() << std::endl;
        closesocket(listen_sock);
        WSACleanup();
        return;
    }

    std::cout << "[HttpServer] 监听 http://127.0.0.1:" << port_ << "/" << std::endl;
    std::cout << "[HttpServer] 按 Ctrl+C 退出" << std::endl;

    char buf[65536];  // 64KB 接收缓冲区

    while (true) {
        sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_sock = accept(listen_sock, (sockaddr*)&client_addr, &client_len);
        if (client_sock == INVALID_SOCKET) {
            int err = WSAGetLastError();
            std::cerr << "[HttpServer] accept() 失败: " << err << std::endl;
            if (err == WSAEINTR) break;  // 信号中断
            continue;
        }

        // 接收请求 (一次 recv 即可, HTTP 请求通常较小)
        int recv_len = recv(client_sock, buf, sizeof(buf) - 1, 0);
        if (recv_len <= 0) {
            closesocket(client_sock);
            continue;
        }
        buf[recv_len] = '\0';

        // 解析请求行: "GET /path?query HTTP/1.1\r\n..."
        std::string request(buf, recv_len);
        size_t eol = request.find("\r\n");
        std::string request_line = (eol != std::string::npos)
                                   ? request.substr(0, eol) : request;

        std::istringstream iss(request_line);
        std::string method, full_path, version;
        iss >> method >> full_path >> version;

        if (method.empty() || full_path.empty()) {
            std::string resp = build_response(400, "text/plain", "Bad Request");
            send(client_sock, resp.c_str(), (int)resp.size(), 0);
            closesocket(client_sock);
            continue;
        }

        std::cout << "[HttpServer] " << method << " " << full_path << std::endl;

        std::string response = handle_request(method, full_path);

        // 发送完整响应 (循环 send 直到全部发出)
        int total = (int)response.size();
        int sent = 0;
        auto send_t0 = std::chrono::steady_clock::now();
        while (sent < total) {
            int n = send(client_sock, response.c_str() + sent, total - sent, 0);
            if (n == SOCKET_ERROR) {
                std::cerr << "[HttpServer] send() 失败: " << WSAGetLastError() << std::endl;
                break;
            }
            sent += n;
        }
        auto send_t1 = std::chrono::steady_clock::now();
        auto send_ms = std::chrono::duration_cast<std::chrono::milliseconds>(send_t1 - send_t0).count();
        if (total > 100000) {
            std::cerr << "[send] " << full_path << " size=" << (total / 1024) << "KB send_ms=" << send_ms << std::endl;
        }

        // 关闭连接 (Connection: close 模式)
        shutdown(client_sock, SD_BOTH);
        closesocket(client_sock);
    }

    closesocket(listen_sock);
    WSACleanup();
}
