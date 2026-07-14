// browser_main.cpp - C++ 渲染后端入口 (healpix_browser_cpp)
// 功能: 程序入口, 打开 .hiss/.hcsd 文件, 启动 HTTP 服务器, 自动打开浏览器
// 用法: browser_cpp <file.hiss|file.hcsd>
// 依赖: BrowserBackend + HttpServer + healpix_io.dll + winsock2

#include "browser_backend.h"
#include "http_server.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <string>
#include <windows.h>

// 显示用法
static void print_usage(const char* prog) {
    std::cerr << "HEALPix 浏览器 C++ 后端" << std::endl;
    std::cerr << "用法: " << prog << " <file.hiss|file.hcsd>" << std::endl;
    std::cerr << "  file.hiss  单帧 HEALPix 存储文件" << std::endl;
    std::cerr << "  file.hcsd  HEALPix 天球数据库文件 (按需子叶加载)" << std::endl;
}

// 获取可执行文件所在目录 (Windows)
static std::string get_exe_dir() {
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len == 0) return ".";
    std::string p(path, len);
    size_t pos = p.find_last_of("\\/");
    if (pos != std::string::npos) return p.substr(0, pos);
    return ".";
}

// 检查 healpix_io.dll 是否已加载 (防御性编程)
// 注: browser_cpp.exe 隐式链接 healpix_io.dll, 进程启动时已加载
// 若 DLL 缺失或损坏, 进程根本不会启动; 此检查作为额外防御
static bool check_healpix_io_dll() {
    // 1) 用 GetModuleHandleA 检查 DLL 是否已加载到进程地址空间
    HMODULE hDll = GetModuleHandleA("healpix_io.dll");
    if (hDll != nullptr) {
        std::cout << "[启动] healpix_io.dll 已加载" << std::endl;
        return true;
    }

    // 2) DLL 未加载 - 检查文件是否存在 (给出更友好的错误信息)
    std::string exe_dir = get_exe_dir();
    std::string dll_path = exe_dir + "\\healpix_io.dll";
    std::ifstream f(dll_path, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "错误: healpix_io.dll 加载失败, 文件不存在: " << dll_path << std::endl;
        std::cerr << "请确保 healpix_io.dll 与 browser_cpp.exe 在同一目录" << std::endl;
    } else {
        f.close();
        std::cerr << "错误: healpix_io.dll 已找到但无法加载: " << dll_path << std::endl;
        std::cerr << "可能原因: DLL 架构不匹配 (32/64位) 或 DLL 损坏" << std::endl;
        std::cerr << "Windows 错误码: " << GetLastError() << std::endl;
    }
    return false;
}

int main(int argc, char* argv[]) {
    // 解析命令行参数
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string file_path = argv[1];
    if (file_path == "-h" || file_path == "--help") {
        print_usage(argv[0]);
        return 0;
    }

    // 检查 --no-browser 选项 (用于自动化测试, 不自动打开系统浏览器)
    bool no_browser = false;
    for (int i = 2; i < argc; i++) {
        if (std::string(argv[i]) == "--no-browser") {
            no_browser = true;
        }
    }

    // 检查 healpix_io.dll 加载状态 (防御性, 缺失时进程不会启动到此处)
    if (!check_healpix_io_dll()) {
        return 2;
    }

    // 检查目标文件是否存在
    std::ifstream target(file_path, std::ios::binary);
    if (!target.is_open()) {
        std::cerr << "错误: 文件不存在: " << file_path << std::endl;
        return 1;
    }
    target.close();

    // 打开数据文件
    BrowserBackend backend;
    if (backend.open_file(file_path) != 0) {
        std::cerr << "错误: 打开文件失败: " << file_path << std::endl;
        std::cerr << "  可能原因: Magic 不匹配 (非 .hiss/.hcsd 文件) 或 文件格式损坏" << std::endl;
        return 1;
    }

    // 输出文件信息
    std::cout << "========================================" << std::endl;
    std::cout << "文件已打开: " << file_path << std::endl;
    std::cout << "模式: " << (backend.is_hiss() ? "单帧 (.hiss)" : "球面 (.hcsd)")
              << std::endl;
    std::cout << "nside: " << backend.get_nside()
              << ", n_pix: " << backend.get_n_pix() << std::endl;
    std::cout << "========================================" << std::endl;

    // 启动 HTTP 服务器
    const int port = 18080;
    HttpServer server(backend, port);

    std::cout << "HTTP 服务器启动: http://localhost:" << port << std::endl;
    std::cout << "前端页面: http://localhost:" << port << "/" << std::endl;

    // 自动打开默认浏览器 (Windows: start 命令), 除非指定 --no-browser
    if (!no_browser) {
        std::string open_cmd = "start \"\" http://localhost:" + std::to_string(port) + "/";
        std::cout << "正在打开浏览器..." << std::endl;
        int sys_ret = std::system(open_cmd.c_str());
        (void)sys_ret;  // 忽略返回值 (即使浏览器打开失败, 服务器仍继续运行)
    } else {
        std::cout << "(--no-browser 模式, 不自动打开浏览器)" << std::endl;
    }

    // 阻塞运行 HTTP 服务器 (直到 Ctrl+C 终止)
    server.run();

    return 0;
}
