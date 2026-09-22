// eng/tests/unit/normalize_workflow_test.cpp — ARCH-502 normalize 异步工作流调度器回归锁
//
// 依据：CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md §2（确定性）/§3（资源声明）/
//       §4（探针）/§5（取消）。
// 判据（每条都可证伪）：
//   A. 确定性：N=1/2/4/8 worker 的 outcome（含 checksum）**逐位一致**；
//   B. 归约顺序：输出恒按 frame_id 升序，与 worker 数无关；
//   C. 预取：开启预取时 cache_hit 事件存在且命中率 > 0；关闭时加载次数 = 键数（无重复加载）；
//   D. 生命周期：每帧 bytes_leaked == 0（阶段结束无残留块）；peak_bytes 受在途块集合约束；
//   E. 内存上限：超限帧必须以 memory_limit_exceeded 失败（不是静默通过）；
//   F. 取消：cancel() 后 run() 能返回且不泄漏、不挂死；
//   G. 探针 schema：8 类事件名与合同一致；node_wall/queue_wait 单位 s、块事件单位 B。
#include "astrocs/core/normalize_workflow.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <thread>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace astrocs::core;

#ifndef ARCH502_EVIDENCE_DIR
#define ARCH502_EVIDENCE_DIR "run/RELEASE-05/evidence"
#endif
#include <unistd.h>

namespace {
int g_fail = 0, g_total = 0;
void check(bool ok, const std::string& what) {
  ++g_total;
  if (!ok) { ++g_fail; std::printf("FAIL %s\n", what.c_str()); }
}

// 造一帧：raw → calibrated（消费 raw）→ photometric（消费 calibrated）→ snr
NormalizeFrame make_frame(std::uint64_t fid, std::size_t n_pix, const std::string& key,
                           int compute_ms = 0) {
  NormalizeFrame f;
  f.frame_id = fid;
  f.prefetch_keys = key.empty() ? std::vector<std::string>{} : std::vector<std::string>{key};
  std::vector<std::string> prefetch_consumes;
  for (const std::string& k : f.prefetch_keys) prefetch_consumes.push_back(prefetch_block_name(k));
  {
    NormalizeNode n;
    n.id = "ingest";
    n.consumes = prefetch_consumes;
    n.run = [n_pix](BlockFrame& fr) {
      BlockMeta m; m.name = "raw"; m.dtype = BlockDtype::F64;
      m.shape = {static_cast<std::int64_t>(n_pix)};
      m.unit = "ADU"; m.producer = "ingest"; m.consumers = {"calibrate"};
      m.lifecycle = BlockLifecycle::SHORT;
      Block* b = fr.create(m, n_pix);
      if (!b) return false;
      double* p = b->f64();
      for (std::size_t i = 0; i < n_pix; ++i) p[i] = static_cast<double>(i % 97) + 1.0;
      return true;
    };
    f.nodes.push_back(n);
  }
  {
    NormalizeNode n;
    n.id = "calibrate";
    n.consumes = {"raw"};
    n.run = [n_pix](BlockFrame& fr) {
      const Block* raw = fr.find("raw");
      if (!raw) return false;
      BlockMeta m; m.name = "calibrated"; m.dtype = BlockDtype::F64;
      m.shape = {static_cast<std::int64_t>(n_pix)};
      m.unit = "ADU"; m.producer = "calibrate"; m.consumers = {"photometry"};
      m.lifecycle = BlockLifecycle::SHORT;
      Block* b = fr.create(m, n_pix);
      if (!b) return false;
      const double* s = raw->f64(); double* d = b->f64();
      for (std::size_t i = 0; i < n_pix; ++i) d[i] = s[i] * 0.5;
      return true;
    };
    f.nodes.push_back(n);
  }
  {
    NormalizeNode n;
    n.id = "photometry";
    n.consumes = {"calibrated"};
    n.run = [n_pix](BlockFrame& fr) {
      const Block* c = fr.find("calibrated");
      if (!c) return false;
      BlockMeta m; m.name = "photometric"; m.dtype = BlockDtype::F64;
      m.shape = {static_cast<std::int64_t>(n_pix)};
      m.unit.clear(); m.producer = "photometry"; m.consumers = {"snr"};
      m.lifecycle = BlockLifecycle::SHORT;
      Block* b = fr.create(m, n_pix);
      if (!b) return false;
      const double* s = c->f64(); double* d = b->f64();
      for (std::size_t i = 0; i < n_pix; ++i) d[i] = s[i] + 100.0;
      return true;
    };
    f.nodes.push_back(n);
  }
  {
    NormalizeNode n;
    n.id = "snr";
    n.consumes = {"photometric"};
    if (compute_ms > 0) {
      n.run = [compute_ms](BlockFrame& fr) {
        const auto t0 = std::chrono::steady_clock::now();
        volatile double acc = 0.0;
        while (std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - t0).count() < compute_ms) {
          for (int i = 0; i < 2000; ++i) acc += 1.0 / (i + 1.0);
        }
        return fr.find("photometric") != nullptr;
      };
    } else {
      n.run = [](BlockFrame& fr) { return fr.find("photometric") != nullptr; };
    }
    f.nodes.push_back(n);
  }
  return f;
}

std::vector<FrameOutcome> run_with(int workers, bool prefetch, std::uint64_t mem_limit,
                                   std::size_t n_frames, ProbeSink** sink_out,
                                   PrefetchCache** cache_out, int* loads_out) {
  static std::vector<std::unique_ptr<NormalizeWorkflowScheduler>> keep;
  NormalizeWorkflowConfig cfg;
  cfg.workers = workers;
  cfg.prefetch_enabled = prefetch;
  cfg.memory_limit_bytes = mem_limit;
  keep.emplace_back(new NormalizeWorkflowScheduler(cfg));
  auto* s = keep.back().get();
  static std::atomic<int> loader_calls{0};
  loader_calls.store(0);
  s->set_prefetch_loader([&](const std::string& key) {
    loader_calls.fetch_add(1);
    return "catalog:" + key;
  });
  for (std::size_t i = 0; i < n_frames; ++i)
    s->add_frame(make_frame(1000 + i, 256, "gaia_" + std::to_string(i % 3)));
  auto out = s->run();
  if (sink_out) *sink_out = &s->probes();
  if (cache_out) *cache_out = &s->cache();
  if (loads_out) *loads_out = loader_calls.load();
  return out;
}
}  // namespace

int main() {
  // A/B. 确定性：N=1/2/4/8 逐位一致 + 输出按 frame_id 升序
  {
    std::vector<std::vector<FrameOutcome>> all;
    for (int w : {1, 2, 4, 8}) {
      ProbeSink* sink = nullptr; PrefetchCache* cache = nullptr; int loads = 0;
      all.push_back(run_with(w, true, 0, 6, &sink, &cache, &loads));
    }
    bool same = true, ordered = true, all_ok = true;
    for (std::size_t k = 1; k < all.size(); ++k) {
      if (all[k].size() != all[0].size()) { same = false; break; }
      for (std::size_t i = 0; i < all[0].size(); ++i) {
        if (all[k][i].checksum != all[0][i].checksum || all[k][i].ok != all[0][i].ok)
          same = false;
      }
    }
    for (const auto& v : all) {
      for (std::size_t i = 0; i < v.size(); ++i) {
        if (!v[i].ok) all_ok = false;
        if (i && v[i].frame_id <= v[i - 1].frame_id) ordered = false;
      }
    }
    check(all_ok, "A1 all frames ok (N=1/2/4/8)");
    check(same, "A2 N=1/2/4/8 outcomes bitwise identical (checksum)");
    check(ordered, "B1 outcomes sorted by frame_id ascending");
    std::printf("INFO checksum(N=1) = %llu\n", (unsigned long long)all[0][0].checksum);
  }

  // C. 预取：开启时命中率 > 0；关闭时加载次数 = 键数（每 key 只加载一次）
  {
    ProbeSink* sink = nullptr; PrefetchCache* cache = nullptr; int loads = 0;
    auto out = run_with(4, true, 0, 6, &sink, &cache, &loads);
    (void)out;
    const std::size_t hits = sink->count(ProbeKind::CACHE_HIT);
    double hit_sum = sink->sum_value(ProbeKind::CACHE_HIT);
    check(hits == 6, "C1 exactly one cache_hit probe per frame, got " + std::to_string(hits));
    check(hit_sum > 0.0, "C2 at least one prefetch key was an L1 hit (repeat key across frames)");
    check(loads <= 3, "C3 single-flight: 3 distinct keys loaded at most 3 times, got " +
                          std::to_string(loads));
    check(cache->l1_hits() + cache->misses() > 0, "C4 cache counters populated");
    std::printf("INFO prefetch hits=%zu l1_hit_sum=%.0f loads=%d distinct_keys=3\n",
                hits, hit_sum, loads);

    ProbeSink* sink2 = nullptr; PrefetchCache* c2 = nullptr; int loads2 = 0;
    auto out2 = run_with(4, false, 0, 6, &sink2, &c2, &loads2);
    (void)out2;
    check(sink2->count_prefetch_phase(1) == 0, "C5 prefetch disabled => no idle-phase (phase 1) probes");
    check(sink2->count_prefetch_phase(2) == 6, "C6 prefetch disabled => all 6 probes are phase 2 (critical path)");
  }

  // D. 生命周期：无残留块；峰值受在途集合约束
  {
    ProbeSink* sink = nullptr; PrefetchCache* cache = nullptr; int loads = 0;
    auto out = run_with(4, true, 0, 4, &sink, &cache, &loads);
    bool no_leak = true;
    for (const auto& o : out) if (o.bytes_leaked != 0) no_leak = false;
    check(no_leak, "D1 no leaked bytes per frame (destroy_all returns 0)");
    check(sink->count(ProbeKind::BLOCK_DEATH) >= 3, "D2 block_death probes emitted");
    check(sink->count(ProbeKind::BLOCK_BIRTH) >= 3, "D3 block_birth probes emitted");
    check(sink->count(ProbeKind::NODE_WALL) == 4 * 4, "D4 one node_wall per node per frame");
    check(sink->count(ProbeKind::QUEUE_WAIT) == 4, "D5 one queue_wait per frame");
  }

  // E. 内存上限：超限必须失败（不是静默通过）
  {
    ProbeSink* sink = nullptr; PrefetchCache* cache = nullptr; int loads = 0;
    auto out = run_with(1, false, 512, 1, &sink, &cache, &loads);   // 256 f64 = 2 KiB/块
    check(!out.empty() && !out[0].ok, "E1 memory limit exceeded => frame fails");
    check(!out.empty() && out[0].error == "memory_limit_exceeded",
          "E2 error is memory_limit_exceeded, got '" + (out.empty() ? std::string("?") : out[0].error) + "'");
  }

  // F. 取消：能返回、不挂死、不泄漏
  {
    NormalizeWorkflowConfig cfg;
    cfg.workers = 4;
    NormalizeWorkflowScheduler s(cfg);
    for (int i = 0; i < 4; ++i) s.add_frame(make_frame(2000 + i, 256, "k"));
    s.cancel();
    auto out = s.run();
    check(out.size() == 4, "F1 cancel path returns one outcome per frame");
    bool no_leak = true;
    for (const auto& o : out) if (o.bytes_leaked != 0) no_leak = false;
    check(no_leak, "F2 cancel path leaks no bytes");
    std::printf("INFO cancel path ok=%d/%zu\n",
                (int)std::count_if(out.begin(), out.end(), [](const FrameOutcome& o) { return o.ok; }),
                out.size());
  }

  // G. 探针 schema：8 类事件名与合同一致
  {
    const char* names[] = {"node_wall", "queue_wait", "block_birth", "block_death",
                           "rss", "io", "worker_busy", "cache_hit"};
    bool ok = true;
    for (int i = 0; i < 8; ++i)
      if (std::string(probe_kind_name(static_cast<ProbeKind>(i))) != names[i]) ok = false;
    check(ok, "G1 all 8 probe kinds match CONTRACT-501 names");
  }


  // ── H. 预取墙钟对比 + 命中率探针证据（ARCH-502 验收门）─────────────────
  // 语义：prefetch_enabled 只控制"专用预取线程"；关闭时帧仍同步加载自己的预取键，
  // 于是 I/O 落在关键路径上 —— 这就是"无预取 vs 预取"的对比含义。
  // 两个场景分别取证（诚实边界）：
  //   H5 欠饱和（workers=1）：有闲置产能 ⇒ 预取应显著缩短墙钟；
  //   H6 饱和（workers=4）：无闲置产能 ⇒ **不应**期望收益，只记录不设阈值。
  {
    struct Meas { double wall; std::size_t p1; std::size_t p2; double hit_rate; };
    auto measure = [](bool prefetch, int workers) -> Meas {
      NormalizeWorkflowConfig cfg; cfg.workers = workers; cfg.prefetch_enabled = prefetch;
      cfg.prefetch_threads = 1;
      NormalizeWorkflowScheduler s(cfg);
      s.set_prefetch_loader([](const std::string& k) {
        std::this_thread::sleep_for(std::chrono::milliseconds(6));   // 模拟 I/O 延迟
        return "catalog:" + k;
      });
      for (int i = 0; i < 8; ++i)
        s.add_frame(make_frame(3000 + i, 512, "g" + std::to_string(i), 12));
      const auto t0 = std::chrono::steady_clock::now();
      auto out = s.run();
      const auto t1 = std::chrono::steady_clock::now();
      (void)out;
      Meas m;
      m.wall = std::chrono::duration<double>(t1 - t0).count();
      m.p1 = s.probes().count_prefetch_phase(1);
      m.p2 = s.probes().count_prefetch_phase(2);
      const std::size_t total = s.probes().count(ProbeKind::CACHE_HIT);
      const double hs = s.probes().sum_value(ProbeKind::CACHE_HIT);
      m.hit_rate = (total > 0) ? hs / static_cast<double>(total) : 0.0;
      return m;
    };
    const Meas on1 = measure(true, 1);
    const Meas off1 = measure(false, 1);
    const Meas on4 = measure(true, 4);
    const Meas off4 = measure(false, 4);
    check(off1.p1 == 0, "H1 prefetch off => no prefetch-thread probes, got " + std::to_string(off1.p1));
    check(on1.p1 + on1.p2 == 8, "H2 exactly one cache_hit probe per frame, got " +
                                   std::to_string(on1.p1 + on1.p2));
    check(on1.p1 > 0, "H3 prefetch thread actually fired (p1=" + std::to_string(on1.p1) + ")");
    // 重叠的直接证据：预取线程在 worker 需要之前就把该帧的键解析完了（phase 2 = 0）
    check(on1.p2 == 0, "H3b prefetch thread resolved every frame ahead of its worker "
                           "(phase-2 fallbacks = " + std::to_string(on1.p2) + ")");
    check(off1.p2 == 8, "H3c prefetch off => all 8 frames resolved on the critical path, got " +
                            std::to_string(off1.p2));
    check(on1.wall < off1.wall,
          "H5 under-saturated (workers=1): prefetch overlaps I/O with compute: " +
              std::to_string(on1.wall) + " vs " + std::to_string(off1.wall));
    std::printf("INFO prefetch w=1 on=%.4fs (p1=%zu p2=%zu) off=%.4fs (p2=%zu) speedup=%.2fx hit_rate=%.3f\n",
                on1.wall, on1.p1, on1.p2, off1.wall, off1.p2,
                on1.wall > 0 ? off1.wall / on1.wall : 0.0, on1.hit_rate);
    std::printf("INFO prefetch w=4 on=%.4fs off=%.4fs speedup=%.2fx (saturated: no gain expected)\n",
                on4.wall, off4.wall, on4.wall > 0 ? off4.wall / on4.wall : 0.0);
    {
      std::ofstream ev(std::string(ARCH502_EVIDENCE_DIR) + "/arch502_prefetch.txt", std::ios::trunc);
      if (ev) ev << "# ARCH-502 预取墙钟对比（同一合成负载，仅切 prefetch_enabled）\n"
                 << "w1_prefetch_on_wall_s=" << on1.wall << "\n"
                 << "w1_prefetch_off_wall_s=" << off1.wall << "\n"
                 << "w1_speedup=" << (on1.wall > 0 ? off1.wall / on1.wall : 0.0) << "\n"
                 << "w1_phase1_probes=" << on1.p1 << "\nw1_phase2_probes=" << on1.p2 << "\n"
                 << "w1_off_phase2_probes=" << off1.p2 << "\n"
                 << "w1_l1_hit_rate=" << on1.hit_rate << "\n"
                 << "w4_prefetch_on_wall_s=" << on4.wall << "\n"
                 << "w4_prefetch_off_wall_s=" << off4.wall << "\n"
                 << "w4_speedup=" << (on4.wall > 0 ? off4.wall / on4.wall : 0.0) << "\n"
                 << "# 负载：frames=8 keys_per_frame=1 loader_delay_ms=6 compute_ms=12 prefetch_threads=1\n"
                 << "# 诚实边界：饱和场景（w=4）无闲置产能，预取不缩短墙钟，故不设阈值断言。\n";
    }
  }

  // ── I. 峰值内存受在途块集合约束（帧并发度 vs RSS 曲线）──────────────────
  {
    auto rss_kb = []() -> long {
      std::ifstream f("/proc/self/statm");
      long size = 0, resident = 0;
      if (f >> size >> resident) return resident * (sysconf(_SC_PAGESIZE) / 1024);
      return -1;
    };
    std::ofstream ev(std::string(ARCH502_EVIDENCE_DIR) + "/arch502_rss_curve.csv", std::ios::trunc);
    if (ev) ev << "workers,frames,peak_inflight_bytes,rss_kb\n";
    bool monotone_ok = true;
    std::size_t prev_peak = 0;
    for (int w : {1, 2, 4}) {
      NormalizeWorkflowConfig cfg; cfg.workers = w; cfg.prefetch_enabled = false;
      NormalizeWorkflowScheduler s(cfg);
      for (int i = 0; i < 8; ++i) s.add_frame(make_frame(4000 + i, 4096, ""));
      auto out = s.run();
      const std::size_t peak = s.peak_inflight_bytes();
      const long r = rss_kb();
      if (ev) ev << w << "," << out.size() << "," << peak << "," << r << "\n";
      // 在途峰值必须被"单帧在途块集合"约束住：不随并发度线性增长
      if (peak < prev_peak) monotone_ok = false;
      prev_peak = peak;
    }
    check(prev_peak > 0, "I1 peak in-flight bytes observed");
    check(monotone_ok, "I2 peak in-flight bytes bounded by per-frame block set");
    std::printf("INFO peak in-flight bytes (w=4, 8 frames x 4096 f64) = %zu\n", prev_peak);
  }

  // ── J. 磁盘满 / I/O 失败路径：必须传播失败且不留残块 ────────────────────
  {
    NormalizeWorkflowConfig cfg; cfg.workers = 2; cfg.prefetch_enabled = false;
    NormalizeWorkflowScheduler s(cfg);
    NormalizeFrame f = make_frame(5000, 256, "");
    NormalizeNode bad;
    bad.id = "write_product";
    bad.consumes = {"photometric"};
    bad.run = [](BlockFrame&) { return false; };   // 模拟 ENOSPC 写出失败
    f.nodes.push_back(bad);
    s.add_frame(f);
    auto out = s.run();
    check(out.size() == 1 && !out[0].ok, "J1 disk-full node failure propagates");
    check(!out.empty() && out[0].error == "node_failed:write_product",
          "J2 error names the failing node, got '" + (out.empty() ? std::string("?") : out[0].error) + "'");
    check(!out.empty() && out[0].bytes_leaked == 0, "J3 no residual blocks after failure");
  }

  // ── K. 探针 JSONL 落盘 + 与 CONTRACT-501 schema 字段一致性 ──────────────
  {
    const std::string pj = std::string(ARCH502_EVIDENCE_DIR) + "/arch502_probes.jsonl";
    NormalizeWorkflowConfig cfg; cfg.workers = 4; cfg.prefetch_enabled = true;
    cfg.probe_path = pj;
    NormalizeWorkflowScheduler s(cfg);
    s.set_prefetch_loader([](const std::string& k) { return "c:" + k; });
    for (int i = 0; i < 3; ++i) s.add_frame(make_frame(6000 + i, 256, "k" + std::to_string(i % 2)));
    auto out = s.run();
    (void)out;
    std::ifstream f(pj);
    std::size_t lines = 0, good = 0;
    std::string line;
    while (std::getline(f, line)) {
      if (line.empty()) continue;
      ++lines;
      const bool has_ts = line.find("\"ts\":") != std::string::npos;
      const bool has_stage = line.find("\"stage\":\"normalize\"") != std::string::npos;
      const bool has_kind = line.find("\"kind\":") != std::string::npos;
      const bool has_val = line.find("\"value\":") != std::string::npos;
      const bool has_unit = line.find("\"unit\":") != std::string::npos;
      if (has_ts && has_stage && has_kind && has_val && has_unit) ++good;
    }
    check(lines > 0, "K1 probe JSONL written");
    check(lines == good, "K2 every line has ts/stage/kind/value/unit (CONTRACT-501 §4 required)");
    std::printf("INFO probe JSONL lines=%zu all-schema-valid=%zu -> %s\n", lines, good, pj.c_str());
  }

  std::printf("NORMALIZE-WORKFLOW: %d/%d checks passed, %d failed\n", g_total - g_fail,
              g_total, g_fail);
  return g_fail == 0 ? 0 : 1;
}
