// tests/backend/loader_probe_main.cpp — loader 行为探针(ABI-002 测试驱动)
// argv: <backends_dir> <manifest.json>
// 输出: PARSE_FAIL/REJECT_SECURITY/LOADED backend_id=.../SELFTEST_OK/FALLBACK <reason>
// 恒 return 0(拒绝≠崩溃; 进程存活是断言的一部分)。
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#include "backend_loader.h"
#include "cpu_features.h"

extern "C" {
int astrocs_host_services_default_v1(astrocs_host_services_v1* out, void** state_out);
void astrocs_host_services_destroy_state_v1(void* state);
void astrocs_host_state_set_budget_v1(void* state, uint32_t cpus, uint32_t max_workers,
                                      astrocs_host_services_v1* out);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: loader_probe <backends_dir> <manifest>\n");
        return 2;
    }
    astrocs_host_services_v1 host;
    void* state = nullptr;
    if (astrocs_host_services_default_v1(&host, &state) != ACS_OK) return 2;
    astrocs_host_state_set_budget_v1(state, 1, 1, &host);

    std::ifstream f(argv[2], std::ios::binary);
    std::stringstream buf;
    buf << f.rdbuf();

    std::vector<astrocs::backend_host::ManifestEntry> entries;
    std::string err;
    if (!astrocs::backend_host::parse_backends_manifest(buf.str(), &entries, &err)) {
        std::printf("FALLBACK malformed: %s\n", err.c_str());
        return 0;
    }
    for (const auto& e : entries) {
        astrocs_backend_api_v1 api;
        std::memset(&api, 0, sizeof(api));
        void* handle = nullptr;
        std::string reason;
        auto r = astrocs::backend_host::load_backend(argv[1], e, &host, &api, &handle, &reason);
        if (r.decision == astrocs::backend_host::LoadResult::OK) {
            std::printf("LOADED backend_id=%s kernels=%u\n", api.backend_id, api.kernel_count);
            if (api.self_test && api.self_test(&host) == ACS_OK) std::printf("SELFTEST_OK\n");
            astrocs::backend_host::close_backend(handle);
        } else if (r.decision == astrocs::backend_host::LoadResult::REJECT_SECURITY) {
            std::printf("REJECT_SECURITY %s\n", reason.c_str());
        } else {
            std::printf("FALLBACK %s\n", reason.c_str());
        }
    }
    astrocs_host_services_destroy_state_v1(state);
    return 0;
}
