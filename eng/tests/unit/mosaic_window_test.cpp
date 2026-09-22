// eng/tests/unit/mosaic_window_test.cpp — ARCH-503 mosaic 天球窗口并行调度器回归锁
//
// 依据：CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md §2（窗口并行与确定性）/
//       §3（资源声明）/§4（探针）；ASTROCS_DESIGN §8.3（mosaic 行）。
// 判据（每条可证伪）：
//   A. 确定性：N=1/2/4/8/16 worker 的窗口 outcome（含 checksum）**逐位一致**；
//   B. 归约顺序：输出恒按 window_id 升序；
//   C. 与单窗口参考实现数值一致（窗口大小 = 全部 tile ⇒ 1 个窗口），checksum 完全相同；
//   D. 读放大：按窗口路由的读取量显著低于「为每个窗口读整帧」的朴素基线；给对比表；
//   E. 窗口峰值驻留（MEM-WIRE-01 修 ARCH-AUDIT-02 F-07）：判据量**实测驻留**
//      （窗口输出像素缓冲 capacity + 路由指针向量），四条同时成立才绿：
//      ① peak>0 ② peak<=解析上界 ③ 与总图规模解耦 ④ 装满时 window_tiles 更大 ⇒ peak 严格更大；
//      含两条**负例**：常量驻留（sizeof(double)*4）与零驻留注入必须判红（旧判据恒真）；
//   F. manifest：窗口大小、窗口数、路由字节、读放大必须落盘；
//   G. 探针：每窗口一条 io + 一条 node_wall；stage 必须为 mosaic；
//   H. 取消：能返回、不挂死。
#include "astrocs/core/mosaic_window.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace astrocs::core;

#ifndef ARCH503_EVIDENCE_DIR
#define ARCH503_EVIDENCE_DIR "run/RELEASE-05/evidence"
#endif

namespace {
int g_fail = 0, g_total = 0;
void check(bool ok, const std::string& what) {
  ++g_total;
  if (!ok) { ++g_fail; std::printf("FAIL %s\n", what.c_str()); }
}

void ensure_dir() {
  std::string p(ARCH503_EVIDENCE_DIR), acc;
  std::size_t i = 0;
  if (!p.empty() && p[0] == '/') { acc = "/"; i = 1; }
  while (i <= p.size()) {
    const std::size_t j = p.find('/', i);
    const std::string part = p.substr(i, (j == std::string::npos ? p.size() : j) - i);
    if (!part.empty()) {
      if (!acc.empty() && acc.back() != '/') acc.push_back('/');
      acc += part;
      ::mkdir(acc.c_str(), 0755);
    }
    if (j == std::string::npos) break;
    i = j + 1;
  }
}

// 合成输入：n_frames 帧 × n_tiles 个 tile，每 tile 固定字节数
std::vector<MosaicFrameInput> make_frames(int n_frames, int n_tiles, std::uint64_t tile_bytes) {
  std::vector<MosaicFrameInput> v;
  for (int f = 0; f < n_frames; ++f) {
    MosaicFrameInput in;
    in.frame_id = 7000 + static_cast<std::uint64_t>(f);
    for (int t = 0; t < n_tiles; ++t) in.tile_bytes[static_cast<std::uint64_t>(t)] = tile_bytes;
    v.push_back(std::move(in));
  }
  return v;
}

std::vector<WindowOutcome> run_with(int workers, int window_tiles, int n_frames, int n_tiles,
                                    std::uint64_t tile_bytes, std::size_t* peak_out = nullptr,
                                    double* amp_out = nullptr, std::uint64_t* routed_out = nullptr,
                                    std::uint64_t* naive_out = nullptr) {
  MosaicWindowConfig cfg;
  cfg.workers = workers;
  cfg.window_tiles = window_tiles;
  cfg.manifest_path = std::string(ARCH503_EVIDENCE_DIR) + "/arch503_manifest.json";
  MosaicWindowScheduler s(cfg);
  for (auto& f : make_frames(n_frames, n_tiles, tile_bytes)) s.add_frame(std::move(f));
  auto out = s.run();
  if (peak_out) *peak_out = s.peak_inflight_bytes();
  if (amp_out) *amp_out = s.read_amplification();
  if (routed_out) *routed_out = s.total_read_bytes();
  if (naive_out) *naive_out = s.naive_baseline_bytes();
  return out;
}
}  // namespace

int main() {
  ensure_dir();

  // A/B. 确定性：N=1/2/4/8/16 逐位一致 + 按 window_id 升序
  {
    std::vector<std::vector<WindowOutcome>> all;
    for (int w : {1, 2, 4, 8, 16}) all.push_back(run_with(w, 4, 6, 16, 4096));
    bool same = true, ordered = true, all_ok = true;
    for (std::size_t k = 1; k < all.size(); ++k) {
      if (all[k].size() != all[0].size()) { same = false; break; }
      for (std::size_t i = 0; i < all[0].size(); ++i)
        if (all[k][i].checksum != all[0][i].checksum || all[k][i].ok != all[0][i].ok) same = false;
    }
    for (const auto& v : all) {
      for (std::size_t i = 0; i < v.size(); ++i) {
        if (!v[i].ok) all_ok = false;
        if (i && v[i].window_id <= v[i - 1].window_id) ordered = false;
      }
    }
    check(all_ok, "A1 all windows ok (N=1/2/4/8/16)");
    check(same, "A2 N=1/2/4/8/16 window outcomes bitwise identical");
    check(ordered, "B1 outcomes sorted by window_id ascending");
    std::printf("INFO windows=%zu checksum(w0)=%llu\n", all[0].size(),
                (unsigned long long)all[0][0].checksum);
  }

  // C. 与单窗口参考实现数值一致（冻结容差 = 0：逐位相同）
  {
    auto ref = run_with(1, 16, 6, 16, 4096);      // 窗口大小 = 全部 tile ⇒ 1 个窗口 = 参考
    auto par = run_with(8, 4, 6, 16, 4096);       // 4 个窗口并行
    check(ref.size() == 1, "C1 single-window reference has exactly 1 window");
    std::vector<double> a, b;
    for (const auto& o : ref) a.insert(a.end(), o.pixel_values.begin(), o.pixel_values.end());
    for (const auto& o : par) b.insert(b.end(), o.pixel_values.begin(), o.pixel_values.end());
    check(!a.empty() && a.size() == b.size(),
          "C2 pixel counts match reference: " + std::to_string(a.size()) + " vs " +
              std::to_string(b.size()));
    bool bitwise = (a.size() == b.size());
    std::size_t first_diff = 0;
    for (std::size_t i = 0; bitwise && i < a.size(); ++i)
      if (a[i] != b[i]) { bitwise = false; first_diff = i; }
    check(bitwise, "C3 window-parallel result is BITWISE identical to single-window reference" +
                       (bitwise ? std::string() : " (first diff at " + std::to_string(first_diff) + ")"));
    std::printf("INFO reference px=%zu parallel px=%zu bitwise_equal=%d\n", a.size(), b.size(),
                (int)bitwise);
  }

  // D. 读放大：路由读取 vs 朴素基线（为每个窗口读整帧）
  {
    std::size_t peak = 0; double amp = 0.0; std::uint64_t routed = 0, naive = 0;
    auto out = run_with(8, 4, 6, 16, 4096, &peak, &amp, &routed, &naive);
    check(routed > 0 && naive > 0, "D1 read counters populated");
    check(routed < naive, "D2 routed read < naive whole-frame baseline: " + std::to_string(routed) +
                              " vs " + std::to_string(naive));
    check(amp < 0.3, "D3 read amplification < 0.3 (window routing removes whole-frame re-read), got " +
                         std::to_string(amp));
    std::printf("INFO read routed=%llu B naive=%llu B amplification=%.4f windows=%zu\n",
                (unsigned long long)routed, (unsigned long long)naive, amp, out.size());
    std::ofstream ev(std::string(ARCH503_EVIDENCE_DIR) + "/arch503_read_amp.txt", std::ios::trunc);
    if (ev) ev << "routed_bytes=" << routed << "\nnaive_whole_frame_bytes=" << naive
               << "\nread_amplification=" << amp << "\nwindows=" << out.size()
               << "\nframes=6 tiles=16 tile_bytes=4096 window_tiles=4\n";
  }

  // E. 窗口峰值驻留（MEM-WIRE-01 修 ARCH-AUDIT-02 F-07：判据量**实测驻留**，可红可绿）
  //
  // 修复前：run_window 把峰值驻留写成编译期常量 sizeof(double)*4 = 32 B，是写入 peak 的
  // 唯一来源 ⇒ 旧 E1（peak>4096 判红）与旧 E2（16 vs 64 tiles 峰值相等）断言对象即该常量，
  // **不可能红**（AGENTS §5「恒真门没有证据资格」；docs/KNOWN_LIMITATIONS.md 条目 35）。
  // 现行：峰值 = 窗口真实存活分配的实测值（输出像素缓冲 capacity + 路由指针向量 + 标量），
  // 判据由 window_peak_residency_ok 唯一实现，四条同时成立才绿：
  //   ① peak>0 ② peak<=解析上界 ③ 与总图规模解耦 ④ window_tiles 增大 ⇒ peak 不减。
  {
    std::ofstream ev(std::string(ARCH503_EVIDENCE_DIR) + "/arch503_window_sweep.csv", std::ios::trunc);
    if (ev) ev << "window_tiles,tiles,frames,windows,peak_resident_bytes,bound_bytes,routed_bytes,naive_bytes,amplification\n";
    std::vector<astrocs::core::WindowPeakSample> samples;
    const std::size_t kFrames = 6;
    for (int wt : {1, 2, 4, 8, 16}) {
      std::size_t peak = 0; double amp = 0.0; std::uint64_t routed = 0, naive = 0;
      auto out = run_with(4, wt, 6, 16, 4096, &peak, &amp, &routed, &naive);
      const std::size_t bound =
          astrocs::core::MosaicWindowScheduler::window_peak_bound_bytes(
              static_cast<std::size_t>(wt), kFrames);
      if (ev) ev << wt << ",16,6," << out.size() << "," << peak << "," << bound << ","
                 << routed << "," << naive << "," << amp << "\n";
      samples.push_back({wt, 16, peak, bound});
    }
    for (int nt : {16, 32, 64}) {
      std::size_t peak = 0; double amp = 0.0; std::uint64_t routed = 0, naive = 0;
      auto out = run_with(4, 4, 6, nt, 4096, &peak, &amp, &routed, &naive);
      const std::size_t bound =
          astrocs::core::MosaicWindowScheduler::window_peak_bound_bytes(4, kFrames);
      if (ev) ev << 4 << "," << nt << ",6," << out.size() << "," << peak << "," << bound << ","
                 << routed << "," << naive << "," << amp << "\n";
      samples.push_back({4, static_cast<std::size_t>(nt), peak, bound});
    }
    std::string why;
    const bool green = astrocs::core::window_peak_residency_ok(samples, &why);
    check(green, "E1/E2/E3 window peak residency criterion GREEN on measured samples" +
                     (green ? std::string() : (" (why: " + why + ")")));
    // 实测非退化自证：窗口越大峰值越大（若为编译期常量则必然相等 ⇒ 旧门恒真）
    std::size_t p1 = 0, p16 = 0;
    for (const auto& s : samples) {
      if (s.tiles_total != 16) continue;
      if (s.window_tiles == 1) p1 = s.peak_bytes;
      if (s.window_tiles == 16) p16 = s.peak_bytes;
    }
    check(p16 > p1, "E4 measured peak strictly grows with window size (1 vs 16 tiles/window): " +
                        std::to_string(p1) + " -> " + std::to_string(p16) +
                        " (compile-time constant would be equal = degenerate gate)");
    // ── 负例（红）：同一判据函数喂入「编译期常量 32 B」样本 ⇒ 必须判红 ──
    std::vector<astrocs::core::WindowPeakSample> mutated = samples;
    for (auto& s : mutated) s.peak_bytes = sizeof(double) * 4;   // F-07 的旧常量注入
    std::string why_const;
    const bool red_const = !astrocs::core::window_peak_residency_ok(mutated, &why_const);
    check(red_const, "E5 NEGATIVE: constant-residency injection (sizeof(double)*4) turns the " +
                         std::string("criterion RED (got ") + (red_const ? "RED" : "GREEN") +
                         "; why=" + why_const + ")");
    // ── 负例（红）：峰值恒 0（未记账 / 硬写 0）⇒ 必须判红 ──
    std::vector<astrocs::core::WindowPeakSample> zeroed = samples;
    for (auto& s : zeroed) s.peak_bytes = 0;
    std::string why_zero;
    const bool red_zero = !astrocs::core::window_peak_residency_ok(zeroed, &why_zero);
    check(red_zero, "E6 NEGATIVE: zero-residency injection turns the criterion RED (got " +
                        std::string(red_zero ? "RED" : "GREEN") + "; why=" + why_zero + ")");
    std::printf("INFO window sweep: peak resident w1=%zu B w16=%zu B (constant was %zu B); "
                "negative why_const=\"%s\"\n",
                p1, p16, sizeof(double) * 4, why_const.c_str());
  }

  // F. manifest 落盘
  {
    const std::string mp = std::string(ARCH503_EVIDENCE_DIR) + "/arch503_manifest.json";
    std::ifstream f(mp);
    std::stringstream ss; ss << f.rdbuf();
    const std::string s = ss.str();
    check(!s.empty(), "F1 manifest written");
    check(s.find("\"window_tiles\":") != std::string::npos, "F2 manifest records window size");
    check(s.find("\"window_count\":") != std::string::npos, "F3 manifest records window count");
    check(s.find("\"read_amplification\":") != std::string::npos, "F4 manifest records read amplification");
  }

  // G. 探针：每窗口一条 io + 一条 node_wall，stage=mosaic
  {
    MosaicWindowConfig cfg; cfg.workers = 4; cfg.window_tiles = 4;
    cfg.probe_path = std::string(ARCH503_EVIDENCE_DIR) + "/arch503_probes.jsonl";
    MosaicWindowScheduler s(cfg);
    for (auto& f : make_frames(3, 8, 2048)) s.add_frame(std::move(f));
    auto out = s.run();
    check(s.probes().count(ProbeKind::IO) == out.size(),
          "G1 one io probe per window");
    check(s.probes().count(ProbeKind::NODE_WALL) == out.size(),
          "G2 one node_wall probe per window");
    bool stage_ok = true;
    for (const auto& e : s.probes().events()) if (e.stage != "mosaic") stage_ok = false;
    check(stage_ok, "G3 every probe event has stage=mosaic");
  }

  // H. 取消：能返回、不挂死
  {
    MosaicWindowConfig cfg; cfg.workers = 4; cfg.window_tiles = 2;
    MosaicWindowScheduler s(cfg);
    for (auto& f : make_frames(4, 16, 1024)) s.add_frame(std::move(f));
    s.cancel();
    auto out = s.run();
    check(!out.empty(), "H1 cancel path returns outcomes");
  }

  std::printf("MOSAIC-WINDOW: %d/%d checks passed, %d failed\n", g_total - g_fail, g_total, g_fail);
  return g_fail == 0 ? 0 : 1;
}
