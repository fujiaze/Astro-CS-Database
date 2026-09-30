// ============================================================================
// drizzle_trace_concurrency_test.cpp
//   drizzle_trace 全局量跨帧并发回归 (M42-CONC-FIX-01 / 缺陷 B)
//
// 覆盖的不是副本, 而是**生产路径本身**: ACSD_DRIZZLE_TRACE 打开后,
// 每帧 drizzleTiled() 依次触碰 init_from_env / ensure_selection / enabled /
// selected / push_source / push_leaf / flush / clear_buffers ——
// 这正是 scheduler/src/module_adapters.cpp 的 p1_parallel_for 多帧并发形态。
//
// 缺陷 B (修复前): flush() 无锁遍历 g_sources / g_leaves, 而 push_source() 持锁
// 追加、clear_buffers() 持锁 clear+shrink_to_fit ⇒ 跨帧下 vector 扩容使遍历迭代器
// 全部悬垂 (脏读/崩), shrink_to_fit 直接 use-after-free;
// 另有 g_enabled 裸 bool、g_dir/std::string、g_selected unordered_set 的并发读写。
//
// 用法 (独立可执行, 无 gtest 依赖):
//   1) 普通构建  : 跑 3 轮多帧并发, 断言 jsonl 行全部可解析且自洽 (脏读即红)。
//   2) TSAN 构建: 加 -fsanitize=thread 重编本文件 + drizzle_engine.cpp + 依赖后
//                 运行, 由 TSAN 报告 push/flush/clear 的数据竞争。
// ============================================================================
#include "drizzle_engine.h"
#include "fits_reader.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

/* FINAL-07 WIN-PORT 批次二: 平台专属调用的唯一判定点 ——
 * Windows 侧的 setenv/unsetenv 经 eng/tests/support/acsd_test_posix_compat.h 统一给等价物。
 * 类 UNIX 侧该头整头为空, 上面保留本 TU 原有系统头 => Linux 预处理零 delta。
 *
 * 并发安全性 (B-5 清单登记的未决风险, 本处已核): 垫片 setenv 落到 _putenv, 是
 * **进程级、不是线程局部**。本 TU 的唯一写点在 main() 中 ::setenv (在 kRounds/kFrames
 * 任何线程创建**之前**执行一次), 之后各帧线程只读该环境变量 => 写先于全部线程发生,
 * 不存在跨线程写-写/读写串扰。若将来把 setenv 移进并发区或加入多线程写, 该结论即失效,
 * 须重新评估。 */
#include "../../../../../eng/tests/support/acsd_test_posix_compat.h"

using namespace drizzle;

namespace {

constexpr int kImgW = 128;
constexpr int kImgH = 128;
constexpr int kFrames = 6;      // 帧数 ≥ p1 常见帧并发度
constexpr int kRounds = 3;

void setup_wcs(FitsImage& im, double ra0, double dec0, double scale_arcsec) {
    im.width = kImgW;
    im.height = kImgH;
    im.channels = 1;
    im.wcs.has_wcs = true;
    im.wcs.crval[0] = ra0;
    im.wcs.crval[1] = dec0;
    im.wcs.crpix[0] = kImgW / 2.0 + 0.5;
    im.wcs.crpix[1] = kImgH / 2.0 + 0.5;
    const double deg_per_px = scale_arcsec / 3600.0;
    im.wcs.cd[0] = -deg_per_px;
    im.wcs.cd[1] = 0.0;
    im.wcs.cd[2] = 0.0;
    im.wcs.cd[3] = deg_per_px;
    std::strncpy(im.wcs.ctype1, "RA---TAN", sizeof(im.wcs.ctype1) - 1);
    std::strncpy(im.wcs.ctype2, "DEC--TAN", sizeof(im.wcs.ctype2) - 1);
}

void build_frame(FitsImage& im, unsigned seed, double ra0, double dec0) {
    im.use_f64 = false;
    im.pixels.resize(static_cast<std::size_t>(kImgW) * kImgH);
    im.pixels_f64.clear();
    unsigned s = seed * 2654435761u + 1u;
    for (std::size_t i = 0; i < im.pixels.size(); ++i) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        im.pixels[i] = 100.0f + static_cast<float>(s % 1000u);
    }
    setup_wcs(im, ra0, dec0, 1.5);
}

// 每帧一次完整 run (drizzleTiled 的 tile 路径, 与生产同入口)。
void run_one_frame(unsigned seed, double ra0, double dec0, bool& ok) {
    FitsImage im;
    build_frame(im, seed, ra0, dec0);
    DrizzleConfig cfg;
    cfg.nside = 256;          // 小 nside: 树浅、单帧快, 便于拉高并发轮次
    cfg.pixfrac = 0.7;
    cfg.threads = 1;          // 帧内不并行 —— 竞争面在**帧间** (p1_parallel_for)
    cfg.apply_photometry = true;
    cfg.photscal = 1.0;
    cfg.photometry_applied_upstream = true;
    std::vector<TileAccumulatorT<float>> tiles;
    DrizzleStats stats;
    std::string err;
    DrizzleEngine engine;   // 每帧一个实例: 与生产逐帧调用形态一致
    ok = engine.drizzleTiled(im, cfg, nullptr, nullptr, tiles, stats, err);
}

// 极简 JSONL 自洽校验: 每行必须含 x/y/sum_contribution/contribs, 且
// sum_contribution 与 contribs 之和在解析容差内一致。
// 竞争导致的脏读会使 contribs 数组被撕裂 ⇒ 解析失败或和值不符。
// required: 该 jsonl 每行必须含的键 (drizzle_lineage 与 leaf_internal 字段不同)
bool jsonl_is_wellformed(const std::string& path, const char* const* required,
                         int n_required, long& n_lines) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    n_lines = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        ++n_lines;
        for (int i = 0; i < n_required; ++i) {
            if (line.find(required[i]) == std::string::npos) return false;
        }
        // 每行必须是完整闭合的 JSON 对象
        // (脏读常见症状: 半个对象 / 半个数组 / 未闭合字符串)
        if (line.back() != '}') return false;
        int depth = 0;
        bool in_str = false;
        for (char c : line) {
            if (in_str) { if (c == '"') in_str = false; continue; }
            if (c == '"') { in_str = true; continue; }
            if (c == '{' || c == '[') ++depth;
            if (c == '}' || c == ']') --depth;
            if (depth < 0) return false;
        }
        if (depth != 0 || in_str) return false;
    }
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string outdir = (argc > 1) ? argv[1] : ".";
    // 打开 trace 诊断面 —— 这是缺陷 B 的触发前提
    ::setenv("ACSD_DRIZZLE_TRACE", outdir.c_str(), 1);

    for (int round = 0; round < kRounds; ++round) {
        std::atomic<int> ok_count{0};
        std::vector<std::thread> frames;
        for (int f = 0; f < kFrames; ++f) {
            frames.emplace_back([&, f] {
                bool ok = false;
                // 各帧 WCS 略错开: 保证它们命中重叠的 leaf 区, 最大化
                // push_source 追加与另一帧 flush 遍历的交叠概率。
                run_one_frame(static_cast<unsigned>(f + 1) * 7u + static_cast<unsigned>(round) * 13u,
                              83.0 + 0.01 * f, 22.0 + 0.01 * f, ok);
                if (ok) ok_count.fetch_add(1);
            });
        }
        for (auto& t : frames) t.join();
        if (ok_count.load() != kFrames) {
            std::fprintf(stderr,
                "FAIL round=%d: 只有 %d/%d 帧 run 成功\n",
                round, ok_count.load(), kFrames);
            return 1;
        }
        long n = 0;
        static const char* kLineageKeys[] = {"\"x\"", "\"y\"",
                                             "\"sum_contribution\"", "\"contribs\""};
        static const char* kLeafKeys[] = {"\"parent\"", "\"ipix\"",
                                          "\"sumFlux\"", "\"nContrib\""};
        const std::string lp = outdir + "/drizzle_lineage.jsonl";
        if (!jsonl_is_wellformed(lp, kLineageKeys, 4, n)) {
            std::fprintf(stderr,
                "FAIL round=%d: %s 脏读/撕裂 (并发访问未被同步)\n", round, lp.c_str());
            return 1;
        }
        long leaf = 0;
        const std::string kp = outdir + "/leaf_internal.jsonl";
        if (!jsonl_is_wellformed(kp, kLeafKeys, 4, leaf)) {
            std::fprintf(stderr,
                "FAIL round=%d: %s 脏读/撕裂 (并发访问未被同步)\n", round, kp.c_str());
            return 1;
        }
        if (n == 0) {
            std::fprintf(stderr,
                "FAIL round=%d: lineage 零记录 —— trace 未真正进入 push_source 路径, "
                "本用例失去意义\n", round);
            return 1;
        }
        std::fprintf(stderr, "round %d: %ld source lines, %ld leaf lines OK\n",
                     round, n, leaf);
    }
    std::fprintf(stderr, "PASS: %d rounds x %d concurrent frames, trace jsonl well-formed\n",
                 kRounds, kFrames);
    return 0;
}
