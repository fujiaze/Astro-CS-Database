// P1-SESSION-TEST · performance 组: Phase1 小链装配哨兵 (CI 森严上界)
//
// 控制包任务: P1-SESSION-TEST (SA-P1SS-T, queue 47, lock-P1-SESSION)。
// 模板覆盖面: performance 哨兵 — session 是装配层, 无内核算法数值基线;
// 哨兵锚定为装配开销上界 (宽松, 防 CI 森严脆弱) + 1-N worker 完成性:
//   W1  8 帧 32x32 全链 (无 master) run 上界 20s (装配层只做 IO+委托,
//       现状基线 <1s; 20s 哨兵只拦截装配异常退化, 不拦截机器慢)
//   W2  budget.max_workers=1/4 两次 run 均 complete (预算注入路径alive)
// 计时来源 std::chrono (单调), 只计 p1_session_run, fixture 生成不计。
#include "p1_session.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "p1sess_fixtures.hpp"
#include "p1sess_test_main.hpp"

using json = nlohmann::json;

namespace p1sess {

extern CheckState g_cs;

static void* perf_alloc(void*, std::uint64_t n, std::uint64_t) {
    return std::malloc(static_cast<std::size_t>(n));
}
static void perf_free(void*, void* p) { std::free(p); }
static int perf_not_cancelled(void*) { return 0; }

static astrocs_host_services_v1 perf_make_host(std::uint32_t workers) {
    astrocs_host_services_v1 h{};
    h.struct_size = sizeof(astrocs_host_services_v1);
    h.abi_version = ACS_ABI_VERSION_V1;
    h.allocator = {sizeof(acs_allocator), ACS_ABI_VERSION_V1, perf_alloc, perf_free, nullptr};
    h.logger = {sizeof(acs_logger), ACS_ABI_VERSION_V1, nullptr, nullptr};
    h.cancel = {sizeof(acs_cancel), ACS_ABI_VERSION_V1, perf_not_cancelled, nullptr};
    h.budget = {sizeof(acs_thread_budget), ACS_ABI_VERSION_V1, workers, workers,
                nullptr, nullptr, nullptr};
    return h;
}

int run_performance() {
    CheckState& cs = g_cs;
    namespace fs = std::filesystem;
    const char* d = std::getenv("TMPDIR");
    const char* w = std::getenv("TEMP");
    const char* w2 = std::getenv("TMP");
    const std::string tmp_root = (d ? d : (w ? w : (w2 ? w2 : ".")));
    const std::string base =
        (fs::path(tmp_root) / "astrocs_p1sess_perf").generic_string();
    const std::string out_dir = base + "/out";
    std::error_code ec;
    fs::remove_all(base, ec);
    fs::create_directories(out_dir, ec);

    // 8 帧 32x32 常数场 (FIX-SESS-B 谱系, seed 派生)
    constexpr int kFrames = 8;
    std::vector<std::string> lights;
    for (int j = 0; j < kFrames; ++j) {
        ConstField f{0x5E55ED0F00ULL + static_cast<std::uint64_t>(j),
                     100.0f + static_cast<float>(j)};
        const std::string p = base + "/light" + std::to_string(j) + ".fts";
        if (write_fits_file(p, 32, 32, const_field_pixel, &f)) return 2;
        lights.push_back(p);
    }
    std::string arr;
    for (std::size_t j = 0; j < lights.size(); ++j) {
        if (j) arr += ",";
        arr += "\"" + lights[j] + "\"";
    }
    const std::string cfg =
        std::string("{\"input_lights\":[") + arr + "],\"output_dir\":\"" + out_dir +
        "\",\"cosmetic\":{\"enabled\":false}}";

    double seconds_1w = -1.0;
    for (const std::uint32_t workers : {1u, 4u}) {
        astrocs_host_services_v1 host = perf_make_host(workers);
        acs_handle h = nullptr;
        P1SESS_CHECK_EQ(cs, p1_session_create(&host, &h), ACS_OK);
        acs_span_u8 sp = ACS_SPAN_U8(reinterpret_cast<std::uint8_t*>(const_cast<char*>(cfg.c_str())),
                                     static_cast<std::uint64_t>(cfg.size()));
        const auto t0 = std::chrono::steady_clock::now();
        const acs_status rc = p1_session_run(h, sp, 0);
        const auto t1 = std::chrono::steady_clock::now();
        const double sec =
            std::chrono::duration_cast<std::chrono::duration<double>>(t1 - t0).count();
        P1SESS_CHECK_MSG(cs, rc == ACS_OK, "w1_run_ok", "workers=%u rc=%d", workers,
                         static_cast<int>(rc));
        acs_span_u8 mf{};
        P1SESS_CHECK_EQ(cs, p1_session_inspect(h, &mf), ACS_OK);
        json m = json::parse(std::string(reinterpret_cast<const char*>(mf.data), mf.count));
        host.allocator.free(host.allocator.user_data, mf.data);
        P1SESS_CHECK(cs, m.value("status", "") == "partial", "w1_partial");
        P1SESS_CHECK_EQ(cs, m.value("frames", 0), kFrames);
        // W1 哨兵上界 (宽松; 只拦装配异常退化)
        P1SESS_CHECK_MSG(cs, sec < 20.0, "w1_upper_bound", "workers=%u run took %.2fs", workers, sec);
        if (workers == 1u) seconds_1w = sec;
        P1SESS_CHECK_EQ(cs, p1_session_destroy(h), ACS_OK);
        std::fprintf(stdout, "[p1sess] perf workers=%u: %.3fs (sentinel <20s)\n", workers, sec);
    }
    (void)seconds_1w;

    if (cs.failures == 0) return 0;
    std::fprintf(stderr, "performance group: %d failure(s)\n", cs.failures);
    return 1;
}

}  // namespace p1sess
