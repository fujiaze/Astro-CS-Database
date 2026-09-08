// tests/unit/mon002_alloc_test.cpp — MON-002 (V7 04_CPU_RESOURCE_TASKS) 内存泄露/增长检测单测
// 规格面: 长 run 定期采样 RSS/private/commit/allocator outstanding; 区分 cache/
// 高水位/工作集; run 结束验证可解释回落; 保存原始曲线(alloc_samples.csv/
// alloc_report.json)。验收: 注入 leak 失败 / 重复小 run 无单调增长 / cache 上限
// 生效 / 禁止只看峰值截图。
// 哨兵纪律(批次 P p2007 先例, R13 登记): 采样失败显式(-1/哨兵行非合法值),
// 测试侧自洽校验 —— 未采样不冒充 0 回落/低占用证据。
// 负向样例: 注入 leak → AllocGrowthUnbounded; 面在零有效样本(伪造 monitor/全
// 哨兵) → FAIL; 回落不可解释 → AllocReclaimMissing; 报告/曲线格式篡改 → 拒绝。
// 判定用合成曲线(observe 注入 t, 不真实睡眠); 真实 /proc 探针面以自检样例覆盖。
#include "memory_report.h"
#include "resource_gate.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      ++failures;                                                         \
    }                                                                     \
  } while (0)

namespace {

astrocs::ProcSample sample_rss(uint64_t rss) {
    astrocs::ProcSample s;
    s.rss_bytes = rss;
    s.vms_bytes = rss + (64ull << 20);   // VM commit = RSS + 64MiB 保留
    s.pss_bytes = rss;
    return s;
}

// 稳定曲线: 首轮 200MiB + 每轮 ±4MiB 交替噪声(峰谷往返, 非单调)。
void feed_stable(astrocs::AllocationRecorder& rec) {
    for (int i = 0; i < 30; ++i)
        rec.observe(i * 0.5, sample_rss((200ull << 20) + (i % 2 ? 4ull << 20 : 0)));
}

// 注入 leak: 每轮 +3MiB 单调增长(30 轮 0.5s = 15s, 斜率 6 MiB/s > warn)。
void feed_leak(astrocs::AllocationRecorder& rec) {
    for (int i = 0; i < 30; ++i)
        rec.observe(i * 0.5, sample_rss((200ull << 20) + static_cast<uint64_t>(i) * (3ull << 20)));
}

// 快速 leak(30 轮 +20MiB/轮 = 40 MiB/s ≥ 32 失败线)。
void feed_leak_fast(astrocs::AllocationRecorder& rec) {
    for (int i = 0; i < 30; ++i)
        rec.observe(i * 0.5, sample_rss((200ull << 20) + static_cast<uint64_t>(i) * (20ull << 20)));
}

// 长 run 回落: 涨到 500MiB(峰值出现区间 t=19..22.5s, 全部在收尾窗外),
// t>=23s 起释放到 150MiB(收尾工作集; 回落比例 0.7 >= 阈值 → Reclaimed)。
void feed_reclaim(astrocs::AllocationRecorder& rec) {
    for (int i = 0; i <= 50; ++i) {
        const double t = i * 0.5;
        uint64_t rss = (100ull << 20) + (i > 8 ? static_cast<uint64_t>(i - 8) * (15ull << 20) : 0);
        if (rss > 500ull << 20) rss = 500ull << 20;
        if (t >= 23.0) rss = 150ull << 20;
        rec.observe(t, sample_rss(rss));
    }
}

// 长 run 不回落: 同形曲线(峰值出现区间 t=19..22.5s, 全在收尾窗外), t>=23s 起
// 收尾留 460MiB — retained 40MiB 超容差(32MiB)且回落比例 0.08 < 0.5 →
// UnexplainedResidual(不可解释残留)。
void feed_no_reclaim(astrocs::AllocationRecorder& rec) {
    for (int i = 0; i <= 50; ++i) {
        const double t = i * 0.5;
        uint64_t rss = (100ull << 20) + (i > 8 ? static_cast<uint64_t>(i - 8) * (15ull << 20) : 0);
        if (rss > 500ull << 20) rss = 500ull << 20;
        if (t >= 23.0) rss = 460ull << 20;
        rec.observe(t, sample_rss(rss));
    }
}

}  // namespace

int main() {
    using astrocs::AllocGrowthVerdict;
    using astrocs::AllocReclaimVerdict;
    using astrocs::GateDiag;

    // 1) 稳定曲线: Stable; 峰谷噪声不误判(验收: 重复小 run 无单调增长)。
    {
        astrocs::AllocationRecorder rec;
        feed_stable(rec);
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.growth_verdict == AllocGrowthVerdict::Stable);
        CHECK(r.peak_rss_bytes == (204ull << 20));
        CHECK(r.last_rss_bytes == (204ull << 20));  // 尾点 i=29(奇)→ 噪声高位
        CHECK(r.n_samples == 30);
        CHECK(r.n_sentinel == 0);
    }
    // 2) 注入 leak(慢速): Growing(预警, 未到失败线)。
    {
        astrocs::AllocationRecorder rec;
        feed_leak(rec);
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.growth_verdict == AllocGrowthVerdict::Growing);
        CHECK(r.rss_growth_mb_per_s > astrocs::kAllocGrowthWarnMbPerS);
    }
    // 3) 注入 leak(快速, 负向): Unbounded → gate AllocGrowthUnbounded FAIL
    //    (验收: 注入 leak 失败; 整条曲线判定, 非峰值)。
    {
        astrocs::AllocationRecorder rec;
        feed_leak_fast(rec);
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.growth_verdict == AllocGrowthVerdict::Unbounded);
        astrocs::GateConfig g;
        g.alloc_report_present = true;
        g.alloc_samples_measured = static_cast<double>(r.n_samples);
        g.alloc_growth_mb_per_s = r.rss_growth_mb_per_s;
        g.alloc_reclaim_verdict = r.reclaim_verdict;
        CHECK(astrocs::evaluate_mon002(g) == GateDiag::AllocGrowthUnbounded);
    }
    // 4) 峰值高但曲线平稳(假峰): 不得凭峰值判增长 —— 禁止只看峰值验收。
    {
        astrocs::AllocationRecorder rec;
        rec.observe(0.0, sample_rss(200ull << 20));
        rec.observe(0.5, sample_rss(600ull << 20));  // 单点假峰(截图式峰值)
        rec.observe(1.0, sample_rss(200ull << 20));
        rec.observe(1.5, sample_rss(200ull << 20));
        rec.observe(2.0, sample_rss(200ull << 20));
        rec.observe(2.5, sample_rss(200ull << 20));
        rec.observe(3.0, sample_rss(200ull << 20));
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.peak_rss_bytes == (600ull << 20));          // 峰值如实呈现
        CHECK(r.growth_verdict == AllocGrowthVerdict::Stable);  // 判定不看峰值
    }
    // 5) 长 run 可解释回落: Reclaimed(验收: run 结束验证可解释回落)。
    {
        astrocs::AllocationRecorder rec;
        feed_reclaim(rec);
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.reclaim_verdict == AllocReclaimVerdict::Reclaimed);
        CHECK(r.reclaim_frac >= astrocs::kAllocMinReclaimFrac);
    }
    // 6) 长 run 回落不可解释(负向): UnexplainedResidual → gate AllocReclaimMissing。
    {
        astrocs::AllocationRecorder rec;
        feed_no_reclaim(rec);
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.reclaim_verdict == AllocReclaimVerdict::UnexplainedResidual);
        astrocs::GateConfig g;
        g.alloc_report_present = true;
        g.alloc_samples_measured = static_cast<double>(r.n_samples);
        g.alloc_growth_mb_per_s = r.rss_growth_mb_per_s;
        g.alloc_reclaim_verdict = r.reclaim_verdict;
        CHECK(astrocs::evaluate_mon002(g) == GateDiag::AllocReclaimMissing);
    }
    // 7) 采样失败哨兵(负向+哨兵纪律): 全哨兵曲线 → 判定双 InsufficientSamples,
    //    reclaim_frac=-1(未采样非 0 回落); gate 面在零有效样本 → FAIL。
    {
        astrocs::AllocationRecorder rec;
        for (int i = 0; i < 8; ++i) rec.observe(i * 0.5, sample_rss(0));  // rss=0 采样失败
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.n_curve == 8);
        CHECK(r.n_samples == 0);
        CHECK(r.n_sentinel == 8);
        CHECK(r.growth_verdict == AllocGrowthVerdict::InsufficientSamples);
        CHECK(r.reclaim_verdict == AllocReclaimVerdict::InsufficientSamples);
        CHECK(r.reclaim_frac < 0.0);   // -1 哨兵, 不得冒充 0 回落
        astrocs::GateConfig g;
        g.alloc_report_present = true;
        g.alloc_samples_measured = static_cast<double>(r.n_samples);  // 0.0
        g.alloc_growth_mb_per_s = astrocs::kMon001NotSampled;
        CHECK(astrocs::evaluate_mon002(g) == GateDiag::AllocReclaimMissing);
    }
    // 8) 部分哨兵混入: 哨兵行不入统计但入曲线留证; 有效样本仍可判定。
    {
        astrocs::AllocationRecorder rec;
        rec.observe(0.0, sample_rss(200ull << 20));
        rec.observe(0.5, sample_rss(0));             // 哨兵
        rec.observe(1.0, sample_rss(200ull << 20));
        rec.observe(1.5, sample_rss(0));             // 哨兵
        rec.observe(2.0, sample_rss(204ull << 20));
        rec.observe(2.5, sample_rss(200ull << 20));
        rec.observe(3.0, sample_rss(202ull << 20));
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.n_curve == 7);
        CHECK(r.n_samples == 5);
        CHECK(r.n_sentinel == 2);
        CHECK(r.growth_verdict == AllocGrowthVerdict::Stable);
    }
    // 9) 短 run 显式跳过回落判定(非静默): wall<10s → ShortRunSkipped(非 FAIL)。
    {
        astrocs::AllocationRecorder rec;
        for (int i = 0; i < 8; ++i) rec.observe(i * 0.5, sample_rss(200ull << 20));
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.reclaim_verdict == AllocReclaimVerdict::ShortRunSkipped);
        astrocs::GateConfig g;
        g.alloc_report_present = true;
        g.alloc_samples_measured = static_cast<double>(r.n_samples);
        g.alloc_growth_mb_per_s = r.rss_growth_mb_per_s;
        g.alloc_reclaim_verdict = r.reclaim_verdict;
        CHECK(astrocs::evaluate_mon002(g) == GateDiag::Ok);
    }
    // 10) gate 兼容: report 面未接入(向后兼容)与样本不足(斜率未算)不阻塞。
    {
        astrocs::GateConfig g;
        CHECK(astrocs::evaluate_mon002(g) == GateDiag::Ok);   // 面未接入
        g.alloc_report_present = true;
        g.alloc_samples_measured = 6.0;
        g.alloc_growth_mb_per_s = astrocs::kMon001NotSampled;  // 样本不足未算斜率
        g.alloc_reclaim_verdict = AllocReclaimVerdict::ShortRunSkipped;
        CHECK(astrocs::evaluate_mon002(g) == GateDiag::Ok);
    }
    // 11) cache/commit 口径与 allocator/private 探针可用性: cache=commit-RSS;
    //     本进程真实 /proc 采样(private 可得/glibc outstanding>0)。
    {
        astrocs::AllocationRecorder rec;
        rec.observe(0.0, sample_rss(100ull << 20));
        rec.observe(0.5, sample_rss(100ull << 20));
        rec.observe(1.0, sample_rss(100ull << 20));
        rec.finalize();
        const auto& r = rec.report();
        CHECK(r.peak_cache_bytes == (64ull << 20));    // commit(164M) - RSS(100M)
        CHECK(r.private_probe_available);              // Linux /proc/self/smaps_rollup
        CHECK(r.allocator_probe_available);            // glibc mallinfo2
        CHECK(r.peak_alloc_outstanding_bytes > 0);     // 进程必有在用分配
    }
    // 12) 产物落盘+格式篡改拒绝(负向): 重算摘要与报告不一致 → validate false。
    {
        const std::string dir = "run/local/agent_mon002/tmp_valid";
        std::filesystem::create_directories(dir);   // write 前建目录
        astrocs::AllocationRecorder rec;
        feed_stable(rec);
        rec.finalize();
        CHECK(rec.write_all(dir));
        CHECK(astrocs::validate_alloc_report(dir));    // 未篡改 → 一致
        // 篡改 1: 截断曲线行(n_curve 不符)
        {
            std::FILE* f = std::fopen((dir + "/alloc_samples.csv").c_str(), "r");
            CHECK(f != nullptr);
            std::string all; char buf[512];
            while (f && std::fgets(buf, sizeof(buf), f)) all += buf;
            if (f) std::fclose(f);
            const auto cut = all.find_last_of('\n', all.size() - 2);
            CHECK(cut != std::string::npos);
            f = std::fopen((dir + "/alloc_samples.csv").c_str(), "w");
            if (f && cut != std::string::npos) { std::fwrite(all.c_str(), 1, cut + 1, f); std::fclose(f); }
            CHECK(!astrocs::validate_alloc_report(dir));   // 曲线篡改 → 拒绝
        }
        // 篡改 2: 伪造峰值字段(peak_rss_bytes 改大)
        rec.write_all(dir);
        {
            std::FILE* f = std::fopen((dir + "/alloc_report.json").c_str(), "r");
            CHECK(f != nullptr);
            std::string all; char buf[512];
            while (f && std::fgets(buf, sizeof(buf), f)) all += buf;
            if (f) std::fclose(f);
            const auto pos = all.find("\"peak_rss_bytes\":");
            CHECK(pos != std::string::npos);
            const char* rest = all.c_str() + pos + 20;   // 跳过 "peak_rss_bytes":NN,
            f = std::fopen((dir + "/alloc_report.json").c_str(), "w");
            if (f) {
                std::fwrite(all.c_str(), 1, pos, f);
                std::fprintf(f, "\"peak_rss_bytes\":999999999999,");
                std::fwrite(rest, 1, all.size() - (pos + 20), f);
                std::fclose(f);
            }
            CHECK(!astrocs::validate_alloc_report(dir));   // 字段篡改 → 拒绝
        }
        // 篡改 3: 哨兵行冒充合法样本(第 2 行 sampled 0→1, 按行精确改写)
        {
            astrocs::AllocationRecorder rec2;
            rec2.observe(0.0, sample_rss(200ull << 20));
            rec2.observe(0.5, sample_rss(0));   // 哨兵
            rec2.finalize();
            CHECK(rec2.write_all(dir));
            std::FILE* f = std::fopen((dir + "/alloc_samples.csv").c_str(), "r");
            CHECK(f != nullptr);
            std::vector<std::string> lines;
            char buf[512];
            while (f && std::fgets(buf, sizeof(buf), f)) lines.push_back(buf);
            if (f) std::fclose(f);
            CHECK(lines.size() == 3);
            const auto p = lines[2].find(",0,");   // 行 2 = 哨兵行
            CHECK(p != std::string::npos);
            if (p != std::string::npos) lines[2][p + 1] = '1';   // 哨兵改合法
            f = std::fopen((dir + "/alloc_samples.csv").c_str(), "w");
            if (f) {
                for (const auto& l : lines) std::fwrite(l.c_str(), 1, l.size(), f);
                std::fclose(f);
            }
            CHECK(!astrocs::validate_alloc_report(dir));   // 哨兵计数不符 → 拒绝
        }
    }

    if (failures == 0) {
        std::printf("MON-002 alloc report TESTS PASS (12 组: 稳定/注入leak预警/leak失败/"
                    "峰值不算判定/回落/reclaim失败/全哨兵/部分哨兵/短run跳过/兼容/"
                    "cache探针/篡改拒绝)\n");
        return 0;
    }
    std::printf("MON-002 alloc report TESTS FAILED (%d)\n", failures);
    return 1;
}
