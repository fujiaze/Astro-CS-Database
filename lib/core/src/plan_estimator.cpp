// AstroCS Core Contracts — RT-005 plan 真实资源估算实现（独立纯函数库）
//
// 公式权威（不得重定义科学常量；docs/algorithms、docs/science、lib/phase* 实现）:
//   P3 order 选择/内存守卫: docs/science/PHASE3_HIPS_TO_FITS.md §5 + p3_session.cpp
//     (max_tiles = ceil(W·H/512²)+16, cap 1024; 输出平面 S+C 双 float; TileCache
//     f32 tile = 512·512·4 = 1 MiB; 每 worker 独立 sampler+cache)
//   P2: coverage target_order = min(max leaf order)（lib/phase2 coverage.cpp）;
//     block plan 峰值公式（p2_block_plan 先例）;
//   P1: 帧级行带（calibration/star-psf/wcs/photometry/noise-snr/drizzle）
//   kernel ID 权威清单 = lib/backend_host/backend_table.inc（12 kernel）
//   tile 宽 W=512 冻结（hips_properties.h kHipsTileWidth）
//
// 估算语义（14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md §4 + RT-005 验收）:
//   - 不同输入 metadata → 不同计划（确定性；同输入必同输出）；
//   - work_units==1 的 heavy 任务：max useful workers==1 → 拒绝(HEAVY_TINY)，
//     除明确 tiny（极小输入）标注 tiny_work/serial_only 允许串行；
//   - min/max useful workers 为本征并行上限（不读主机预算；scheduler 侧再 cap）。
#include "astrocs/core/plan_estimator.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace astrocs::core {

namespace {

// 保守并行上限（估算本征上限；避免 uint32 溢出；scheduler 按预算再 cap）。
constexpr uint32_t kMaxUsefulWorkers = 1024;
// 行带高度（P1 帧内行带 / P3 保守行带切片冻结粒度；ALG 文档行带语义，
// 库内冻结常数——非科学常量，行带内像素级独立保证 1/N 等价）。
constexpr uint64_t kRowBandRows = 16;
// tiny 输出判定阈值（px）：输出域像素 ≤ 128×128 等效 → 明确 tiny。
constexpr uint64_t kTinyOutputPixels = 128u * 128u;
// f32/f64 元素字节
constexpr uint64_t kF32Bytes = 4;
constexpr uint64_t kF64Bytes = 8;

bool mul_overflow(uint64_t a, uint64_t b, uint64_t* out) {
  if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a) return true;
  *out = a * b;
  return false;
}

// ceil(a/b) 饱和版（仅 p3_default_max_tiles 内存守卫 cap 前使用：真值远超 1024
// cap 时饱和到 UINT64_MAX 仍被 min(1024, …) 收敛，结果准确，无回绕）。
uint64_t ceil_div_sat(uint64_t a, uint64_t b) {
  if (b == 0) return std::numeric_limits<uint64_t>::max();
  const uint64_t hi = std::numeric_limits<uint64_t>::max() - (b - 1);
  if (a > hi) return std::numeric_limits<uint64_t>::max();
  return (a + b - 1) / b;
}

// P3 sampler 默认内存守卫 max_tiles（与 p3_session.cpp 完全一致）:
//   default_max = min(1024, max(8, ceil(W·H/512²)+16))；W/H<=0 → 8
uint64_t p3_default_max_tiles(uint64_t width_px, uint64_t height_px) {
  if (width_px == 0 || height_px == 0) return 8;
  uint64_t wh = 0;
  if (mul_overflow(width_px, height_px, &wh)) return 1024;
  const uint64_t per_tile = 512u * 512u;
  // ceil(W·H/512²)+16, cap [8,1024]（p3_session.cpp 一致）；+16 前 cap 防回绕
  const uint64_t need = ceil_div_sat(wh, per_tile);
  if (need > 1024u - 16u) return 1024;
  const uint64_t need_margin = need + 16;
  return std::min<uint64_t>(1024, std::max<uint64_t>(8, need_margin));
}

uint64_t safe_add(uint64_t a, uint64_t b) {
  return (b > std::numeric_limits<uint64_t>::max() - a)
             ? std::numeric_limits<uint64_t>::max()
             : a + b;
}

uint64_t tile_bytes(uint64_t tile_width_px) {
  uint64_t w = tile_width_px == 0 ? kHipsTileWidthPx : tile_width_px;
  return w * w * kF32Bytes;
}

// π（与 lib/common/healpix/healpix_core.cpp kPi 一致，禁止二义常量）
constexpr double kPi = 3.14159265358979323846264338327950288;

// 叶级像素角分辨率（度）——权威公式照搬 healpix_core.cpp
// pixel_resolution_arcsec(nside)=sqrt(4π/(12·nside²))·(180·3600/π) arcsec，
// 换算为 deg 即 sqrt(4π/12)/nside·(180/π)。
double leaf_res_deg(uint64_t leaf_nside) {
  return std::sqrt(4.0 * kPi / (12.0 * static_cast<double>(leaf_nside) *
                                static_cast<double>(leaf_nside))) *
         (180.0 / kPi);
}

// P3 order 选择（与 p3_resample.cpp p3_order_select 完全同逻辑：
// 求最小 k∈[0,max_order] 使 res_deg(512<<k) ≤ scale_deg；无则取 max_order。
// leaf nside = 512<<k（leaf_order = tile_order+9，W=512）。）
int order_select_deg(uint64_t max_order, double scale_deg_per_px) {
  const uint64_t cap = std::min<uint64_t>(max_order, kHipsMaxOrder);
  for (uint64_t k = 0; k <= cap; ++k) {
    if (leaf_res_deg(512u << k) <= scale_deg_per_px) return static_cast<int>(k);
  }
  return static_cast<int>(cap);
}

// 真实(子)模块前缀判断
bool starts_with(const std::string& s, const char* p) {
  return s.rfind(p, 0) == 0;
}

}  // namespace

PlanEstimateResult estimate_plan(const PlanInputMetadata& in) {
  PlanEstimateResult res;
  PlanEstimate& e = res.plan;

  // ── 输入校验 ──
  if (in.module_id.empty()) {
    res.error = PlanEstimateError::PARAM;
    res.error_message = "module_id required";
    return res;
  }
  if (in.is_heavy && !in.width_px && !in.height_px && !in.output_pixels &&
      !in.n_tiles_input) {
    res.error = PlanEstimateError::PARAM;
    res.error_message = "heavy input metadata must carry a size dimension";
    return res;
  }
  // scale 语义: >0 才参与估算；<=0 且 heavy HiPS 时视为缺失 → 保守误差界放行
  const bool has_scale = in.scale_deg_per_px > 0.0;
  (void)has_scale;

  e.module_id = in.module_id;
  e.execution_class = in.is_heavy ? "cpu_heavy" : "io";

  // ── 分支：Phase3 HiPS 重投影 ──
  if (starts_with(in.module_id, "astrocs.phase3.")) {
    const bool resample =
        starts_with(in.module_id, "astrocs.phase3.resample") ||
        starts_with(in.module_id, "astrocs.phase3.resample2");
    const uint64_t W = in.width_px;
    const uint64_t H = in.height_px;
    if (resample) {
      // 输出平面尺寸必须有效：resample 是输出像素域 CPU heavy 任务，W/H=0 会给出
      // work_units=0 / 伪平面（RT-005: 越界输入必须 checked 拒绝，不产出无效计划）。
      if (W == 0 || H == 0) {
        res.error = PlanEstimateError::PARAM;
        res.error_message =
            "phase3.resample* requires width_px>0 and height_px>0 (got " +
            std::to_string(W) + "x" + std::to_string(H) + ")";
        return res;
      }
      // P3 alpha 投影仅 TAN（SCI-P3 §1/API-P3 §4 显式拒非 TAN，无静默默认）
      if (!in.projection.empty() && in.projection != "TAN") {
        res.error = PlanEstimateError::UNSUPPORTED_PROJECTION;
        res.error_message =
            "phase3 projection '" + in.projection +
            "' unsupported (alpha 仅 TAN, SCI-P3 冻结)";
        return res;
      }
      // 输出像素(行带 work unit 基数)
      uint64_t pixels = 0;
      if (mul_overflow(W, H, &pixels)) {
        res.error = PlanEstimateError::PARAM;
        res.error_message = "width_px*height_px overflow";
        return res;
      }
      // 输入 order 决定叶级分辨率上限（p3_resample.cpp leaf_nside = 512<<order）
      if (in.input_order > kHipsMaxOrder) {
        res.error = PlanEstimateError::PARAM;
        res.error_message = "input_order " + std::to_string(in.input_order) +
                            " exceeds kHipsMaxOrder=" +
                            std::to_string(kHipsMaxOrder);
        return res;
      }
      const uint64_t input_order = in.input_order;
      // 采样 order_sel = 最小 k 使 leaf 分辨率 ≤ scale（SCI §5 order 选择公式）；
      // scale 缺失时保守用输入最高 order（不低估读量）。
      const int order_sel =
          has_scale ? order_select_deg(input_order, in.scale_deg_per_px)
                    : static_cast<int>(input_order);
      const uint64_t leaf_nside = 512u << std::max(0, order_sel);
      // 读覆盖几何（权威: PHASE3_HIPS_TO_FITS.md §5 逐像素反向映射 →
      // 输出足迹覆盖叶级像素; tile = 512×512 叶级像素的 HiPS 文件单元）。
      //   leaf_res_deg = 单叶级像素角(deg); tile_deg = 512·leaf_res_deg;
      //   输出行跨度(deg) ≈ W·scale_deg → 每行跨 tile 数 = ceil(跨度/tile_deg);
      //   足迹 tile 需求 ≈ row_tiles × col_tiles(列跨度 H·scale_deg 同式)。
      double leaf_deg = leaf_res_deg(leaf_nside);
      if (!(leaf_deg > 0.0)) leaf_deg = 1.0;
      const double tile_deg = leaf_deg * 512.0;
      uint64_t footprint_tiles = 1;
      if (has_scale) {
        const double row_span_deg =
            static_cast<double>(W) * in.scale_deg_per_px;
        const double col_span_deg =
            static_cast<double>(H) * in.scale_deg_per_px;
        const uint64_t rt = static_cast<uint64_t>(
            std::max(1.0, std::ceil(row_span_deg / tile_deg)));
        const uint64_t ct = static_cast<uint64_t>(
            std::max(1.0, std::ceil(col_span_deg / tile_deg)));
        footprint_tiles = (rt > UINT64_MAX / ct) ? UINT64_MAX : rt * ct;
      }
      // axis: row-band（P3 session worker 按行带并行；work_units=输出行带数，
      // 每个行带内像素级独立，1/N worker 结果等价——计划并行原子单元）
      e.axis = "row-band";
      e.work_units = ceil_div_sat(H, kRowBandRows);  // 行带数（≥1）
      // min/max useful workers: 行带可切分数为真实并行上限
      const uint32_t band_units = static_cast<uint32_t>(
          std::min<uint64_t>(e.work_units, kMaxUsefulWorkers));
      e.min_useful_workers = 1;  // heavy 可 1 worker 保底（但 max==1 时需标注/拒绝）
      e.max_useful_workers = band_units;
      // tiny: 输出域像素 ≤ 冻结阈值 → 标注（tiny 允许串行 heavy）
      e.tiny_work = pixels <= kTinyOutputPixels;
      e.serial_only = e.tiny_work && e.max_useful_workers <= 1;

      // 输出平面: S + C 双 float（p3_session sig/cov 两个 nelem buffer）
      uint64_t plane_bytes = 0;
      if (mul_overflow(pixels, kF32Bytes, &plane_bytes)) {
        res.error = PlanEstimateError::PARAM;
        res.error_message = "plane bytes overflow";
        return res;
      }
      const uint64_t out_two_planes = plane_bytes * 2;
      // tile 缓存: max_tiles 内存守卫（P3-006/DOC-003），每 worker 独立 sampler
      const uint64_t cache_tiles = p3_default_max_tiles(W, H);
      const uint64_t tw = tile_bytes(
          in.tile_width_px == 0 ? kHipsTileWidthPx : in.tile_width_px);
      uint64_t cache_bytes = 0;
      if (mul_overflow(cache_tiles, tw, &cache_bytes)) cache_bytes = UINT64_MAX;
      // 每 worker 独立 sampler cache（worker 数 ≤ max_useful_workers，保守取 1 份
      // cache 的 worker 因子——实际峰值含 worker 私有 cache，冻结误差界取最大
      // 上界=max_useful_workers 份；估算取中间值会在多 worker 时低估，故估算按
      // max_useful_workers 全量计（保守不低估）。tiny/串行时 workers=1。
      const uint64_t worker_factor =
          std::max<uint64_t>(1u, e.max_useful_workers);
      uint64_t cache_total = 0;
      if (mul_overflow(cache_bytes, worker_factor, &cache_total))
        cache_total = UINT64_MAX;
      // 输入 tile 读入暂存：每次缺 tile 读 1 f32 tile（IO 缓冲，短驻留）
      const uint64_t read_buf = tw;
      // peak = 输出双平面 + 全 worker tile cache + 输入读暂存 + 覆盖平面(双平面内)
      e.peak_memory_bytes = safe_add(safe_add(out_two_planes, cache_total), read_buf);
      // read/write: 读=足迹 tile×tile 字节（LRU cache 逐出后重读；覆盖 tile 数
      // 优先（真实覆盖），否则足迹几何推导；写=输出 f32 主平面）
      uint64_t read_tiles = 0;
      if (in.n_tiles_input > 0) {
        read_tiles = in.n_tiles_input;
      } else {
        read_tiles = footprint_tiles;
      }
      uint64_t rb = 0;
      if (mul_overflow(read_tiles, tw, &rb)) rb = UINT64_MAX;
      e.read_bytes = rb;
      uint64_t wb = 0;
      if (mul_overflow(pixels, kF32Bytes, &wb)) wb = UINT64_MAX;
      e.write_bytes = wb;  // 输出 FITS f32 主平面（写 S 平面）
      // kernel: 权威 backend_table.inc hips-bulk-transform (ALG-P3-002)
      e.kernel_ids = {"hips-bulk-transform"};
      e.serial_sections = {
          {"properties-parse", SerialSectionKind::METADATA, 1000,
           "HiPS properties 解析/校验 (短串行元数据, 约束 D.2)"},
          {"atomic-fits-write", SerialSectionKind::IO_WRITE, 0,
           "输出 FITS 原子写+provenance (短 I/O 串行 writer, 约束 D.2)"},
      };
    } else {
      // properties/wcs: 轻量 metadata 解析 + WCS 构造（非重采样）
      e.axis = "metadata";
      e.work_units = 1;
      e.min_useful_workers = 1;
      e.max_useful_workers = 1;
      e.tiny_work = true;
      e.serial_only = true;
      e.peak_memory_bytes = 1u << 20;  // 保守 1 MiB（properties/WCS 无重平面）
      e.read_bytes = 1u << 20;
      e.write_bytes = 1u << 20;
      e.kernel_ids = {"hips-bulk-transform"};
      e.serial_sections = {
          {"properties-parse", SerialSectionKind::METADATA, 1000,
           "HiPS properties 解析 (短串行元数据)"},
      };
    }
  }
  // ── 分支：Phase2 mosaic（coverage/sample/upm/reject/integrate） ──
  else if (starts_with(in.module_id, "astrocs.phase2.")) {
    // P2 有效工作域检查（RT-005: 越界/溢出输入必须 checked 拒绝，不静默饱和）：
    // 覆盖 cell 数（输入 tiles / order 推导）与输出像素域相乘不得溢出。
    if (in.n_tiles_input > (std::numeric_limits<uint64_t>::max() / (512u * 512u))) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "n_tiles_input exceeds representable output pixels";
      return res;
    }
    if (in.input_order > kHipsMaxOrder) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "input_order " + std::to_string(in.input_order) +
                          " exceeds kHipsMaxOrder=" +
                          std::to_string(kHipsMaxOrder);
      return res;
    }
    // 输出像素为 tile 级工作量；覆盖 cell 数优先（P2 union MOC）
    uint64_t cells = in.n_tiles_input;
    if (cells == 0) {
      // 保守：用输入 order 的完整 sky cell 数（12·2^(2·order)）避免低估
      const uint64_t order = in.input_order;
      cells = 12u << (2u * order);
    }
    // 输出域像素（tile 覆盖）× tile 像素(512²)；output_pixels 优先
    uint64_t out_px = in.output_pixels;
    if (out_px == 0) {
      if (mul_overflow(cells, 512u * 512u, &out_px)) {
        res.error = PlanEstimateError::PARAM;
        res.error_message = "coverage cells * tile pixels overflow";
        return res;
      }
    }
    // work_units = 覆盖 tile 数（MOC cell；每 tile 一个并行原子单元，
    // tile 内像素级独立 → 1/N worker 等价）
    e.axis = "tile";
    e.work_units = cells;
    const uint32_t cell_cap = static_cast<uint32_t>(
        std::min<uint64_t>(cells, kMaxUsefulWorkers));
    e.max_useful_workers = cell_cap;
    e.min_useful_workers = 1;
    // tiny: 输出域像素 ≤ 冻结阈值（覆盖 cell 极少 → 输出像素域小）
    e.tiny_work = out_px <= kTinyOutputPixels;
    e.serial_only = e.tiny_work && e.max_useful_workers <= 1;

    // 每覆盖 cell 处理 = 输入 tile 读 + 输出 tile 暂存；峰值保守 = 输出平面
    // （integration 输出 f32 平面）+ 每 tile scratch（p2_block_plan 先例）。
    uint64_t out_plane = 0;
    if (mul_overflow(out_px, kF32Bytes, &out_plane)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "output plane bytes overflow";
      return res;
    }
    // 每覆盖 tile 读: f32 tile + 每 tile 候选样本栈(f32 × n_frames × 像素)
    const uint64_t tile_px = 512u * 512u;
    uint64_t samples_per_tile = 0;
    if (mul_overflow(tile_px, in.n_frames, &samples_per_tile)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "n_frames exceeds per-tile sample count";
      return res;
    }
    uint64_t sample_bytes = 0;
    if (mul_overflow(samples_per_tile, kF32Bytes, &sample_bytes)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "sample stack bytes overflow";
      return res;
    }
    uint64_t stack_bytes = 0;
    if (mul_overflow(cells, sample_bytes, &stack_bytes)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "sample stack total overflow";
      return res;
    }
    // 峰值 = max(输出平面, 样本栈) + 读暂存（实际分块, 冻结界覆盖）
    e.peak_memory_bytes = safe_add(std::max(out_plane, stack_bytes),
                                   tile_bytes(kHipsTileWidthPx));
    uint64_t rb = 0;
    if (mul_overflow(cells, tile_bytes(kHipsTileWidthPx), &rb)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "read bytes overflow";
      return res;
    }
    e.read_bytes = rb;
    e.write_bytes = out_plane;
    // kernel: 模块 ALG→kernel（backend_table.inc 权威名）
    if (starts_with(in.module_id, "astrocs.phase2.coverage") ||
        starts_with(in.module_id, "astrocs.phase2.sample")) {
      e.kernel_ids = {"hips-bulk-transform"};
    } else if (starts_with(in.module_id, "astrocs.phase2.upm-fit") ||
               starts_with(in.module_id, "astrocs.phase2.upm-apply")) {
      e.kernel_ids = {"upm-spmv", "upm-residual", "upm-weight-update"};
    } else if (starts_with(in.module_id, "astrocs.phase2.reject")) {
      e.kernel_ids = {"rejection-statistics"};
    } else if (starts_with(in.module_id, "astrocs.phase2.integrate")) {
      e.kernel_ids = {"integration-accumulate"};
    } else {
      e.kernel_ids = {"hips-bulk-transform"};
    }
    e.serial_sections = {
        {"coverage-parse", SerialSectionKind::METADATA, 1000,
         "输入 properties/覆盖解析 (短串行元数据)"},
        {"atomic-hips-write", SerialSectionKind::IO_WRITE, 0,
         "输出 HiPS tile 原子写 (串行 writer, 约束 D.2)"},
    };
  }
  // ── 分支：Phase1 帧处理 ──
  else if (starts_with(in.module_id, "astrocs.phase1.")) {
    // P1 输入必须携带帧像素域尺寸（宽高缺省会被硬编码 1 掩盖 → checked 拒绝）
    if (in.width_px == 0 || in.height_px == 0) {
      res.error = PlanEstimateError::PARAM;
      res.error_message =
          "phase1 requires width_px>0 and height_px>0 (metadata carries frame "
          "shape; got " +
          std::to_string(in.width_px) + "x" + std::to_string(in.height_px) + ")";
      return res;
    }
    if (in.n_frames == 0) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "n_frames must be >= 1";
      return res;
    }
    // 每帧宽高；帧级 work units：输出域像素 = 帧像素 × 帧数
    const uint64_t frames = in.n_frames;
    const uint64_t w = in.width_px;
    const uint64_t h = in.height_px;
    uint64_t frame_px = 0;
    if (mul_overflow(w, h, &frame_px)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "width_px*height_px overflow";
      return res;
    }
    uint64_t total_px = 0;
    if (mul_overflow(frame_px, frames, &total_px)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "frame pixels * n_frames overflow";
      return res;
    }
    // work_units = 帧数 × 帧行带数（每帧行带为并行原子单元，行带内像素独立）
    e.axis = "frame-row-band";
    uint64_t row_bands = std::max<uint64_t>(1u, ceil_div_sat(h, kRowBandRows));
    uint64_t units = 0;
    if (mul_overflow(frames, row_bands, &units)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "frame count * row bands overflow";
      return res;
    }
    e.work_units = units;
    const uint32_t unit_cap = static_cast<uint32_t>(
        std::min<uint64_t>(units, kMaxUsefulWorkers));
    e.min_useful_workers = 1;
    e.max_useful_workers = unit_cap;
    // tiny: 单帧小图（输出域像素 ≤ 冻结阈值）
    e.tiny_work = total_px <= kTinyOutputPixels;
    e.serial_only = e.tiny_work && e.max_useful_workers <= 1;

    // 每帧平面 f32（校准/星空检测读帧）；peak = 帧平面 + 输出平面 + 少量暂存
    // （=2 帧平面余量 + 1 MiB；checked 拒绝溢出，不产出饱和伪峰值）
    uint64_t frame_plane = 0;
    if (mul_overflow(frame_px, kF32Bytes, &frame_plane)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "frame plane bytes overflow";
      return res;
    }
    uint64_t peak = 0;
    if (mul_overflow(frame_plane, 2, &peak)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "peak memory overflow";
      return res;
    }
    e.peak_memory_bytes = safe_add(peak, 1u << 20);
    // read = 全部输入帧像素 × f32（字节；每帧宽高×帧数）
    uint64_t all_frame_px = 0;
    if (mul_overflow(frame_px, frames, &all_frame_px)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "total frame pixels overflow";
      return res;
    }
    uint64_t rb = 0;
    if (mul_overflow(all_frame_px, kF32Bytes, &rb)) {
      res.error = PlanEstimateError::PARAM;
      res.error_message = "read bytes overflow";
      return res;
    }
    e.read_bytes = rb;
    e.write_bytes = frame_plane;
    // kernel: ALG 映射（module_adapters.cpp ALG id 同源 backend_table）
    if (starts_with(in.module_id, "astrocs.phase1.calibration") ||
        starts_with(in.module_id, "astrocs.phase1.cosmetic")) {
      e.kernel_ids = {"calibration-pixel-transform"};
    } else if (starts_with(in.module_id, "astrocs.phase1.star-psf") ||
               starts_with(in.module_id, "astrocs.phase1.wcs-platesolve") ||
               starts_with(in.module_id, "astrocs.phase1.photometry")) {
      e.kernel_ids = {"wcs-psf-batch"};
    } else if (starts_with(in.module_id, "astrocs.phase1.noise-snr")) {
      e.kernel_ids = {"noise-snr-reductions"};
    } else if (starts_with(in.module_id, "astrocs.phase1.drizzle")) {
      e.kernel_ids = {"drizzle-overlap", "drizzle-accumulate", "drizzle-normalize"};
    } else {
      e.kernel_ids = {"calibration-pixel-transform"};
    }
    e.serial_sections = {
        {"frame-io-read", SerialSectionKind::IO_READ, 0,
         "输入帧短读 (有界 IO 串行读, 约束 D.2)"},
        {"frame-io-write", SerialSectionKind::IO_WRITE, 0,
         "输出帧/HiPS 原子写 (串行 writer)"},
    };
  }
  // ── 未知模块 ──
  else {
    res.error = PlanEstimateError::UNSUPPORTED_MODULE;
    res.error_message = "unsupported module_id: " + in.module_id;
    return res;
  }

  // ── RT-005 验收核心：heavy 且 work_units==1（或 max useful==1）拒绝 ──
  // (14_RUNTIME_SCHEDULER_AND_TRACE_STANDARD.md §3: 重节点计划并行度为 1 时，
  //  只有 tiny_work 或明确 serial_reason 才可继续；否则 plan FAIL。)
  if (in.is_heavy && e.max_useful_workers == 1 && !e.tiny_work) {
    res.error = PlanEstimateError::HEAVY_TINY;
    res.error_message =
        "heavy module '" + in.module_id +
        "' estimates work_units==1 (max useful workers==1) without tiny_work: "
        "refused before run (单线程 heavy 拒绝, 14 标准 §3)";
    return res;
  }
  // heavy 串行兜底仅 tiny 放行
  if (in.is_heavy && e.max_useful_workers == 1 && e.tiny_work) {
    e.serial_only = true;  // 明确标注（14 标准: tiny_work 可继续）
  }
  res.error = PlanEstimateError::NONE;
  return res;
}

bool peak_within_frozen_bounds(const PlanEstimate& est, uint64_t* min_allowed,
                               uint64_t* max_allowed) {
  // 冻结误差界（独立复算源=实现结构；min/max 为真实运行峰值必然落点）:
  //   min = 纯输出 f32 主平面（运行峰值 ≥ 输出缓冲）；write_bytes 即主平面字节。
  //   max = 输出双平面(×2) + 每 worker tile cache 上界×workers
  //         + 输入读余量 + 2 MiB 常数余量。
  // 每 worker cache 上界 = p3_default_max_tiles 公式（ceil(W·H/512²)+16,
  // cap [8,1024]），从 write_bytes(=W·H·4) 可复算: tiles = ceil(wb/1MiB)+16。
  const uint64_t wb = est.write_bytes > 0 ? est.write_bytes : 1;
  const uint64_t min_v = wb;
  const uint64_t out_two = (wb > (UINT64_MAX >> 1)) ? UINT64_MAX : wb * 2;
  uint64_t cache_tiles = wb / kHipsTileF32Bytes;
  if (wb % kHipsTileF32Bytes) ++cache_tiles;
  cache_tiles += 16;
  cache_tiles = std::max<uint64_t>(8, std::min<uint64_t>(1024, cache_tiles));
  const uint64_t workers = std::max<uint64_t>(1u, est.max_useful_workers);
  uint64_t cache_total = 0;
  if (mul_overflow(cache_tiles, kHipsTileF32Bytes, &cache_total))
    cache_total = UINT64_MAX;
  uint64_t cache_all = 0;
  if (mul_overflow(cache_total, workers, &cache_all)) cache_all = UINT64_MAX;
  const uint64_t read_margin = est.read_bytes >> 3;  // 读余量（缓存逐出重读小比例）
  const uint64_t const_margin = 2u << 20;
  uint64_t max_v = 0;
  if (mul_overflow(out_two, 1, &max_v)) max_v = UINT64_MAX;
  max_v = safe_add(max_v, cache_all);
  max_v = safe_add(max_v, read_margin);
  max_v = safe_add(max_v, const_margin);
  if (min_allowed) *min_allowed = min_v;
  if (max_allowed) *max_allowed = max_v;
  return est.peak_memory_bytes >= min_v && est.peak_memory_bytes <= max_v;
}

std::string plan_estimate_to_json(const PlanEstimate& est) {
  std::ostringstream os;
  os << "{\"node_id\":\"" << est.node_id << "\",\"module_id\":\"" << est.module_id
     << "\",\"execution_class\":\"" << est.execution_class << "\",\"work_units\":"
     << est.work_units << ",\"axis\":\"" << est.axis
     << "\",\"min_useful_workers\":" << est.min_useful_workers
     << ",\"max_useful_workers\":" << est.max_useful_workers
     << ",\"tiny_work\":" << (est.tiny_work ? "true" : "false")
     << ",\"serial_only\":" << (est.serial_only ? "true" : "false")
     << ",\"peak_memory_bytes\":" << est.peak_memory_bytes
     << ",\"read_bytes\":" << est.read_bytes << ",\"write_bytes\":"
     << est.write_bytes << ",\"kernel_ids\":[";
  for (size_t i = 0; i < est.kernel_ids.size(); ++i) {
    if (i) os << ",";
    os << "\"" << est.kernel_ids[i] << "\"";
  }
  os << "],\"serial_sections\":[";
  for (size_t i = 0; i < est.serial_sections.size(); ++i) {
    if (i) os << ",";
    const auto& s = est.serial_sections[i];
    os << "{\"name\":\"" << s.name << "\",\"kind\":" << static_cast<int>(s.kind)
       << ",\"estimated_us\":" << s.estimated_us << ",\"reason\":\"" << s.reason
       << "\"}";
  }
  os << "]}";
  return os.str();
}

}  // namespace astrocs::core
