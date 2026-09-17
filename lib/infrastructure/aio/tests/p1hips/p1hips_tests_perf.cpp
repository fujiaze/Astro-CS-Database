// P1-HIPS-TEST · performance 基线哨兵 (独立可执行, 时长敏感与 core 组隔离)
//
// CI 森严口径 (对齐 p1cos_tests_perf.cpp / p1drz_tests_perf.cpp): 门必须
// 稳, 不做绝对耗时回归 (共享负载环境抖动大), 三重哨兵:
//   perf_parity : 单 tile 全产品集 (signal+support+variance+ivar+snr+finalize)
//                 T(2 worker 并行进程)/T(1) < 4.0× (并行不倒退; writer 单句柄
//                 串行合同 → worker 级并行, 不共享句柄/目录)
//   perf_trend  : T(4)/T(1) ≥ 0.25× (无负加速趋势, 共享负载容噪)
//
// 测量协议 (前台裁决 2026-09-17「改测量协议, 不改判据阈值」; 三处判据值不变):
//   warmup : 每个 worker 数先各跑 1 次**不计时** (页缓存/分配器/线程栈预热)
//   交错   : 同一 rep 内按 1→2→4 顺序连测, 使三者经历**同一负载窗口**
//            (旧协议对每个 worker 数各自连测 3 次, 分母 t1w 一旦被瞬时争用
//             抬高, parity4=t4w/t1w 就假红: 实测 0.43 / 0.24 红 / 0.27)
//   N=5    : 5 个 rep 取**中位数** (旧协议 3 次取 min, 对单次抖动无抵抗;
//            中位数 + 交错把"瞬时负载"从比值里对消掉, 判别力不变 ——
//            真退化会同时抬高 t4w 或压低 t1w 的**中位数**, 仍必判红)
//   独占   : 测量前打印 /proc/loadavg, 供红灯归因 (非判据; 不改阈值)
// HIPS_WRITER.md §9: "性能/资源: 单 tile 写耗与内存水位 smoke 记录 (非冻结
// 容差, 登记即可)" — 哨兵登记到 stdout, 不设绝对阈值。
#include "p1hips_test_main.hpp"
#include "p1hips_fixtures.hpp"
#include "p1hips_oracle.hpp"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include "aio_hips.h"

using namespace p1hips;
using namespace p1hips::oracle;

namespace {

CheckState g_perf_cs;

// 单 worker: 写完整 4-tile 产品 (F1×2 + F3 边界 + F5 半方差) — I/O 主导负载
void perf_write_worker(const std::string& dir) {
    AioHipsProductSet* ps = aio_hips_product_begin(
        dir.c_str(), FIX_NSIDE, 512, AIO_HIPS_FLOAT64, AIO_HIPS_PRODUCT_ALL_V19,
        "ivo://astrocs/test/p1hips", "perf", nullptr, 100.0,
        "2026-09-07T00:00:00", 0);
    if (!ps) return;
    FixViewF64 t0 = fix_hips_a_tile(0, 10.0, 0.5, 1.5, true, true);
    FixViewF64 t1 = fix_hips_c_edge_tile(1, 20.0, 0.5);
    FixViewF64 t2 = fix_hips_a_tile(2, 10.0, 0.5, 1.5, false, true);
    FixViewF64 t3 = fix_hips_e_half_var_tile(3, 10.0, 0.5, 1.0);
    aio_hips_write_signal_support_tile(ps, &t0.view);
    aio_hips_write_signal_support_tile(ps, &t1.view);
    aio_hips_write_signal_support_tile(ps, &t2.view);
    aio_hips_write_signal_support_tile(ps, &t3.view);
    aio_hips_write_variance_tile(ps, &t0.view);
    aio_hips_write_variance_tile(ps, &t2.view);
    std::vector<FixSnrPointF> pts = fix_hips_d_snr_points(20260907u, 256);
    aio_hips_write_snr_points(ps, pts.data(), (int)pts.size());
    aio_hips_finalize(ps);
}

// 单次计时: workers 个独立 worker 各写一个 mkdtemp 产品集; 返回 per-worker 秒
double time_once(int workers) {
    std::vector<std::string> dirs;
    std::vector<std::thread> ths;
    for (int i = 0; i < workers; ++i) dirs.push_back(make_tmp_dir("perf"));
    const auto t0 = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < workers; ++i)
        ths.emplace_back(perf_write_worker, dirs[(std::size_t)i]);
    for (auto& t : ths) t.join();
    const auto t1 = std::chrono::high_resolution_clock::now();
    const double sec = std::chrono::duration<double>(t1 - t0).count();
    return sec / workers;   // 归一 per-worker
}

double median_of(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n == 0) return 1e30;
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

struct PerfTimes {
    double t1, t2, t4;
};

// 预热 + 交错 + N=5 中位数 (协议见文件头注释; 判据阈值不变)
PerfTimes measure_interleaved(int reps) {
    time_once(1);   // warmup (不计时)
    time_once(2);
    time_once(4);
    std::vector<double> s1, s2, s4;
    for (int rep = 0; rep < reps; ++rep) {
        s1.push_back(time_once(1));
        s2.push_back(time_once(2));
        s4.push_back(time_once(4));
    }
    PerfTimes out;
    out.t1 = median_of(s1);
    out.t2 = median_of(s2);
    out.t4 = median_of(s4);
    return out;
}

void print_loadavg(const char* when) {
#if defined(__linux__)
    FILE* f = fopen("/proc/loadavg", "r");
    if (!f) return;
    char buf[128] = {0};
    if (fgets(buf, sizeof(buf), f)) {
        for (char* p = buf; *p; ++p)
            if (*p == '\n') { *p = 0; break; }
        std::fprintf(stdout, "[p1hips] loadavg(%s)=%s\n", when, buf);
    }
    fclose(f);
#else
    (void)when;
#endif
}

}  // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    CheckState& cs = g_perf_cs;
    cs.failures = 0;
    cs.fault_reported = false;

    print_loadavg("before");
    const PerfTimes tm = measure_interleaved(5);   // warmup + 交错 + 5 次中位数
    print_loadavg("after");
    const double t1w = tm.t1;
    const double t2w = tm.t2;
    const double t4w = tm.t4;
    P1HIPS_CHECK(cs, t1w > 0.0, "perf_baseline");

    const double parity2 = (t1w > 0.0) ? t2w / t1w : 1e30;
    const double parity4 = (t1w > 0.0) ? t4w / t1w : 1e30;
    std::fprintf(stdout,
                 "[p1hips] perf(协议=warmup+交错+5次中位数): t1w=%.4fs t2w=%.4fs t4w=%.4fs "
                 "parity2=%.2fx parity4=%.2fx\n",
                 t1w, t2w, t4w, parity2, parity4);
    P1HIPS_CHECK(cs, parity2 < 4.0, "perf_parity_2w");
    P1HIPS_CHECK(cs, parity4 < 4.0, "perf_parity_4w");
    P1HIPS_CHECK(cs, parity4 >= 0.25, "perf_trend_4w");

    if (cs.failures == 0) {
        std::fprintf(stdout, "P1HIPS PERF PASS (parity2=%.2fx parity4=%.2fx)\n",
                     parity2, parity4);
        return 0;
    }
    std::fprintf(stderr, "P1HIPS PERF FAIL (%d check(s))\n", cs.failures);
    return 1;
}
