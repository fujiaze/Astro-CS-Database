// 实验/issue87-stage3-tile-integrity/session_tile_probe.cpp — Issue #87 正式 session 入口探针
//
// 直接调用生产 C API `p3_session_run`（lib/phase3_session），验证 session 入口
// 对 tile 完整性故障的传播：真实故障必须非零返回、不发布 FITS、last_error 携带
// 具体 P3-TILE-* 机器码与 tile ipix。合法输入仍成功出图。
//
// 用法:
//   session_tile_probe <hips_dir> <output_dir> <ra> <dec> <w> <h> <scale> <sampler>
// 输出: 单行 JSON {status,status_name,last_error,output_fits,output_fits_size}
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

extern "C" {
#include "acsd/common_abi_v1.h"
}
#include "p3_session.h"

namespace {

void* host_alloc(void*, uint64_t size, uint64_t) { return std::malloc((size_t)size); }
void host_free(void*, void* p) { std::free(p); }
void host_log(void*, int, const char*, const char*) {}
int host_cancelled(void*) { return 0; }
int host_acquire(void*, uint32_t) { return 0; }
void host_release(void*, uint32_t) {}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') { o += '\\'; o += c; }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else o += c;
    }
    return o;
}

const char* status_name(acsd_status s) {
    switch (s) {
        case ACS_OK: return "ACS_OK";
        case ACS_ERR_PARAM: return "ACS_ERR_PARAM";
        case ACS_ERR_ABI_MISMATCH: return "ACS_ERR_ABI_MISMATCH";
        case ACS_ERR_NOMEM: return "ACS_ERR_NOMEM";
        case ACS_ERR_IO: return "ACS_ERR_IO";
        case ACS_ERR_UNSUPPORTED: return "ACS_ERR_UNSUPPORTED";
        case ACS_ERR_CANCELLED: return "ACS_ERR_CANCELLED";
        case ACS_ERR_STATE: return "ACS_ERR_STATE";
        case ACS_ERR_BUDGET: return "ACS_ERR_BUDGET";
        case ACS_ERR_SELFTEST: return "ACS_ERR_SELFTEST";
        default: return "ACS_ERR_INTERNAL";
    }
}

long file_size(const std::string& p) {
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return -1;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fclose(f);
    return n;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 9) {
        std::fprintf(stderr,
                     "usage: %s <hips_dir> <out_dir> <ra> <dec> <w> <h> <scale> <sampler>\n",
                     argv[0]);
        return 64;
    }
    const std::string hips_dir = argv[1];
    const std::string out_dir = argv[2];
    const std::string ra = argv[3];
    const std::string dec = argv[4];
    const std::string w = argv[5];
    const std::string h = argv[6];
    const std::string scale = argv[7];
    const std::string sampler = argv[8];

    acsd_host_services_v1 host{};
    host.struct_size = (uint32_t)sizeof(acsd_host_services_v1);
    host.abi_version = ACS_ABI_VERSION_V1;
    host.allocator.struct_size = (uint32_t)sizeof(acsd_allocator);
    host.allocator.abi_version = ACS_ABI_VERSION_V1;
    host.allocator.alloc = host_alloc;
    host.allocator.free = host_free;
    host.logger.struct_size = (uint32_t)sizeof(acsd_logger);
    host.logger.abi_version = ACS_ABI_VERSION_V1;
    host.logger.log = host_log;
    host.cancel.struct_size = (uint32_t)sizeof(acsd_cancel);
    host.cancel.abi_version = ACS_ABI_VERSION_V1;
    host.cancel.is_cancelled = host_cancelled;
    host.budget.struct_size = (uint32_t)sizeof(acsd_thread_budget);
    host.budget.abi_version = ACS_ABI_VERSION_V1;
    host.budget.available_cpus = 4;
    host.budget.max_workers = 4;
    host.budget.acquire = host_acquire;
    host.budget.release = host_release;

    const std::string req =
        std::string("{\"schema_version\":\"1\",\"source\":{\"hips_dir\":\"") +
        json_escape(hips_dir) + "\"},\"output_dir\":\"" + json_escape(out_dir) +
        "\",\"center\":{\"ra_deg\":" + ra + ",\"dec_deg\":" + dec + "}," +
        "\"width_px\":" + w + ",\"height_px\":" + h +
        ",\"scale_deg_per_px\":" + scale + ",\"sampler\":\"" + sampler +
        "\",\"output_mode\":\"surface_brightness\"}";

    acsd_handle handle = nullptr;
    acsd_status st = p3_session_create(&host, &handle);
    if (st != ACS_OK) {
        std::printf("{\"status\":%d,\"status_name\":\"%s\","
                    "\"last_error\":\"p3_session_create failed\"}\n",
                    (int)st, status_name(st));
        return 1;
    }
    acsd_span_u8 span{};
    span.head.struct_size = (uint32_t)sizeof(acsd_span_u8);
    span.head.abi_version = ACS_ABI_VERSION_V1;
    span.data = reinterpret_cast<uint8_t*>(const_cast<char*>(req.data()));
    span.count = (uint64_t)req.size();
    st = p3_session_run(handle, span);
    const std::string err = acsd::phase3::last_error(handle);
    p3_session_destroy(handle);

    const std::string fits = out_dir + "/output_phase3.fits";
    std::printf("{\"status\":%d,\"status_name\":\"%s\",\"last_error\":\"%s\","
                "\"output_fits\":\"%s\",\"output_fits_size\":%ld}\n",
                (int)st, status_name(st), json_escape(err).c_str(),
                json_escape(fits).c_str(), file_size(fits));
    return (st == ACS_OK) ? 0 : 1;
}
