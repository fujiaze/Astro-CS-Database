// 实验/issue91-stage3-bunit/session_entry_probe.cpp — Issue #91 正式 session 入口探针
//
// 目的：直接调用**正式生产 session API** `p3_session_run`（lib/phase3_session，
// 生产链接闭包成员），验证同一个 signal/properties 在 session 入口与 CLI/scheduler
// 入口给出**同一 verdict**。这不是 CLI 复刻，也不伪造 CLI 运行 —— session 入口
// 不在 CLI 的 phase3 IR 节点链上（见 REPORT「入口可达性」一节），只能经其生产
// C API 驱动。
//
// 用法: session_entry_probe <hips_dir> <output_dir>
// 输出: 单行 JSON {status,status_name,last_error,output_fits,output_fits_size}
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>

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
    struct stat st{};
    return (::stat(p.c_str(), &st) == 0) ? (long)st.st_size : -1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <hips_dir> <output_dir>\n", argv[0]);
        return 64;
    }
    const std::string hips_dir = argv[1];
    const std::string out_dir = argv[2];

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
        "\",\"center\":{\"ra_deg\":10.0,\"dec_deg\":0.0},\"width_px\":64,"
        "\"height_px\":64,\"scale_deg_per_px\":0.01,\"sampler\":\"bilinear\","
        "\"output_mode\":\"surface_brightness\"}";

    acsd_handle h = nullptr;
    acsd_status st = p3_session_create(&host, &h);
    if (st != ACS_OK) {
        std::printf("{\"status\":%d,\"status_name\":\"%s\",\"last_error\":\"p3_session_create failed\"}\n",
                    (int)st, status_name(st));
        return 1;
    }
    acsd_span_u8 span{};
    span.head.struct_size = (uint32_t)sizeof(acsd_span_u8);
    span.head.abi_version = ACS_ABI_VERSION_V1;
    span.data = reinterpret_cast<uint8_t*>(const_cast<char*>(req.data()));
    span.count = (uint64_t)req.size();
    st = p3_session_run(h, span);
    const std::string err = acsd::phase3::last_error(h);
    p3_session_destroy(h);

    const std::string fits = out_dir + "/output_phase3.fits";
    std::printf("{\"status\":%d,\"status_name\":\"%s\",\"last_error\":\"%s\","
                "\"output_fits\":\"%s\",\"output_fits_size\":%ld}\n",
                (int)st, status_name(st), json_escape(err).c_str(),
                json_escape(fits).c_str(), file_size(fits));
    return (st == ACS_OK) ? 0 : 1;
}
