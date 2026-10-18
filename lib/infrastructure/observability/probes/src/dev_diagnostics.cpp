// lib/infrastructure/observability/probes/src/dev_diagnostics.cpp
// 开发诊断 sink 实现。**只在 ACSD_DEV_MODE=1 时参与编译**（CMake 侧 OFF 时本
// TU 不进任何 target，二进制中无 acsd::devdiag::* 符号）。
//
// 依据: docs/engineering/architecture/DEV_MODE.md §4–§7；
//       docs/engineering/standards/DIAGNOSTIC_PROBES.md §2/§3/§5/§6/§7。
//
// 科学性边界（实现层复查）:
//   · 本文件只**观察**调用点给出的只读 view，从不读回、从不被科学模块读取；
//   · 不用快照填充任何算法输入；不存在"复制最终产物冒充中间节点快照"的路径
//     （文件内容直接来自调用点缓冲，且 meta 带 producer/upstream_product）；
//   · 失败只影响诊断计数，不影响科学退出码与科学产物；
//   · 落盘经 aio 原语（aio_atomic::write_file_atomic / make_dirs / remove_tree），
//     FITS 图像经 aio 的 fits_write_file 写私有暂存名后原子 rename。
#include "acsd/dev_diagnostics.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "aio_atomic_file.h"
#include "aio_file_io.h"
#include "aio_fits.h"
#include "crypto/sha256.h"
#include <nlohmann/json.hpp>

namespace acsd {
namespace devdiag {

namespace {

using Json = nlohmann::json;

// ── 探针注册表（白名单；probe_id 稳定，改被估计量必须换 ID）─────────────────
// 登记表字段与 DIAGNOSTIC_PROBES.md §2 逐条对应。未登记的 probe_id 一律
// NOT_SELECTED（不得把任意表达式/指针地址当通用 dump 请求）。
struct ProbeEntry {
  const char* probe_id;
  const char* kind;
  const char* semantic_hint;   // 仅供 debug_manifest 的登记表快照，不替代调用点声明
  const char* default_unit;
  CoordSpace default_coord;
};

const ProbeEntry kRegistry[] = {
    // 阶段一
    {"normalize.noise.reference_snr", "snapshot/table",
     "reference point-source flux variance V_F = (F_ref/SNR_ref)^2 and its SNR",
     "[selected_flux_unit]^2", CoordSpace::Icrs},
    {"normalize.drizzle.signal_tile", "snapshot/image",
     "drizzled surface brightness S_p and its geometric denominator N_p",
     "BUNIT", CoordSpace::HipsTileFitsRowmajor},
    {"normalize.drizzle.geometry", "snapshot/table",
     "drop/leaf overlap a_jp, A_drop, source solid angle Omega_j",
     "sr", CoordSpace::HealpixNested},
    // 阶段二
    {"mosaic.snr.input_controls", "snapshot/table",
     "sparse reference-SNR control points actually read by phase 2",
     "[selected_flux_unit]^2", CoordSpace::Icrs},
    {"mosaic.snr.reconstruction", "snapshot/model",
     "reconstructed dense reference precision field V_F on the query ROI",
     "[selected_flux_unit]^2", CoordSpace::HealpixNested},
    {"mosaic.weight.surface_precision", "snapshot/image",
     "surface-brightness inverse variance w_B = 1/V_B with transfer factor T",
     "1/[B]^2", CoordSpace::HealpixNested},
    {"mosaic.sky.relative_field", "snapshot/table",
     "per-connected-component additive background offsets and residuals",
     "background_surface_brightness_unit", CoordSpace::Icrs},
    {"mosaic.sky.normal_matrix", "snapshot/model",
     "Schur-reduced penalized normal equation of the P5 sky plane "
     "(H_data/H_red/H_solve/rhs/Q, D stencil index, solve branch, pivots and "
     "original-equation backward error)",
     "JSON body states each matrix unit", CoordSpace::None},
    {"mosaic.rejection.sample_mask", "snapshot/image",
     "per-sample rejection eligibility and reason summary",
     "boolean/reason", CoordSpace::HealpixNested},
    {"mosaic.integration.signal_tile", "snapshot/image",
     "weighted surface-brightness signal before the final tile is emitted",
     "BUNIT", CoordSpace::HealpixNested},
    // 阶段三
    {"export.resample.footprint", "snapshot/table",
     "target WCS position, input leaf, effective kernel weights and coverage",
     "sr or dimensionless", CoordSpace::TargetWcsPixel},
    {"export.fits.signal_tile", "snapshot/image",
     "un-stretched signal region actually written to the output FITS",
     "BUNIT", CoordSpace::TargetWcsPixel},
};

const ProbeEntry* find_probe(const char* id) {
  if (!id) return nullptr;
  for (const ProbeEntry& e : kRegistry) {
    if (std::strcmp(e.probe_id, id) == 0) return &e;
  }
  return nullptr;
}

// ── 全局状态（进程内单例）──────────────────────────────────────────────────
struct Budget {
  // 选择器（只控制观测多少）。默认：性能汇总允许，大数组快照**不**自动全量落盘
  // （DEV_MODE.md §2）。由 ACSD_DEV_SELECTOR 指向的 JSON 文件覆盖。
  bool perf_enabled = true;
  bool snapshot_enabled = false;
  std::vector<std::string> probe_ids;      // 空 = 已登记表内全部（受 snapshot_enabled 约束）
  std::vector<std::string> frame_ids;      // 空 = 不按帧筛选
  std::vector<std::string> tile_ids;       // 空 = 不按 tile 筛选
  std::size_t max_frames = 2;
  std::size_t max_tiles = 4;
  std::uint64_t max_total_bytes = 1073741824ull;      // 1 GiB
  std::uint64_t max_inflight_bytes = 67108864ull;     // 64 MiB
};

struct RunState {
  bool begun = false;
  std::string root;          // <work_root>/_dev_diagnostics/<run_id>
  std::string run_id;
  std::string git_commit;
  std::string source_digest;
  Budget budget;
  std::vector<Json> entries;
  std::unordered_map<std::string, std::size_t> per_probe_count;   // probe_id -> 已写次数
  std::unordered_map<std::string, std::size_t> per_frame_count;   // frame_id -> 已写次数
  std::unordered_map<std::string, std::size_t> per_tile_count;    // tile     -> 已写次数
  std::size_t written = 0;
  std::size_t errors = 0;
};

RunState& state() {
  static RunState s;
  return s;
}

std::mutex& mu() {
  static std::mutex m;
  return m;
}

// ── 文本工具 ───────────────────────────────────────────────────────────────
const char* env_str(const char* name) {
  const char* p = std::getenv(name);
  return (p && *p) ? p : nullptr;
}

// 安全短标识：禁止用户输入构造 "../"、绝对路径或符号链接逃逸
// （DIAGNOSTIC_PROBES.md §7）。不合格字符一律替换为 '_'。
std::string safe_segment(const char* s) {
  if (!s || !*s) return std::string("unnamed");
  std::string out;
  out.reserve(std::strlen(s));
  for (const char* p = s; *p; ++p) {
    const unsigned char c = static_cast<unsigned char>(*p);
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
    out.push_back(ok ? static_cast<char>(c) : '_');
  }
  if (out == "." || out == "..") return std::string("unnamed");
  return out;
}

void ensure_dir(const std::string& path) {
  if (aio_atomic::path_exists(path, nullptr) != 0) return;
  (void)aio_atomic::make_dirs(path);
}

std::string read_text(const std::string& path) {
  std::string out;
  if (!aio_file::read_all(path.c_str(), &out)) return std::string();
  return out;
}

// ── 选择器解析（失败 ⇒ 保持默认，不因此改变科学逻辑）──────────────────────
void load_selector(const std::string& selector_path) {
  const std::string text = read_text(selector_path);
  if (text.empty()) return;
  Json j;
  try {
    j = Json::parse(text);
  } catch (...) {
    return;  // 诊断选择器损坏 ⇒ 用默认；不得影响科学任务
  }
  try {
    if (j.contains("perf") && j["perf"].is_object())
      state().budget.perf_enabled = j["perf"].value("enabled", true);
    if (j.contains("snapshots") && j["snapshots"].is_object()) {
      const Json& s = j["snapshots"];
      state().budget.snapshot_enabled = s.value("enabled", false);
      if (s.contains("probe_ids") && s["probe_ids"].is_array()) {
        for (const auto& v : s["probe_ids"])
          if (v.is_string()) state().budget.probe_ids.push_back(v.get<std::string>());
      }
      if (s.contains("frame_ids") && s["frame_ids"].is_array()) {
        for (const auto& v : s["frame_ids"])
          if (v.is_string()) state().budget.frame_ids.push_back(v.get<std::string>());
      }
      if (s.contains("tile_ids") && s["tile_ids"].is_array()) {
        for (const auto& v : s["tile_ids"])
          if (v.is_string()) state().budget.tile_ids.push_back(v.get<std::string>());
      }
      state().budget.max_frames = s.value("max_frames", state().budget.max_frames);
      state().budget.max_tiles = s.value("max_tiles", state().budget.max_tiles);
      state().budget.max_total_bytes =
          s.value("max_total_bytes", state().budget.max_total_bytes);
      state().budget.max_inflight_bytes =
          s.value("max_inflight_bytes", state().budget.max_inflight_bytes);
    }
  } catch (...) {
    // 部分字段损坏 ⇒ 其余保持默认
  }
}

bool in_list(const std::vector<std::string>& v, const char* s) {
  if (v.empty()) return true;              // 空 = 不按该项筛选
  if (!s) return false;
  for (const std::string& e : v) if (e == s) return true;
  return false;
}

}  // namespace

// ── 对外：登记词面 ──────────────────────────────────────────────────────────
const char* coord_space_token(CoordSpace c) {
  switch (c) {
    case CoordSpace::DetectorPixel: return "detector_pixel";
    case CoordSpace::Icrs: return "ICRS";
    case CoordSpace::HealpixNested: return "HEALPix_NESTED";
    case CoordSpace::HipsTileFitsRowmajor: return "HiPS_tile_FITS_rowmajor";
    case CoordSpace::TargetWcsPixel: return "target_WCS_pixel";
    case CoordSpace::None: return "none";
  }
  return "none";
}

// ── 对外：运行期 ───────────────────────────────────────────────────────────
bool begin_run(const char* work_root, const char* run_id,
               const char* git_commit, const char* source_digest) {
  std::lock_guard<std::mutex> lock(mu());
  RunState& s = state();
  if (s.begun) return true;   // 已建立（幂等）
  if (!work_root || !*work_root || !run_id || !*run_id) return false;
  s.root = std::string(work_root) + "/_dev_diagnostics/" + safe_segment(run_id);
  s.run_id = safe_segment(run_id);
  s.git_commit = git_commit ? git_commit : "";
  s.source_digest = source_digest ? source_digest : "";
  if (aio_atomic::make_dirs(s.root) != 0 ||
      aio_atomic::make_dirs(s.root + "/snapshots") != 0 ||
      aio_atomic::make_dirs(s.root + "/perf") != 0) {
    s.errors += 1;
    return false;
  }
  if (const char* sel = env_str("ACSD_DEV_SELECTOR")) load_selector(sel);
  s.begun = true;
  return true;
}

void finish_run() {
  std::lock_guard<std::mutex> lock(mu());
  RunState& s = state();
  if (!s.begun) return;

  Json registry = Json::array();
  for (const ProbeEntry& e : kRegistry) {
    registry.push_back(Json{{"probe_id", e.probe_id},
                            {"kind", e.kind},
                            {"semantic_hint", e.semantic_hint},
                            {"default_unit", e.default_unit},
                            {"coord_space", coord_space_token(e.default_coord)}});
  }

  Json man = Json{
      {"schema", "acsd.dev-diagnostics/v1"},
      {"non_scientific_product", true},
      {"run_id", s.run_id},
      {"build", Json{{"dev_mode", true},
                     {"git_commit", s.git_commit},
                     {"source_digest", s.source_digest}}},
      {"selection", Json{{"snapshot_enabled", s.budget.snapshot_enabled},
                         {"perf_enabled", s.budget.perf_enabled},
                         {"max_frames", s.budget.max_frames},
                         {"max_tiles", s.budget.max_tiles},
                         {"max_total_bytes", s.budget.max_total_bytes},
                         {"max_inflight_bytes", s.budget.max_inflight_bytes}}},
      {"probe_registry", registry},
      {"perf_sink", env_str("ACSD_PROBE_LOG") ? Json(env_str("ACSD_PROBE_LOG"))
                                              : Json(nullptr)},
      {"counts", Json{{"written", s.written}, {"io_error", s.errors}}},
      {"entries", s.entries},
  };
  const std::string path = s.root + "/debug_manifest.json";
  std::string err;
  if (aio_atomic::write_file_atomic(path, man.dump(2) + "\n", &err) != 0)
    s.errors += 1;   // 诊断 manifest 自身写失败也只记诊断

  // 命令行可见的一行摘要（诊断运行日志，不替代正式运行记录）。
  std::fprintf(stderr,
               "[dev_diag] run=%s snapshots_written=%zu diag_io_error=%zu root=%s\n",
               s.run_id.c_str(), s.written, s.errors, s.root.c_str());
}

bool selected(const char* probe_id) {
  std::lock_guard<std::mutex> lock(mu());
  RunState& s = state();
  if (!s.begun) return false;
  if (!s.budget.snapshot_enabled) return false;
  if (find_probe(probe_id) == nullptr) return false;   // 未登记 ⇒ 不 dump
  if (!in_list(s.budget.probe_ids, probe_id)) return false;
  return true;
}

namespace {

// 记一条状态行（进 debug_manifest）。status 必须是稳定词面。
void record(const SnapshotMeta& meta, const char* status, const char* file_path,
            const std::string& sha, std::uint64_t bytes, const char* note) {
  RunState& s = state();
  Json e = Json{
      {"probe_id", meta.probe_id ? meta.probe_id : ""},
      {"stage", meta.stage ? meta.stage : ""},
      {"producer", meta.producer ? meta.producer : ""},
      {"semantic", meta.semantic ? meta.semantic : ""},
      {"unit", meta.unit ? meta.unit : ""},
      {"coord_space", coord_space_token(meta.coord_space)},
      {"dtype", meta.dtype ? meta.dtype : ""},
      {"shape", meta.shape ? meta.shape : ""},
      {"status", status ? status : "IO_ERROR"},
  };
  if (meta.observation_boundary)
    e["observation_boundary"] = meta.observation_boundary;
  if (meta.valid_support) e["valid_support"] = meta.valid_support;
  if (meta.validity_policy) e["validity_policy"] = meta.validity_policy;
  if (meta.frame_id) e["frame_id"] = meta.frame_id;
  if (meta.tile_note) e["tile"] = meta.tile_note;
  if (meta.upstream_product) e["upstream_product"] = meta.upstream_product;
  if (file_path) e["file"] = file_path;
  if (!sha.empty()) e["sha256"] = sha;
  if (bytes != 0) e["bytes"] = bytes;
  if (note) e["note"] = note;
  s.entries.push_back(std::move(e));
  if (std::strcmp(status, "WRITTEN") == 0) s.written += 1;
  else s.errors += 1;
}

// 选择器/预算闸门。返回空串 = 允许写；否则返回状态词面。
const char* gate(const SnapshotMeta& meta) {
  RunState& s = state();
  if (!s.begun) return "IO_ERROR";
  if (!s.budget.snapshot_enabled) return "NOT_SELECTED";
  if (find_probe(meta.probe_id) == nullptr) return "NOT_SELECTED";
  if (!in_list(s.budget.probe_ids, meta.probe_id)) return "NOT_SELECTED";
  if (!in_list(s.budget.frame_ids, meta.frame_id)) return "NOT_SELECTED";
  if (!in_list(s.budget.tile_ids, meta.tile_note)) return "NOT_SELECTED";
  // 每探针/每帧/每 tile 次数上限（确定性选择，破坏两次运行对照的组合不采样）
  if (s.per_probe_count[meta.probe_id] >= 4096) return "SKIPPED_BUDGET";
  if (meta.frame_id && s.per_frame_count[meta.frame_id] >= s.budget.max_frames)
    return "SKIPPED_BUDGET";
  if (meta.tile_note && s.per_tile_count[meta.tile_note] >= s.budget.max_tiles)
    return "SKIPPED_BUDGET";
  return nullptr;
}

void bump(const SnapshotMeta& meta) {
  RunState& s = state();
  s.per_probe_count[meta.probe_id] += 1;
  if (meta.frame_id) s.per_frame_count[meta.frame_id] += 1;
  if (meta.tile_note) s.per_tile_count[meta.tile_note] += 1;
}

// 写旁挂 meta JSON（与数值文件同 stem）。失败不影响数值文件本身。
void write_sidecar(const std::string& meta_path, const SnapshotMeta& meta,
                   const char* representation) {
  Json m = Json{
      {"probe_id", meta.probe_id ? meta.probe_id : ""},
      {"schema", "acsd.dev-snapshot-meta/v1"},
      {"non_scientific_product", true},
      {"stage", meta.stage ? meta.stage : ""},
      {"producer", meta.producer ? meta.producer : ""},
      {"semantic", meta.semantic ? meta.semantic : ""},
      {"unit", meta.unit ? meta.unit : ""},
      {"coord_space", coord_space_token(meta.coord_space)},
      {"dtype", meta.dtype ? meta.dtype : ""},
      {"shape", meta.shape ? meta.shape : ""},
      {"representation", representation},
      {"endianness", "little"},
      {"roi", Json{{"row0", meta.roi_row0}, {"col0", meta.roi_col0},
                   {"rows", meta.roi_rows}, {"cols", meta.roi_cols}}},
  };
  if (meta.observation_boundary) m["observation_boundary"] = meta.observation_boundary;
  if (meta.valid_support) m["valid_support"] = meta.valid_support;
  if (meta.validity_policy) m["validity_policy"] = meta.validity_policy;
  if (meta.frame_id) m["frame_id"] = meta.frame_id;
  if (meta.tile_note) m["tile"] = meta.tile_note;
  if (meta.upstream_product) m["upstream_product"] = meta.upstream_product;
  if (meta.extra_json) {
    try { m["extra"] = Json::parse(meta.extra_json); } catch (...) {}
  }
  std::string err;
  (void)aio_atomic::write_file_atomic(meta_path, m.dump(2) + "\n", &err);
}

std::string sha256_of(const std::string& path) {
  std::string hex;
  if (aio_file::sha256_hex(path.c_str(), &hex)) return hex;
  return std::string();
}

}  // namespace

const char* snapshot_image_f32(const SnapshotMeta& meta,
                               int rows, int cols, const float* data) {
  std::lock_guard<std::mutex> lock(mu());
  RunState& s = state();
  if (!s.begun) { record(meta, "IO_ERROR", nullptr, "", 0, "run not begun"); return "IO_ERROR"; }
  if (!data || rows <= 0 || cols <= 0) {
    record(meta, "INVALID_VIEW", nullptr, "", 0, "null buffer or non-positive shape");
    return "INVALID_VIEW";
  }
  if (const char* st = gate(meta)) { record(meta, st, nullptr, "", 0, nullptr); return st; }

  // ROI 归一（越界 ⇒ INVALID_VIEW, 不静默裁剪）
  const int rn = (meta.roi_rows > 0) ? meta.roi_rows : rows;
  const int cn = (meta.roi_cols > 0) ? meta.roi_cols : cols;
  const int r0 = (meta.roi_rows > 0) ? meta.roi_row0 : 0;
  const int c0 = (meta.roi_cols > 0) ? meta.roi_col0 : 0;
  if (r0 < 0 || c0 < 0 || rn <= 0 || cn <= 0 || r0 + rn > rows || c0 + cn > cols) {
    record(meta, "INVALID_VIEW", nullptr, "", 0, "ROI outside buffer");
    return "INVALID_VIEW";
  }
  const std::size_t n = static_cast<std::size_t>(rn) * static_cast<std::size_t>(cn);
  const std::uint64_t bytes = static_cast<std::uint64_t>(n) * sizeof(float);
  if (bytes > s.budget.max_inflight_bytes) {
    record(meta, "SKIPPED_BUDGET", nullptr, "", bytes,
           "single view exceeds max_inflight_bytes");
    return "SKIPPED_BUDGET";
  }
  if (bytes > s.budget.max_total_bytes) {
    record(meta, "SKIPPED_BUDGET", nullptr, "", bytes,
           "single view exceeds max_total_bytes");
    return "SKIPPED_BUDGET";
  }

  // 确定性序号：同一 probe 身份重复写不覆盖先前采集版本（§7）。
  const std::size_t seq = s.per_probe_count[meta.probe_id];
  const std::string base = s.root + "/snapshots/" + safe_segment(meta.probe_id) +
                           "/" + safe_segment(meta.frame_id ? meta.frame_id : "noframe") +
                           "/" + std::to_string(seq);
  ensure_dir(s.root + "/snapshots/" + safe_segment(meta.probe_id) + "/" +
             safe_segment(meta.frame_id ? meta.frame_id : "noframe"));

  // 数值文件：写私有暂存名 → 校验 → 原子改名（§7；掉电/取消留下 incomplete 状态）。
  const std::string fits_final = base + ".fits";
  const std::string fits_tmp = base + ".fits.incomplete";
  AIOImageData img;
  std::memset(&img, 0, sizeof(img));
  img.dtype = 0;               // FP32（与源数组同型；禁静默降精度）
  img.width = cn;
  img.height = rn;
  img.channels = 1;
  img.bits_per_sample = 32;
  img.float_sample = 1;
  std::snprintf(img.source_format, sizeof(img.source_format), "%s", "FITS");
  std::snprintf(img.source_path, sizeof(img.source_path), "%s", fits_tmp.c_str());
  // FITS 是行主序连续缓冲 ⇒ 先把 ROI 收集到连续缓冲（ROI 命中调用点已有数组,
  // 无额外科学计算; ROI=全幅时仍做一次受控拷贝, 因为 writer 后紧接可能回收）。
  std::vector<float> roi;
  if (r0 == 0 && c0 == 0 && rn == rows && cn == cols) {
    img.data = const_cast<float*>(data);
  } else {
    roi.resize(n);
    for (int r = 0; r < rn; ++r)
      std::memcpy(roi.data() + static_cast<std::size_t>(r) * cn,
                  data + static_cast<std::size_t>(r0 + r) * cols + c0,
                  sizeof(float) * static_cast<std::size_t>(cn));
    img.data = roi.data();
  }
  if (fits_write_file(&img, fits_tmp.c_str()) != 0) {
    (void)aio_atomic::remove_file(fits_tmp);
    record(meta, "IO_ERROR", nullptr, "", bytes, "fits_write_file failed");
    return "IO_ERROR";
  }
  if (aio_atomic::atomic_replace(fits_tmp, fits_final) != 0) {
    (void)aio_atomic::remove_file(fits_tmp);
    record(meta, "IO_ERROR", nullptr, "", bytes, "atomic rename failed");
    return "IO_ERROR";
  }
  write_sidecar(base + ".meta.json", meta, "raw_pixels");
  const std::string sha = sha256_of(fits_final);
  bump(meta);
  s.budget.max_total_bytes -= (bytes < s.budget.max_total_bytes) ? bytes
                                                                 : s.budget.max_total_bytes;
  record(meta, "WRITTEN", fits_final.c_str(), sha, bytes, nullptr);
  return "WRITTEN";
}

const char* snapshot_image_f64(const SnapshotMeta& meta,
                               int rows, int cols, const double* data) {
  std::lock_guard<std::mutex> lock(mu());
  RunState& s = state();
  if (!s.begun) { record(meta, "IO_ERROR", nullptr, "", 0, "run not begun"); return "IO_ERROR"; }
  if (!data || rows <= 0 || cols <= 0) {
    record(meta, "INVALID_VIEW", nullptr, "", 0, "null buffer or non-positive shape");
    return "INVALID_VIEW";
  }
  if (const char* st = gate(meta)) { record(meta, st, nullptr, "", 0, nullptr); return st; }

  const int rn = (meta.roi_rows > 0) ? meta.roi_rows : rows;
  const int cn = (meta.roi_cols > 0) ? meta.roi_cols : cols;
  const int r0 = (meta.roi_rows > 0) ? meta.roi_row0 : 0;
  const int c0 = (meta.roi_cols > 0) ? meta.roi_col0 : 0;
  if (r0 < 0 || c0 < 0 || rn <= 0 || cn <= 0 || r0 + rn > rows || c0 + cn > cols) {
    record(meta, "INVALID_VIEW", nullptr, "", 0, "ROI outside buffer");
    return "INVALID_VIEW";
  }
  const std::size_t n = static_cast<std::size_t>(rn) * static_cast<std::size_t>(cn);
  const std::uint64_t bytes = static_cast<std::uint64_t>(n) * sizeof(double);
  if (bytes > s.budget.max_inflight_bytes || bytes > s.budget.max_total_bytes) {
    record(meta, "SKIPPED_BUDGET", nullptr, "", bytes, "view exceeds byte budget");
    return "SKIPPED_BUDGET";
  }

  const std::size_t seq = s.per_probe_count[meta.probe_id];
  const std::string dir = s.root + "/snapshots/" + safe_segment(meta.probe_id) + "/" +
                          safe_segment(meta.frame_id ? meta.frame_id : "noframe");
  ensure_dir(dir);
  const std::string base = dir + "/" + std::to_string(seq);

  // FP64 原样落盘：裸 little-endian double + 旁挂 meta（aio 的 FITS 写路径
  // 只支持 FP32/INT16 位图，禁静默降精度；DIAGNOSTIC_PROBES.md §6 允许
  // "原样数值 + 显式 dtype/shape/endianness" 的二进制 + 同名 JSON metadata）。
  const std::string final_path = base + ".f64";
  std::string content;
  content.resize(bytes);
  if (r0 == 0 && c0 == 0 && rn == rows && cn == cols) {
    std::memcpy(&content[0], data, bytes);
  } else {
    const std::size_t row_bytes = sizeof(double) * static_cast<std::size_t>(cn);
    for (int r = 0; r < rn; ++r)
      std::memcpy(&content[static_cast<std::size_t>(r) * row_bytes],
                  data + static_cast<std::size_t>(r0 + r) * cols + c0, row_bytes);
  }
  std::string err;
  if (aio_atomic::write_file_atomic(final_path, content, &err) != 0) {
    record(meta, "IO_ERROR", nullptr, "", bytes, err.c_str());
    return "IO_ERROR";
  }
  write_sidecar(base + ".meta.json", meta, "raw_pixels");
  const std::string sha = sha256_of(final_path);
  bump(meta);
  s.budget.max_total_bytes -= (bytes < s.budget.max_total_bytes) ? bytes
                                                                 : s.budget.max_total_bytes;
  record(meta, "WRITTEN", final_path.c_str(), sha, bytes, nullptr);
  return "WRITTEN";
}

const char* snapshot_text(const SnapshotMeta& meta, const char* body) {
  std::lock_guard<std::mutex> lock(mu());
  RunState& s = state();
  if (!s.begun) { record(meta, "IO_ERROR", nullptr, "", 0, "run not begun"); return "IO_ERROR"; }
  if (!body) { record(meta, "INVALID_VIEW", nullptr, "", 0, "null body"); return "INVALID_VIEW"; }
  if (const char* st = gate(meta)) { record(meta, st, nullptr, "", 0, nullptr); return st; }

  const std::size_t seq = s.per_probe_count[meta.probe_id];
  const std::string dir = s.root + "/snapshots/" + safe_segment(meta.probe_id) + "/" +
                          safe_segment(meta.frame_id ? meta.frame_id : "noframe");
  ensure_dir(dir);
  const std::string final_path = dir + "/" + std::to_string(seq) + ".json";
  std::string err;
  if (aio_atomic::write_file_atomic(final_path, body, &err) != 0) {
    record(meta, "IO_ERROR", nullptr, "", std::strlen(body), err.c_str());
    return "IO_ERROR";
  }
  write_sidecar(final_path + ".meta.json", meta, "raw_records");
  const std::string sha = sha256_of(final_path);
  bump(meta);
  const std::uint64_t bytes = static_cast<std::uint64_t>(std::strlen(body));
  s.budget.max_total_bytes -= (bytes < s.budget.max_total_bytes) ? bytes
                                                                 : s.budget.max_total_bytes;
  record(meta, "WRITTEN", final_path.c_str(), sha, bytes, nullptr);
  return "WRITTEN";
}

unsigned long long diag_error_count() {
  std::lock_guard<std::mutex> lock(mu());
  return static_cast<unsigned long long>(state().errors);
}

unsigned long long diag_written_count() {
  std::lock_guard<std::mutex> lock(mu());
  return static_cast<unsigned long long>(state().written);
}

}  // namespace devdiag
}  // namespace acsd
