// tests/backend/hips_properties_probe_main.cpp — P3-001 校验器探针
// 用法: probe validate <dir>          → "OK <order> <width> <format> <frame>" 或 "FAIL <err>"
//       probe parse <properties文件>  → 同上
//       probe path <path>             → "OK" 或 "FAIL <err>"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "hips_properties.h"

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    const std::string mode = argv[1];
    using namespace astrocs::phase3;
    std::string err;
    HipsProperties p{};
    if (mode == "validate") {
        if (hips_product_validate(argv[2], &p, &err)) {
            std::printf("OK %d %d %s %s\n", p.order, p.tile_width,
                        p.tile_format.c_str(), p.frame.c_str());
            return 0;
        }
        std::printf("FAIL %s\n", err.c_str());
        return 1;
    }
    if (mode == "parse") {
        std::ifstream f(argv[2], std::ios::binary);
        if (!f) { std::printf("FAIL open failed\n"); return 1; }
        std::stringstream buf; buf << f.rdbuf();
        if (hips_properties_parse(buf.str(), &p, &err)) {
            std::printf("OK %d %d %s %s\n", p.order, p.tile_width,
                        p.tile_format.c_str(), p.frame.c_str());
            return 0;
        }
        std::printf("FAIL %s\n", err.c_str());
        return 1;
    }
    if (mode == "path") {
        if (path_is_safe(argv[2], &err)) { std::printf("OK\n"); return 0; }
        std::printf("FAIL %s\n", err.c_str());
        return 1;
    }
    return 2;
}
