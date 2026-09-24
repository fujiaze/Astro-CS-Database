// eng/tests/unit/export_stream_test.cpp — ARCH-504 export 子块流式调度器回归锁
//
// 依据：CONTRACT-501 docs/contracts/SCHEDULER_CONTRACT.md §2/§3/§4/§5；
//       ASTROCS_DESIGN §6/§8.3。
// 判据（每条可证伪）：
//   A. 与整幅参考路径输出**逐位一致**（同一像素函数、行主序 checksum 相同），且与 worker 数无关；
//   B. 原子发布：产品文件存在且长度 = WCS 头 + properties + 像素字节；无 .tmp 残留；
//   C. 磁盘满：exit_code = 10、**不发布**、无 .tmp 残留；
//   D. WCS 头/properties 缺失 ⇒ 拒绝开写（exit 1，不产出半成品）；
//   E. 背压：队列水位受 queue_depth 约束（不无界增长）；
//   F. RSS 与子块大小成正比、与总图大小无关（4×/16× 像素规模扫描）；
//   G. 探针：每子块一条 io + 一条 node_wall，stage=export；
//   H. 取消：能返回、不挂死。
#include "astrocs/core/export_stream.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

using namespace astrocs::core;

#ifndef ARCH504_EVIDENCE_DIR
#define ARCH504_EVIDENCE_DIR "run/RELEASE-05/evidence"
#endif

namespace {
int g_fail = 0, g_total = 0;
void check(bool ok, const std::string& what) {
  ++g_total;
  if (!ok) { ++g_fail; std::printf("FAIL %s\n", what.c_str()); }
}

void ensure_dir() {
  std::string p(ARCH504_EVIDENCE_DIR), acc;
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

// 参考像素函数（确定性；与 TAN 无关，本测试只验流式编排的等价性）
double ref_pixel(int x, int y) {
  return 1000.0 + 3.0 * x - 2.0 * y + 0.25 * (x % 7) * (y % 5);
}

// 整幅参考路径：一次性算完，行主序 FNV
std::uint64_t reference_checksum(int w, int h) {
  std::uint64_t hv = 1469598103934665603ULL;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const double v = ref_pixel(x, y);
      const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(&v);
      for (std::size_t i = 0; i < sizeof(double); ++i) { hv ^= p[i]; hv *= 1099511628211ULL; }
    }
  return hv;
}

ExportStreamConfig cfg_for(const std::string& tag, int sub_px, int qd, int workers) {
  ExportStreamConfig c;
  c.workers = workers;
  c.sub_block_px = sub_px;
  c.queue_depth = qd;
  c.probe_path = std::string(ARCH504_EVIDENCE_DIR) + "/arch504_probes.jsonl";
  c.manifest_path = std::string(ARCH504_EVIDENCE_DIR) + "/arch504_manifest.json";
  c.output_path = std::string(ARCH504_EVIDENCE_DIR) + "/arch504_" + tag + ".fits";
  c.wcs_header = "SIMPLE  =                    T / TAN (frozen)\nNAXIS   =                    2\n";
  c.properties = "ASTROCS PROVENANCE\nPROJECT = ACSD\n";
  return c;
}

std::uintmax_t file_size_of(const std::string& p) {
  struct stat st;
  if (::stat(p.c_str(), &st) != 0) return static_cast<std::uintmax_t>(-1);
  return static_cast<std::uintmax_t>(st.st_size);
}
}  // namespace

int main() {
  ensure_dir();
  const int W = 1024, H = 768;

  // A. 与整幅参考逐位一致，且与 worker 数无关
  {
    const std::uint64_t ref = reference_checksum(W, H);
    bool all_same = true;
    std::vector<std::uint64_t> got;
    for (int w : {1, 2, 4, 8}) {
      auto c = cfg_for("w" + std::to_string(w), 128, 4, w);
      ExportStreamScheduler s(c);
      s.set_image(W, H);
      s.set_pixel_fn(ref_pixel);
      auto o = s.run();
      check(o.ok, "A0 run ok (workers=" + std::to_string(w) + ") err=" + o.error);
      got.push_back(o.checksum);
      if (o.checksum != ref) all_same = false;
    }
    check(all_same, "A1 streaming output bitwise identical to whole-image reference (all workers)");
    for (std::size_t i = 1; i < got.size(); ++i)
      if (got[i] != got[0]) all_same = false;
    check(all_same, "A2 output independent of worker count");
    std::printf("INFO reference checksum=%llu streaming(w=1)=%llu\n",
                (unsigned long long)ref, (unsigned long long)got[0]);
  }

  // B. 原子发布
  {
    auto c = cfg_for("publish", 256, 4, 4);
    ExportStreamScheduler s(c);
    s.set_image(W, H);
    s.set_pixel_fn(ref_pixel);
    auto o = s.run();
    const std::uintmax_t sz = file_size_of(c.output_path);
    const std::uintmax_t expect = c.wcs_header.size() + c.properties.size() +
                                  static_cast<std::uintmax_t>(W) * H * sizeof(double);
    check(o.published, "B1 published");
    check(sz == expect, "B2 product size = header + properties + pixels: " +
                            std::to_string(sz) + " vs " + std::to_string(expect));
    check(file_size_of(c.output_path + ".tmp") == static_cast<std::uintmax_t>(-1),
          "B3 no .tmp residue after publish");
  }

  // C. 磁盘满：exit 10、不发布、无残留
  {
    auto c = cfg_for("diskfull", 128, 4, 4);
    c.fail_write_after_bytes = 4096;   // 写 4 KiB 后模拟 ENOSPC
    ExportStreamScheduler s(c);
    s.set_image(W, H);
    s.set_pixel_fn(ref_pixel);
    auto o = s.run();
    check(!o.ok && o.exit_code == 10, "C1 disk full => exit_code 10, got " +
                                          std::to_string(o.exit_code) + " err=" + o.error);
    check(!o.published, "C2 disk full => not published");
    check(file_size_of(c.output_path) == static_cast<std::uintmax_t>(-1),
          "C3 disk full => no partial product at output path");
    check(file_size_of(c.output_path + ".tmp") == static_cast<std::uintmax_t>(-1),
          "C4 disk full => no .tmp residue");
  }

  // D. WCS 头/properties 缺失 ⇒ 拒绝开写
  {
    auto c = cfg_for("noheader", 256, 4, 2);
    c.wcs_header.clear();
    ExportStreamScheduler s(c);
    s.set_image(64, 64);
    s.set_pixel_fn(ref_pixel);
    auto o = s.run();
    check(!o.ok && o.exit_code == 1, "D1 missing WCS header => refuse to open output (exit 1)");
    check(file_size_of(c.output_path) == static_cast<std::uintmax_t>(-1),
          "D2 no half product without header");
  }

  // E. 背压：队列水位受 queue_depth 约束
  {
    std::ofstream ev(std::string(ARCH504_EVIDENCE_DIR) + "/arch504_backpressure.csv", std::ios::trunc);
    if (ev) ev << "queue_depth,workers,max_queue_observed\n";
    bool bounded = true;
    for (int qd : {1, 2, 4, 8}) {
      auto c = cfg_for("bp" + std::to_string(qd), 64, qd, 4);
      ExportStreamScheduler s(c);
      s.set_image(W, H);
      s.set_pixel_fn(ref_pixel);
      auto o = s.run();
      if (ev) ev << qd << ",4," << o.max_queue_depth << "\n";
      if (o.max_queue_depth > static_cast<std::size_t>(2 * qd)) bounded = false;
    }
    check(bounded, "E1 queue watermark bounded by queue_depth (backpressure holds)");
  }

  // F. RSS/在途字节与子块大小成正比、与总图大小无关
  {
    std::ofstream ev(std::string(ARCH504_EVIDENCE_DIR) + "/arch504_rss_curve.csv", std::ios::trunc);
    if (ev) ev << "width,height,sub_block_px,peak_inflight_bytes,max_queue,bytes_written\n";
    // 子块固定、总图 4×/16× 增长 ⇒ 在途峰值必须不变
    std::size_t peak_small = 0, peak_big = 0;
    for (int scale : {1, 4}) {
      const int w = W * scale, h = H * scale;
      auto c = cfg_for("rss" + std::to_string(scale), 128, 4, 4);
      ExportStreamScheduler s(c);
      s.set_image(w, h);
      s.set_pixel_fn(ref_pixel);
      auto o = s.run();
      if (ev) ev << w << "," << h << ",128," << o.peak_resident_bytes << "," << o.max_queue_depth
                 << "," << o.bytes_written << "\n";
      if (scale == 1) peak_small = o.peak_resident_bytes;
      if (scale == 4) peak_big = o.peak_resident_bytes;
    }
    check(peak_small == peak_big,
          "F1 in-flight peak independent of total image size (1x vs 4x): " +
              std::to_string(peak_small) + " vs " + std::to_string(peak_big));
    // 子块增大 ⇒ 在途峰值随之增大（成正比）
    std::size_t p64 = 0, p256 = 0;
    for (int sb : {64, 256}) {
      auto c = cfg_for("sbsize" + std::to_string(sb), sb, 4, 4);
      ExportStreamScheduler s(c);
      s.set_image(W, H);
      s.set_pixel_fn(ref_pixel);
      auto o = s.run();
      if (ev) ev << W << "," << H << "," << sb << "," << o.peak_resident_bytes << ","
                 << o.max_queue_depth << "," << o.bytes_written << "\n";
      if (sb == 64) p64 = o.peak_resident_bytes;
      if (sb == 256) p256 = o.peak_resident_bytes;
    }
    check(p256 > p64, "F2 in-flight peak scales with sub-block size: " + std::to_string(p64) +
                          " -> " + std::to_string(p256));
    std::printf("INFO peak in-flight 1x=%zu 4x=%zu (sub-block 128, qd 4)\n", peak_small, peak_big);
  }

  // G. 探针
  {
    auto c = cfg_for("probe", 256, 4, 4);
    ExportStreamScheduler s(c);
    s.set_image(512, 512);
    s.set_pixel_fn(ref_pixel);
    auto o = s.run();
    (void)o;
    const std::size_t n = s.sub_block_count();
    check(s.probes().count(ProbeKind::IO) == n, "G1 one io probe per sub-block");
    check(s.probes().count(ProbeKind::NODE_WALL) == n, "G2 one node_wall probe per sub-block");
    bool stage_ok = true;
    for (const auto& e : s.probes().events()) if (e.stage != "export") stage_ok = false;
    check(stage_ok, "G3 every probe event has stage=export");
  }

  // H. 取消
  {
    auto c = cfg_for("cancel", 64, 2, 4);
    ExportStreamScheduler s(c);
    s.set_image(W, H);
    s.set_pixel_fn(ref_pixel);
    s.cancel();
    auto o = s.run();
    check(!o.published || o.ok, "H1 cancel path returns without hanging");
  }

  std::printf("EXPORT-STREAM: %d/%d checks passed, %d failed\n", g_total - g_fail, g_total, g_fail);
  return g_fail == 0 ? 0 : 1;
}
