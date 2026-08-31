// tests/backend/p3_session_probe.cpp — SYN-007 端到端 Phase3 会话探针
// 用法:
//   probe run <hips_dir> <ra> <dec> <scale> <w> <h> <sampler> <out_dir>
//       -> "<status> <output_fits_path> <order_used> <covered> <total> <sha256>" | "FAIL <code> <msg>"
//   probe validate <hips_dir> <ra> <dec> <scale> <w> <h> <sampler>
//       -> "<status>" | "FAIL <code> <msg>"
#include "p3_session.h"
#include "astrocs/common_abi_v1.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace astrocs::phase3;

// ---- 最小 host_services v1 stub ----
static void* stub_alloc(void*, uint64_t size, uint64_t /*align*/) { return std::calloc(1, size ? size : 1); }
static void  stub_free(void*, void* p) { std::free(p); }
static void  stub_log(void*, int, const char*, const char* msg) { if (msg) std::fprintf(stderr, "[p3log] %s\n", msg); }
static int   stub_cancel(void*) { return 0; }

static astrocs_host_services_v1 make_host() {
    astrocs_host_services_v1 h; std::memset(&h, 0, sizeof(h));
    h.struct_size = sizeof(h); h.abi_version = ACS_ABI_VERSION_V1;
    h.allocator.struct_size = sizeof(h.allocator); h.allocator.abi_version = ACS_ABI_VERSION_V1;
    h.allocator.user_data = nullptr; h.allocator.alloc = stub_alloc; h.allocator.free = stub_free;
    h.logger.struct_size = sizeof(h.logger); h.logger.abi_version = ACS_ABI_VERSION_V1;
    h.logger.user_data = nullptr; h.logger.log = stub_log;
    h.cancel.struct_size = sizeof(h.cancel); h.cancel.abi_version = ACS_ABI_VERSION_V1;
    h.cancel.user_data = nullptr; h.cancel.is_cancelled = stub_cancel;
    return h;
}

int main(int argc, char** argv) {
    if (argc < 3) return 2;
    const std::string mode = argv[1];
    auto host = make_host();
    acs_handle h = nullptr;
    if (p3_session_create(&host, &h) != ACS_OK) { std::printf("FAIL create\n"); return 1; }

    if (mode == "validate" && argc >= 9) {
        char req[4096];
        std::snprintf(req, sizeof(req),
            "{\"source\":{\"hips_dir\":\"%s\"},\"center\":{\"ra_deg\":%.10f,\"dec_deg\":%.10f},"
            "\"scale_deg_per_px\":%.10f,\"width_px\":%d,\"height_px\":%d,\"sampler\":\"%s\","
            "\"longitude_parity\":\"east_left\"}",
            argv[2], atof(argv[3]), atof(argv[4]), atof(argv[5]), atoi(argv[6]), atoi(argv[7]), argv[8]);
        acs_status st = p3_session_validate(h, ACS_SPAN_U8((uint8_t*)req, (uint64_t)std::strlen(req)));
        std::printf("%d\n", (int)st);
        p3_session_destroy(h); return 0;
    }
    if (mode == "run" && argc >= 10) {
        char req[4096];
        std::snprintf(req, sizeof(req),
            "{\"source\":{\"hips_dir\":\"%s\"},\"center\":{\"ra_deg\":%.10f,\"dec_deg\":%.10f},"
            "\"scale_deg_per_px\":%.10f,\"width_px\":%d,\"height_px\":%d,\"sampler\":\"%s\","
            "\"longitude_parity\":\"east_left\",\"output_dir\":\"%s\"}",
            argv[2], atof(argv[3]), atof(argv[4]), atof(argv[5]), atoi(argv[6]), atoi(argv[7]), argv[8], argv[9]);
        acs_status st = p3_session_run(h, ACS_SPAN_U8((uint8_t*)req, (uint64_t)std::strlen(req)));
        if (st != ACS_OK) { std::printf("FAIL %d %s\n", (int)st, last_error(h).c_str()); p3_session_destroy(h); return 1; }
        acs_span_u8 out{};
        if (p3_session_inspect(h, &out) == ACS_OK) {
            if (out.data && out.count) std::fwrite(out.data, 1, out.count, stdout);
            if (out.data) host.allocator.free(host.allocator.user_data, out.data);
            std::printf("\n");
        }
        p3_session_destroy(h); return 0;
    }
    p3_session_destroy(h);
    return 2;
}
