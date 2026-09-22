// eng/tests/unit/p3_export_stream_prod_test.cpp — P3-STREAM-01 生产 export 子块流式门
//
// 依据：ASTROCS_DESIGN.md §8.3 调度器表 export 行（「子块流式：读子块 → 投影
//   重采样 → 写 FITS，有界队列 + 背压，不整幅驻留；I/O 与计算重叠，内存占用与
//   子块大小成正比、**与总图大小无关**」）；docs/contracts/SCHEDULER_CONTRACT.md
//   §2 export 行（同文，FROZEN）；§3（分块/子块大小由配置决定）。
// 背景：ARCH-AUDIT-03 P3-01 判定生产 export 非子块流式（全幅 vector + 全幅
//   中间产物 + 单次 fits_write_pix），且 ARCH-504 调度器只被组件级单测引用；
//   原门 CHK-ARCH504-EXPORT-STREAM 只跑组件 ctest ⇒ 组件绿、生产未接线。
//
// 判据（每条可证伪，逐条落实测输出）：
//   ① 生产链（properties→wcs→resample2→writer→verify）确实引用
//      ExportStreamScheduler：writer 节点 manifest 必须自报
//      export_stream.scheduler == "ExportStreamScheduler" 且
//      whole_frame_fault == false；resample2/verify 节点必须自报
//      sub_block_streaming == true。（静态镜像见
//      eng/tools/arch/check_p3_export_stream_prod.py --self-test）
//   ② 峰值 RSS **随 sub_block_px 变化**（同一 W×H：sub_block 增大 ⇒ 峰值增大）。
//   ③ 峰值 RSS **与 W×H 无关**（同一 sub_block：面积 4× ⇒ 峰值基本不变）。
//   ④ 判据非退化（阳性对照）：故障注入 ASTROCS_P3_EXPORT_FAULT=
//      whole_frame_resident 走整幅驻留参考路径 ⇒ 同一度量必须显示峰值随面积
//      线性增长（4× 面积 ⇒ ≥2.5× 峰值）。**量的是真实驻留**（内核记账的 VmHWM，
//      经 /proc/self/clear_refs 归零后读取），不是任何声明值/记账计数器 ——
//      ARCH-AUDIT-02 已证同类门量编译期常量（32 B）恒真的先例。
//   ⑤ 产品语义不变：流式路径与整幅驻留参考路径的 FITS 产物**逐字节相同**
//      （integrity sha256 == canonical sha256 == 逐字节比对）。
//
// 隔离：每个 RSS 配置在**独立子进程**（fork+exec 本测试自身）内测量，避免
//   分配器复用/线程残留污染峰值；子进程只跑本测试，不启动任何产品可执行文件。
#include "astrocs/core/module.h"
#include "astrocs/core/module_adapters.h"
#include "astrocs/core/runtime.h"

#include "healpix_core.h"
#include "p1sess_fixtures.hpp"
#include "version_generated.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#endif

using json = nlohmann::json;
using namespace astrocs::core;

namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                          \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, \
                   #cond);                                                   \
      ++failures;                                                            \
    }                                                                        \
  } while (0)
#define CHECK_MSG(cond, msg)                                                 \
  do {                                                                       \
    if (!(cond)) {                                                           \
      std::fprintf(stderr, "CHECK failed %s:%d: %s -- %s\n", __FILE__,      \
                   __LINE__, #cond, (msg));                                  \
      ++failures;                                                            \
    }                                                                        \
  } while (0)

#ifndef P3STREAM_EVIDENCE_DIR
#define P3STREAM_EVIDENCE_DIR "run/P3-STREAM-01/evidence"
#endif

namespace {

// ── 真实驻留实测（内核记账，非采样、非声明值）────────────────────────────
// /proc/self/status 的 VmHWM = 内核维护的峰值 RSS；写 5 到 /proc/self/clear_refs
// 把峰值重置为当前 RSS（Linux ≥4.0）。任何一步不可用 ⇒ 判据显式失败，不静默跳过。
long rss_peak_kb() {
  std::ifstream f("/proc/self/status");
  std::string line;
  while (std::getline(f, line)) {
    if (line.rfind("VmHWM:", 0) == 0) {
      return std::strtol(line.c_str() + 6, nullptr, 10);
    }
  }
  return -1;
}

bool rss_reset_peak() {
  std::ofstream f("/proc/self/clear_refs");
  if (!f) return false;
  f << "5\n";
  f.flush();
  return f.good();
}

// ── 夹具（与 p3002_real_nodes_test / rt001_unique_executor_test 同规则）──
constexpr float kSigVal = 100.0f;
inline float const_px(int, void* user) { return *static_cast<float*>(user); }

std::string hips_properties_text() {
  std::string s;
  s += "hips_order = 0\n";
  s += "hips_tile_width = 512\n";
  s += "hips_tile_format = fits\n";
  s += "hips_frame = equatorial\n";
  s += "dataproduct_type = image\n";
  s += "hips_version = 1.0\n";
  // FIX-402: 生产 Phase3 输入语义守卫只放行显式面亮度输入（FZ-BUNIT-SEMANTICS）
  s += "BUNIT = ADU/px^2\n";
  s += "ASTROCS_PIXEL_SEMANTICS = surface_brightness\n";
  s += "ASTROCS_PIXEL_AREA_POWER = -2\n";
  return s;
}

bool write_completion_manifest(const std::string& root) {
  const std::string body =
      "{\n  \"format_version\": 1,\n  \"product\": \"HiPS\",\n"
      "  \"hips_order\": 0,\n  \"hips_tile_width\": 512,\n"
      "  \"data_type\": \"float32\",\n  \"n_leaf_tiles\": 1,\n"
      "  \"products\": [\"signal\"]\n}\n";
  std::ofstream m(fs::path(root + "/manifest.json"), std::ios::binary);
  if (!m) return false;
  m << body;
  return true;
}

bool write_signal_hips(const std::string& root) {
  const std::string root_posix = fs::path(root).generic_string();
  std::error_code ec;
  fs::create_directories(fs::path(root_posix + "/signal/Norder0/Dir0"), ec);
  if (ec) return false;
  std::ofstream p(fs::path(root_posix + "/signal/properties"), std::ios::binary);
  if (!p) return false;
  p << hips_properties_text();
  p.close();
  if (!write_completion_manifest(root_posix)) return false;
  float v = kSigVal;
  return p1sess::write_fits_file(root_posix + "/signal/Norder0/Dir0/Npix0.fits",
                                 512, 512, const_px, &v) == 0;
}

struct Fixture {
  fs::path root;
  std::string hips;
  std::string out;
  double ra = 0, dec = 0;
};

Fixture make_fixture(const std::string& tag, int w, int h) {
  Fixture fx;
  fx.root = fs::temp_directory_path() /
            ("p3stream_" + tag + "_" + std::to_string(::getpid()));
  std::error_code ec;
  fs::remove_all(fx.root, ec);
  fs::create_directories(fx.root, ec);
  fx.hips = (fx.root / "hips").generic_string();
  fx.out = (fx.root / "out").generic_string();
  fs::create_directories(fx.out, ec);
  char rid[32];
  std::snprintf(rid, sizeof(rid), "%012lx",
                static_cast<unsigned long>(::getpid()) & 0xffffffffffUL);
  if (!write_run_context(fx.out, rid, ASTROCS_VERSION_STRING, ASTROCS_COMMIT_SHA).ok())
    std::fprintf(stderr, "WARN: run_context write failed\n");
  if (!write_signal_hips(fx.hips)) std::fprintf(stderr, "WARN: hips fixture failed\n");
  astrocs::healpix::pix2ang_nest(512u, 131072ull, fx.ra, fx.dec);
  (void)w; (void)h;
  return fx;
}

std::string node_config(const Fixture& fx, int w, int h, int sub_block_px) {
  char buf[2048];
  std::snprintf(buf, sizeof(buf),
                R"({
  "source": {"hips_dir": "%s"},
  "center": {"ra_deg": %.12f, "dec_deg": %.12f},
  "scale_deg_per_px": 0.01,
  "width_px": %d, "height_px": %d,
  "sampler": "nearest",
  "longitude_parity": "east_left",
  "bitpix": -32,
  "output_mode": "surface_brightness",
  "sub_block_px": %d,
  "output_dir": "%s"
})",
                fx.hips.c_str(), fx.ra, fx.dec, w, h, sub_block_px, fx.out.c_str());
  return std::string(buf);
}
}  // namespace
namespace {

// 生产 phase3 五节点链（唯一真实路径；与 p3002_real_nodes_test 同规则）。
struct ChainResult {
  bool ok = false;
  std::string error;
  json writer_man;
  json resample_man;
  json verify_man;
};

ChainResult run_chain(ModuleRegistry& reg, const std::string& cfg, int sub_block_px) {
  ChainResult cr;
  const char* kIds[] = {"astrocs.phase3.properties", "astrocs.phase3.wcs",
                        "astrocs.phase3.resample2", "astrocs.phase3.writer",
                        "astrocs.phase3.verify"};
  for (const char* id : kIds) {
    auto m = reg.create(id);
    if (m.failed()) { cr.error = std::string(id) + ": create failed"; return cr; }
    auto v = m.value()->validate_config(cfg);
    if (v.failed()) { cr.error = std::string(id) + ": validate: " + v.error().message(); return cr; }
    auto p = m.value()->plan(std::string("node_") + id, cfg);
    if (p.failed()) { cr.error = std::string(id) + ": plan failed"; return cr; }
    RunContext ctx;
    auto r = m.value()->execute(ctx);
    if (r.failed()) { cr.error = std::string(id) + ": execute: " + r.error().message(); return cr; }
    auto man = m.value()->last_manifest();
    if (!man.ok()) { cr.error = std::string(id) + ": no manifest"; return cr; }
    json j;
    try { j = json::parse(man.value()); } catch (...) { cr.error = std::string(id) + ": bad manifest"; return cr; }
    if (std::strcmp(id, "astrocs.phase3.writer") == 0) cr.writer_man = j;
    if (std::strcmp(id, "astrocs.phase3.resample2") == 0) cr.resample_man = j;
    if (std::strcmp(id, "astrocs.phase3.verify") == 0) cr.verify_man = j;
  }
  (void)sub_block_px;
  cr.ok = true;
  return cr;
}

std::string read_all_bytes(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(f)),
                     std::istreambuf_iterator<char>());
}

// ── 单配置实测（子进程内执行）────────────────────────────────────────────
struct Meas {
  int w = 0, h = 0, sb = 0;
  bool fault = false;
  long peak_kb = -1;
  long baseline_kb = -1;
  long delta_kb = -1;
  bool chain_ok = false;
  std::string error;
};

Meas measure_once(int w, int h, int sb, bool fault) {
  Meas m;
  m.w = w; m.h = h; m.sb = sb; m.fault = fault;
  if (fault) ::setenv("ASTROCS_P3_EXPORT_FAULT", "whole_frame_resident", 1);
  else ::unsetenv("ASTROCS_P3_EXPORT_FAULT");
  ModuleRegistry reg;
  auto rr = register_phase_modules(reg);
  if (rr.failed()) { m.error = "register failed"; return m; }
  Fixture fx = make_fixture("m" + std::to_string(w) + "x" + std::to_string(h) + "_" +
                            std::to_string(sb) + (fault ? "_f" : ""), w, h);
  const std::string cfg = node_config(fx, w, h, sb);
  // 基线（链执行前）与峰值（链执行后）——峰值经内核记账，非采样
  if (!rss_reset_peak()) { m.error = "clear_refs unavailable (VmHWM reset unsupported)"; return m; }
  m.baseline_kb = rss_peak_kb();
  ChainResult cr = run_chain(reg, cfg, sb);
  m.peak_kb = rss_peak_kb();
  m.delta_kb = (m.peak_kb >= 0 && m.baseline_kb >= 0) ? (m.peak_kb - m.baseline_kb) : -1;
  m.chain_ok = cr.ok;
  if (!cr.ok) m.error = cr.error;
  std::error_code ec;
  fs::remove_all(fx.root, ec);
  return m;
}

#ifndef _WIN32
// 独立子进程测量：避免分配器复用与线程残留污染峰值（fork + exec 本测试自身）。
bool measure_isolated(int w, int h, int sb, bool fault, Meas* out) {
  int fds[2];
  if (::pipe(fds) != 0) return false;
  const pid_t pid = ::fork();
  if (pid < 0) { ::close(fds[0]); ::close(fds[1]); return false; }
  if (pid == 0) {
    ::dup2(fds[1], 1);
    ::close(fds[0]);
    ::close(fds[1]);
    const std::string a = std::to_string(w), b = std::to_string(h),
                      c = std::to_string(sb), d = fault ? "1" : "0";
    ::execl("/proc/self/exe", "p3_export_stream_prod_test", "--measure",
            a.c_str(), b.c_str(), c.c_str(), d.c_str(), (char*)nullptr);
    ::_exit(127);
  }
  ::close(fds[1]);
  std::string buf;
  char tmp[4096];
  ssize_t n = 0;
  while ((n = ::read(fds[0], tmp, sizeof(tmp))) > 0) buf.append(tmp, (std::size_t)n);
  ::close(fds[0]);
  int st = 0;
  ::waitpid(pid, &st, 0);
  if (buf.empty()) return false;
  try {
    const json j = json::parse(buf);
    out->w = j.value("w", 0);
    out->h = j.value("h", 0);
    out->sb = j.value("sb", 0);
    out->fault = j.value("fault", false);
    out->peak_kb = j.value("peak_kb", -1L);
    out->baseline_kb = j.value("baseline_kb", -1L);
    out->delta_kb = j.value("delta_kb", -1L);
    out->chain_ok = j.value("chain_ok", false);
    out->error = j.value("error", std::string());
  } catch (...) {
    return false;
  }
  return true;
}
#endif

}  // namespace

namespace {

// 产品语义对照：流式 vs 整幅驻留参考路径（同配置、同夹具）⇒ 逐字节相同。
void test_product_identity() {
  const int W = 512, H = 512, SB = 128;
  std::string sha_stream, sha_frame, canon_stream, canon_frame, bytes_stream, bytes_frame;
  for (int pass = 0; pass < 2; ++pass) {
    const bool fault = (pass == 1);
    if (fault) ::setenv("ASTROCS_P3_EXPORT_FAULT", "whole_frame_resident", 1);
    else ::unsetenv("ASTROCS_P3_EXPORT_FAULT");
    ModuleRegistry reg;
    CHECK(register_phase_modules(reg).ok());
    Fixture fx = make_fixture(fault ? "ident_f" : "ident_s", W, H);
    const std::string cfg = node_config(fx, W, H, SB);
    ChainResult cr = run_chain(reg, cfg, SB);
    CHECK_MSG(cr.ok, ("product identity chain failed: " + cr.error).c_str());
    const std::string fits = fx.out + "/output_phase3.fits";
    const std::string wr = read_all_bytes(fx.out + "/p3_writer.json");
    json wj;
    try { wj = json::parse(wr); } catch (...) { wj = json::object(); }
    if (fault) {
      sha_frame = wj.value("integrity_sha256", std::string());
      canon_frame = wj.value("canonical_sha256", std::string());
      bytes_frame = read_all_bytes(fits);
      CHECK_MSG(cr.writer_man.value("whole_frame_fault", false),
                "fault injection must be visible in writer manifest");
    } else {
      sha_stream = wj.value("integrity_sha256", std::string());
      canon_stream = wj.value("canonical_sha256", std::string());
      bytes_stream = read_all_bytes(fits);
      CHECK_MSG(!cr.writer_man.value("whole_frame_fault", true),
                "production default path must NOT be the whole-frame fault path");
      CHECK_MSG(cr.writer_man.contains("export_stream") &&
                    cr.writer_man["export_stream"].value("scheduler", std::string()) ==
                        "ExportStreamScheduler",
                "production writer manifest must declare ExportStreamScheduler");
      CHECK_MSG(cr.writer_man["export_stream"].value("sub_blocks", 0) > 0,
                "production writer manifest must report sub-block count > 0");
      CHECK_MSG(cr.writer_man.value("sub_block_px", 0) == SB,
                "production writer manifest must report configured sub_block_px");
      CHECK_MSG(cr.resample_man.value("sub_block_streaming", false),
                "resample node must declare sub-block streaming");
      CHECK_MSG(cr.verify_man.value("sub_block_streaming", false),
                "verify node must declare sub-block streaming");
    }
    std::error_code ec;
    fs::remove_all(fx.root, ec);
  }
  ::unsetenv("ASTROCS_P3_EXPORT_FAULT");
  CHECK_MSG(!sha_stream.empty() && !sha_frame.empty(),
            "both paths must report integrity sha256");
  CHECK_MSG(sha_stream == sha_frame,
            ("FITS integrity sha256 must be identical: streaming=" + sha_stream +
             " whole_frame=" + sha_frame).c_str());
  CHECK_MSG(canon_stream == canon_frame,
            ("canonical sha256 must be identical: streaming=" + canon_stream +
             " whole_frame=" + canon_frame).c_str());
  CHECK_MSG(bytes_stream == bytes_frame,
            ("FITS bytes must be identical (streaming " +
             std::to_string(bytes_stream.size()) + " B vs whole-frame " +
             std::to_string(bytes_frame.size()) + " B)").c_str());
  std::printf("INFO product identity: integrity=%s canonical=%s bytes=%zu\n",
              sha_stream.c_str(), canon_stream.c_str(), bytes_stream.size());
  std::error_code ec;
  fs::create_directories(P3STREAM_EVIDENCE_DIR, ec);
  {
    std::ofstream f(std::string(P3STREAM_EVIDENCE_DIR) + "/p3_product_identity.txt");
    if (f) {
      f << "integrity_sha256=" << sha_stream << "\n";
      f << "canonical_sha256=" << canon_stream << "\n";
      f << "fits_bytes=" << bytes_stream.size() << "\n";
      f << "streaming_vs_whole_frame: integrity/canonical/bytes all identical\n";
    }
  }
}

// ── 判据 ②③④：峰值 RSS 实测扫描 ─────────────────────────────────────────
void test_rss_criteria() {
#ifdef _WIN32
  std::fprintf(stderr, "CHECK failed: RSS criteria require Linux /proc accounting\n");
  ++failures;
#else
  std::error_code ec;
  fs::create_directories(P3STREAM_EVIDENCE_DIR, ec);
  std::ofstream csv(std::string(P3STREAM_EVIDENCE_DIR) + "/p3_stream_rss.csv");
  if (csv) csv << "width,height,sub_block_px,fault,baseline_kb,peak_kb,delta_kb,chain_ok\n";
  auto run = [&](int w, int h, int sb, bool fault) -> Meas {
    Meas m;
    const bool got = measure_isolated(w, h, sb, fault, &m);
    CHECK_MSG(got, ("isolated measurement failed for " + std::to_string(w) + "x" +
                    std::to_string(h) + " sb=" + std::to_string(sb)).c_str());
    CHECK_MSG(m.chain_ok, ("chain failed: " + m.error).c_str());
    CHECK_MSG(m.delta_kb >= 0, "VmHWM accounting unavailable");
    if (csv)
      csv << w << "," << h << "," << sb << "," << (fault ? 1 : 0) << ","
          << m.baseline_kb << "," << m.peak_kb << "," << m.delta_kb << ","
          << (m.chain_ok ? 1 : 0) << "\n";
    std::printf("INFO rss w=%d h=%d sb=%d fault=%d baseline=%ldkB peak=%ldkB delta=%ldkB\n",
                w, h, sb, fault ? 1 : 0, m.baseline_kb, m.peak_kb, m.delta_kb);
    return m;
  };
  const Meas small_sb = run(1024, 1024, 64, false);
  const Meas large_sb = run(1024, 1024, 256, false);
  const Meas area_1x = run(512, 512, 256, false);
  const Meas area_4x = run(1024, 1024, 256, false);
  const Meas frame_1x = run(512, 512, 256, true);
  const Meas frame_4x = run(1024, 1024, 256, true);
  csv.flush();

  // ② 峰值随 sub_block 变化（子块 4× 边长 ⇒ 单块缓冲 16×）
  CHECK_MSG(large_sb.delta_kb > small_sb.delta_kb,
            ("peak RSS must grow with sub_block_px: sb=64 " +
             std::to_string(small_sb.delta_kb) + "kB vs sb=256 " +
             std::to_string(large_sb.delta_kb) + "kB").c_str());
  // ③ 峰值与总图大小无关（面积 4× ⇒ 峰值比值必须远小于面积比）
  const double ratio_stream =
      (area_1x.delta_kb > 0)
          ? static_cast<double>(area_4x.delta_kb) / static_cast<double>(area_1x.delta_kb)
          : 1e9;
  CHECK_MSG(ratio_stream < 2.0,
            ("streaming peak RSS must be independent of W*H: 512^2=" +
             std::to_string(area_1x.delta_kb) + "kB 1024^2=" +
             std::to_string(area_4x.delta_kb) + "kB ratio=" +
             std::to_string(ratio_stream)).c_str());
  // ④ 阳性对照：整幅驻留路径必须随面积线性增长（证明本度量非退化）
  const double ratio_frame =
      (frame_1x.delta_kb > 0)
          ? static_cast<double>(frame_4x.delta_kb) / static_cast<double>(frame_1x.delta_kb)
          : 0.0;
  CHECK_MSG(ratio_frame >= 2.5,
            ("whole-frame residency MUST be detected by this metric (non-degenerate): "
             "512^2=" + std::to_string(frame_1x.delta_kb) + "kB 1024^2=" +
             std::to_string(frame_4x.delta_kb) + "kB ratio=" +
             std::to_string(ratio_frame)).c_str());
  CHECK_MSG(frame_4x.delta_kb > area_4x.delta_kb,
            ("whole-frame reference must be heavier than streaming at 1024^2: " +
             std::to_string(frame_4x.delta_kb) + "kB vs " +
             std::to_string(area_4x.delta_kb) + "kB").c_str());
  std::printf("INFO rss ratios: streaming(4x area)=%.2f whole_frame(4x area)=%.2f\n",
              ratio_stream, ratio_frame);
#endif
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 6 && std::strcmp(argv[1], "--measure") == 0) {
    const int w = std::atoi(argv[2]);
    const int h = std::atoi(argv[3]);
    const int sb = std::atoi(argv[4]);
    const bool fault = (std::atoi(argv[5]) != 0);
    const Meas m = measure_once(w, h, sb, fault);
    json j = json::object();
    j["w"] = m.w; j["h"] = m.h; j["sb"] = m.sb; j["fault"] = m.fault;
    j["peak_kb"] = m.peak_kb; j["baseline_kb"] = m.baseline_kb;
    j["delta_kb"] = m.delta_kb; j["chain_ok"] = m.chain_ok;
    j["error"] = m.error;
    std::printf("%s\n", j.dump().c_str());
    std::fflush(stdout);
    return m.chain_ok ? 0 : 1;
  }
  std::printf("P3-STREAM-01 production export sub-block streaming gate\n");
  test_product_identity();
  test_rss_criteria();
  if (failures == 0) {
    std::printf("P3-STREAM-01 PASS (production export streams sub-blocks; RSS "
                "independent of W*H and scaling with sub_block_px)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-STREAM-01 FAIL (%d)\n", failures);
  return 1;
}
