// http_server.h - HTTP 服务器头文件 (healpix_browser_cpp)
// 功能: 用 winsock2 实现简化的 HTTP 服务器, 提供静态文件服务和 JSON API
// 用途: 为前端 (healpix_browser_web) 提供数据 API 和静态文件
// 依赖: winsock2 (Windows 系统库), BrowserBackend

#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "browser_backend.h"
#include <string>

class HttpServer {
public:
    HttpServer(BrowserBackend& backend, int port = 18080);
    ~HttpServer();

    // 启动服务器 (阻塞运行, 直到进程终止)
    void run();

private:
    BrowserBackend& backend_;
    int port_;

    // 处理 HTTP 请求, 返回完整 HTTP 响应 (含状态行/头/体)
    std::string handle_request(const std::string& method, const std::string& path);

    // 提供静态文件 (从 ../healpix_browser_web/ 读取)
    std::string serve_static_file(const std::string& path);

    // JSON API 响应
    std::string json_file_info();
    std::string json_required_leaves(double ra, double dec, double zoom, double fov);
    std::string json_leaf(uint64_t ipix, uint32_t nside);
    std::string json_all_data();

    // 构造完整 HTTP 响应
    std::string build_response(int status, const std::string& content_type,
                               const std::string& body);
};

#endif // HTTP_SERVER_H
