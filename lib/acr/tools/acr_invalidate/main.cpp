// lib/acr/tools/acr_invalidate/main.cpp — acr-invalidate CLI
// Phase E：作废 profile（删除 routes.json）。
//
// 用法：
//   acr-invalidate [--profile <path>] [--yes]
//
// 默认 --profile ./routes.json，需要 --yes 确认删除
#include "static_router.hpp"
#include "route_profile.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#if defined(_WIN32)
  #include <io.h>
  #define ACR_UNLINK _unlink
#else
  #include <unistd.h>
  #define ACR_UNLINK ::unlink
#endif

namespace {

void print_usage() {
    std::fprintf(stderr,
        "acr-invalidate: 作废 ACR profile\n"
        "用法: acr-invalidate [options]\n"
        "Options:\n"
        "  --profile <path>   routes.json 路径 (默认 routes.json)\n"
        "  --yes, -y          跳过确认提示\n"
        "  --help, -h         显示帮助\n");
}

struct Args {
    std::string profile{"routes.json"};
    bool yes{false};
    bool help{false};
};

bool parse_args(int argc, char** argv, Args& out) {
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help" || a == "-h") { out.help = true; return true; }
        if (a == "--yes" || a == "-y") { out.yes = true; continue; }
        if (a == "--profile") {
            if (++i >= argc) { std::fprintf(stderr, "error: --profile 缺少参数\n"); return false; }
            out.profile = argv[i];
            continue;
        }
        std::fprintf(stderr, "error: 未知参数: %s\n", a.c_str());
        return false;
    }
    return true;
}

} // anonymous namespace

int main(int argc, char** argv) {
    Args args;
    if (!parse_args(argc, argv, args)) return 1;
    if (args.help) { print_usage(); return 0; }

    if (!args.yes) {
        std::printf("即将删除 profile: %s\n", args.profile.c_str());
        std::printf("确认？(y/N) ");
        std::fflush(stdout);
        char buf[16];
        if (!std::cin.getline(buf, sizeof(buf))) return 1;
        if (std::strncmp(buf, "y", 1) != 0 && std::strncmp(buf, "Y", 1) != 0) {
            std::printf("已取消\n");
            return 0;
        }
    }

    // 删除文件
    int rc = ACR_UNLINK(args.profile.c_str());
    if (rc != 0) {
        std::fprintf(stderr, "[acr-invalidate] 删除失败（文件不存在或无权限）: %s\n",
            args.profile.c_str());
        return 2;
    }
    std::printf("[acr-invalidate] 已作废 profile: %s\n", args.profile.c_str());

    // 同步通知 resolver 清缓存（如有运行中的 ACR 实例，下次 resolve 会重新加载）
    // 注：acr-invalidate 是独立进程，无法直接通知其他进程；此处仅做信息提示
    std::printf("[acr-invalidate] 提示：其他 ACR 进程需重启或调用 StaticRouteResolver::invalidate_cache()\n");
    return 0;
}
