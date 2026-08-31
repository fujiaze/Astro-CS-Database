// tests/backend/profile_gen_main.cpp — profile 生成驱动(薄封装, BENCH-004/005 测试用)
#include <cstdio>
#include <fstream>

#include "profile_gen.h"

std::string arg_of(int argc, char** argv, const std::string& key, const std::string& def = "") {
    for (int i = 1; i + 1 < argc; ++i)
        if (key == argv[i]) return argv[i + 1];
    return def;
}

int main(int argc, char** argv) {
    const std::string out = arg_of(argc, argv, "--out");
    const std::string mode = arg_of(argc, argv, "--mode", "quick");
    const std::string version = arg_of(argc, argv, "--version", "0.10.0-alpha.2");
    const std::string commit = arg_of(argc, argv, "--commit", "0");
    const std::string bsha = arg_of(argc, argv, "--backend-sha", "0");
    if (out.empty()) return 2;
    const std::string json =
        astrocs::backend_host::generate_profile_json(mode, version, commit, bsha);
    std::ofstream f(out, std::ios::binary | std::ios::trunc);
    f << json;
    std::printf("%s\n", out.c_str());
    return 0;
}
