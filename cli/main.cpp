// astrocs CLI — 单一用户入口 (V5, CLI-001)
// help/version 接通; 其余子命令为 schema 化 stub(API-002 §1 命令树)。
// MSVC/GCC 双兼容: 严格 C++17, 无扩展, 无平台宏分支。
#include <cstdio>
#include <cstring>
#include <string>

#include "version_generated.h"

namespace {

const char* kHelp =
    "astrocs --version [--json]\n"
    "astrocs hardware inspect --json\n"
    "astrocs config init --output <path>\n"
    "astrocs config validate --config <path>\n"
    "astrocs config show-effective --config <path> [--cpu-profile <path>] --json\n"
    "astrocs benchmark cpu (--quick|--full) [--output <path>]\n"
    "astrocs doctor --json\n"
    "astrocs test synthetic --group <all|calibration|wcs_psf|noise_snr|drizzle|upm|rejection_integration|pipeline>\n"
    "astrocs phase1 run --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs phase2 run --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs phase3 run --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs run --phases <1|2|3|1,2|1,2,3> --config <path> [--cpu-profile <path>] [--events-jsonl]\n"
    "astrocs verify --run-manifest <path> --json\n";

const char* kKnownSubcommands[] = {
    "hardware", "config", "benchmark", "doctor", "test",
    "phase1", "phase2", "phase3", "run", "verify"};

int exitCodeFor(const char* code) {
    if (std::strcmp(code, "OK") == 0) return 0;
    if (std::strcmp(code, "ARGS") == 0) return 2;
    if (std::strcmp(code, "INPUT") == 0) return 3;
    if (std::strcmp(code, "SCIENCE") == 0) return 4;
    if (std::strcmp(code, "BACKEND") == 0) return 5;
    if (std::strcmp(code, "COMPUTE") == 0) return 6;
    if (std::strcmp(code, "IO") == 0) return 7;
    if (std::strcmp(code, "INTEGRITY") == 0) return 8;
    if (std::strcmp(code, "CANCEL") == 0) return 9;
    if (std::strcmp(code, "RESOURCE") == 0) return 10;
    return 70;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fputs(kHelp, stderr);
        return exitCodeFor("ARGS");
    }
    const std::string a1 = argv[1];
    if (a1 == "--help" || a1 == "-h") {
        std::fputs(kHelp, stdout);
        return exitCodeFor("OK");
    }
    if (a1 == "--version") {
        const bool json = (argc >= 3 && std::strcmp(argv[2], "--json") == 0);
        if (json) {
            std::printf("{\"schema_version\":\"1\",\"name\":\"astrocs\",\"version\":\"%s\"}\n",
                        ASTROCS_VERSION_STRING);
        } else {
            std::printf("astrocs %s\n", ASTROCS_VERSION_STRING);
        }
        return exitCodeFor("OK");
    }
    bool known = false;
    for (const char* k : kKnownSubcommands) {
        if (a1 == k) { known = true; break; }
    }
    if (!known) {
        std::fprintf(stderr, "astrocs: unknown command '%s'\n", a1.c_str());
        std::fputs(kHelp, stderr);
        return exitCodeFor("ARGS");
    }
    // 已知子命令: CLI-001 stub 阶段仅打印计划占位(错误码=计算未执行, 用 2 提示参数期)
    std::fprintf(stderr, "astrocs: '%s' is declared by the CLI contract but not wired in this build "
                         "(see docs/api/CLI_PROTOCOL_V1.md)\n", a1.c_str());
    return exitCodeFor("ARGS");
}
