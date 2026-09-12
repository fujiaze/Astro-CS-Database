// P3-002 单元测试: WCS 尺寸溢出检查 + 配置合同上限 + 输出完整性
//           + STD-F1 导出边界 +1 桥接锁 (九宫格 + 负向注入, 前台裁决 R-02 方案 b)
#include "p3_wcs.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

// 溢出检查: 尺寸乘法在 uint64 域不溢出 (模拟 kernel 计划像素数)
static bool safe_pixel_count(int w, int h, std::uint64_t* out) {
  if (w <= 0 || h <= 0) return false;
  std::uint64_t uw = static_cast<std::uint64_t>(w);
  std::uint64_t uh = static_cast<std::uint64_t>(h);
  if (uw > UINT64_MAX / uh) return false;   // 乘法溢出检查
  *out = uw * uh;
  return true;
}

// ===========================================================================
// STD-F1 导出边界 +1 桥接锁 (前台裁决 R-02 方案 b)
// ---------------------------------------------------------------------------
// 合同锚:
//   - docs/science/ASTROMETRY.md §5/§7 (xp = x+1; CRPIX 1-based; 往返 <1e-6 px)
//   - docs/standards/STANDARDS_REGISTRY.md STD-F1 (Paper I §2.1.1)
//   - lib/phase3_session/p3_wcs.cpp (唯一 +1 桥接点 fits_pixel_1based)
// 验收 (任务规格 STD-F1-ADJ 必须动作 4/5):
//   1. 九宫格 = 中心 1 格 + 四角 4 格 + 四边中点 4 格, 每格 100x100 px,
//      逐像素显式验证**无 1px 偏移** (往返 < 1e-6 px 不变量, 每格 10000 像素);
//   2. 导出边界独立性: 由**独立第三方参考实现** (标准 TAN 向量式正投影, 不调用
//      被测函数) 按 Paper I §2.1.1 的 1-based 配对 (xp = x0 + 1) 前向, 与生产
//      p3_wcs_pix2world 逐点一致 (< 1e-6 px);
//   3. 负向注入**必败**: 同一独立参考改用「桥接移除」(xp = x0) 与「双重桥接」
//      (xp = x0 + 2) 配对时, 像素偏差恰为 1px/轴 (2D = √2 px) >> 冻结门 1e-4 px
//      ⇒ 桥接缺失/错置必被检出 (桥接不是恒真装饰);
//   4. parity: east_left/east_right 均无 parity 翻转, det(CD) < 0 冻结不变量;
//   5. 确定性: 同输入重复计算逐位 (bitwise) 一致。
// 证据导出 (JSON): FITS 关键字 + 每格统计 + 确定性抽样点, 供 astropy 第三方
// 对拍 (tests/unit/p1wcs/p1wcs_std_f1_bridge_cross.py)。路径由环境变量
// STD_F1_P3_EXPORT 覆盖, 默认 run/std_f1_adj/p3_nine_grid_export.json
// (AGENTS.md 目录规范: 运行产物一律落 run/, 不落仓库根)。
// ===========================================================================
namespace {

constexpr int kFrameW = 1024;
constexpr int kFrameH = 1024;
constexpr int kCell = 100;                  // 每格 100x100 px (负责人 rev3.1 口径)
constexpr double kScale = 0.0001389;        // deg/px (P3-002 既有用例同值)
constexpr double kRoundtripGatePx = 1e-6;   // SCI-WCS-001 §7 往返不变量 (FP64)
constexpr double kFrozenGatePx = 1e-4;      // 冻结门 (Paper I 交叉, 不放宽)
constexpr int kSampleStride = 10;           // 抽样: 每格每轴每 10px 取一点
constexpr double kDegToRad = 0.01745329251994329577;
constexpr double kRadToDeg = 57.29577951308232087680;

struct GridCell {
  const char* name;
  const char* kind;
  int x0;
  int y0;
};

// 九宫格: 中心 1 + 四角 4 + 四边中点 4 (1024x1024 帧上 9 个 100x100 格)
const GridCell kNineGrid[9] = {
    {"corner_nw", "corner", 0, 0},
    {"corner_ne", "corner", kFrameW - kCell, 0},
    {"corner_sw", "corner", 0, kFrameH - kCell},
    {"corner_se", "corner", kFrameW - kCell, kFrameH - kCell},
    {"edge_n", "edge_mid", (kFrameW - kCell) / 2, 0},
    {"edge_s", "edge_mid", (kFrameW - kCell) / 2, kFrameH - kCell},
    {"edge_w", "edge_mid", 0, (kFrameH - kCell) / 2},
    {"edge_e", "edge_mid", kFrameW - kCell, (kFrameH - kCell) / 2},
    {"center", "center", (kFrameW - kCell) / 2, (kFrameH - kCell) / 2},
};

// ---------------------------------------------------------------------------
// 独立第三方参考实现: FITS WCS Paper I §2.1.1 + Paper II §5 (TAN) 的
// **1-based 配对**前向 (像素 x0 为 0-based 时 xp = x0 + pixel_origin)。
// 独立性: 向量/旋转矩阵路径 (east/north/视线基), 不调用 p3_wcs_* 任何函数,
// 不与生产实现共享代码; 仅共享 Paper I/II 的标准公式语义。
//   pixel_origin = 1.0 : 合同声明桥接 (xp = x0 + 1)  ← 正确
//   pixel_origin = 0.0 : 桥接移除 (把 0-based 坐标当 1-based 用)  ← 负向注入
//   pixel_origin = 2.0 : 双重桥接 (多算一次 +1)                  ← 负向注入
// ---------------------------------------------------------------------------
void fits_tan_forward_ref(const double crpix[2], const double crval[2],
                          const double cd[2][2], double pixel_origin, double x0,
                          double y0, double* ra_deg, double* dec_deg) {
  const double dx = (x0 + pixel_origin) - crpix[0];   // deg
  const double dy = (y0 + pixel_origin) - crpix[1];
  const double xi = (cd[0][0] * dx + cd[0][1] * dy) * kDegToRad;   // rad
  const double eta = (cd[1][0] * dx + cd[1][1] * dy) * kDegToRad;
  const double a0 = crval[0] * kDegToRad, d0 = crval[1] * kDegToRad;
  // 切平面基: east/north/视线 n0 (Paper II §2.1 语义)
  const double ex = -std::sin(a0), ey = std::cos(a0), ez = 0.0;
  const double nx = -std::sin(d0) * std::cos(a0);
  const double ny = -std::sin(d0) * std::sin(a0);
  const double nz = std::cos(d0);
  const double ox = std::cos(d0) * std::cos(a0);
  const double oy = std::cos(d0) * std::sin(a0);
  const double oz = std::sin(d0);
  // gnomonic: 视线方向 v = n0 + xi·east + eta·north (未归一)
  const double vx = ox + xi * ex + eta * nx;
  const double vy = oy + xi * ey + eta * ny;
  const double vz = oz + xi * ez + eta * nz;
  const double r = std::sqrt(vx * vx + vy * vy + vz * vz);
  *dec_deg = std::asin(vz / r) * kRadToDeg;
  double ra = std::atan2(vy, vx) * kRadToDeg;
  if (ra < 0.0) ra += 360.0;
  *ra_deg = ra;
}

// 故障注入点 (测试面, 对齐本仓 FaultRegistry 习例; 生产代码零改动):
//   STD_F1_BRIDGE_FAULT=nobridge → 把"声明桥接"臂改按 xp = x0 求值 (桥接移除)
//   STD_F1_BRIDGE_FAULT=double   → 把"声明桥接"臂改按 xp = x0 + 2 求值 (双重桥接)
// 两者都必须使本例**失败** (负向注入必败), 用于 CI 中可重跑地证明桥接不是恒真装饰。
double declared_pixel_origin() {
  const char* f = std::getenv("STD_F1_BRIDGE_FAULT");
  if (f == nullptr) return 1.0;
  if (std::strcmp(f, "nobridge") == 0) return 0.0;
  if (std::strcmp(f, "double") == 0) return 2.0;
  return 1.0;
}

struct CellStats {
  double max_rt_px = 0.0;              // 逐像素往返最大偏差 (含全部 100x100 像素)
  double max_ref_px = 0.0;             // 独立参考(桥接=1) vs 生产: 必须 ~0
  double max_nobridge_px = 0.0;        // 负向注入 A: 桥接移除 → 1px/轴
  double max_double_px = 0.0;          // 负向注入 B: 双重桥接 → 1px/轴
  double min_nobridge_axis = 1e30;     // 每轴最小 |偏差| (必须 1px)
  double max_nobridge_axis = 0.0;
  double min_double_axis = 1e30;
  double max_double_axis = 0.0;
  int n_px = 0;
  int n_bridge_px = 0;
  std::vector<double> sample;          // x,y,ra,dec 确定性抽样 (JSON 导出)
};

// 独立参考 → 生产逆变换 → 相对像素 (x0,y0) 的偏差 (px)
bool ref_pixel_offset(const astrocs::phase3::P3WcsDescriptor& d,
                      const double crpix[2], const double crval[2],
                      const double cd[2][2], double pixel_origin, double x0,
                      double y0, double* sx, double* sy) {
  double ra, dec, xr, yr;
  fits_tan_forward_ref(crpix, crval, cd, pixel_origin, x0, y0, &ra, &dec);
  if (astrocs::phase3::p3_wcs_world2pix(&d, ra, dec, &xr, &yr) !=
      astrocs::phase3::P3_WCS_OK)
    return false;
  *sx = xr - x0;
  *sy = yr - y0;
  return true;
}

void evaluate_cell(const astrocs::phase3::P3WcsDescriptor& d, const GridCell& c,
                   CellStats* st) {
  const double crpix[2] = {d.crpix_x, d.crpix_y};
  const double crval[2] = {d.crval_ra_deg, d.crval_dec_deg};
  const double cd[2][2] = {{d.cd[0][0], d.cd[0][1]}, {d.cd[1][0], d.cd[1][1]}};
  double ra, dec, xr, yr;
  for (int j = 0; j < kCell; ++j) {
    for (int i = 0; i < kCell; ++i) {
      const double x0 = c.x0 + i;
      const double y0 = c.y0 + j;
      if (astrocs::phase3::p3_wcs_pix2world(&d, x0, y0, &ra, &dec) !=
              astrocs::phase3::P3_WCS_OK ||
          astrocs::phase3::p3_wcs_world2pix(&d, ra, dec, &xr, &yr) !=
              astrocs::phase3::P3_WCS_OK) {
        ++st->n_px;
        st->max_rt_px = 1e30;   // 半球外/参数拒绝: 不得静默通过
        continue;
      }
      const double rt = std::hypot(xr - x0, yr - y0);
      if (rt > st->max_rt_px) st->max_rt_px = rt;
      ++st->n_px;
      if (i % kSampleStride == 0 && j % kSampleStride == 0) {
        st->sample.insert(st->sample.end(), {x0, y0, ra, dec});
      }
    }
  }
  // 桥接臂 (抽样点: 每格 10x10 = 100 点)
  for (int j = 0; j < kCell; j += kSampleStride) {
    for (int i = 0; i < kCell; i += kSampleStride) {
      const double x0 = c.x0 + i;
      const double y0 = c.y0 + j;
      double sx, sy;
      if (ref_pixel_offset(d, crpix, crval, cd, declared_pixel_origin(), x0, y0,
                           &sx, &sy)) {
        const double m = std::hypot(sx, sy);
        if (m > st->max_ref_px) st->max_ref_px = m;
      }
      if (ref_pixel_offset(d, crpix, crval, cd, 0.0, x0, y0, &sx, &sy)) {
        const double m = std::hypot(sx, sy);
        if (m > st->max_nobridge_px) st->max_nobridge_px = m;
        st->min_nobridge_axis = std::min(st->min_nobridge_axis, std::fabs(sx));
        st->max_nobridge_axis = std::max(st->max_nobridge_axis, std::fabs(sx));
      }
      if (ref_pixel_offset(d, crpix, crval, cd, 2.0, x0, y0, &sx, &sy)) {
        const double m = std::hypot(sx, sy);
        if (m > st->max_double_px) st->max_double_px = m;
        st->min_double_axis = std::min(st->min_double_axis, std::fabs(sx));
        st->max_double_axis = std::max(st->max_double_axis, std::fabs(sx));
      }
      ++st->n_bridge_px;
    }
  }
}

std::string json_num(double v) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.17g", v);
  return std::string(buf);
}

std::string json_escape(const std::string& s) {
  std::string out;
  for (char ch : s) {
    if (ch == '"' || ch == '\\') { out.push_back('\\'); out.push_back(ch); }
    else if (ch == '\n') out += "\\n";
    else out.push_back(ch);
  }
  return out;
}

std::string cell_json(const GridCell& c, const CellStats& st) {
  std::string s = "{\"name\":\"" + std::string(c.name) + "\",\"kind\":\"" +
                  std::string(c.kind) + "\",\"x0\":" + std::to_string(c.x0) +
                  ",\"y0\":" + std::to_string(c.y0) +
                  ",\"width\":" + std::to_string(kCell) +
                  ",\"height\":" + std::to_string(kCell) +
                  ",\"n_pixels\":" + std::to_string(st.n_px) +
                  ",\"max_roundtrip_px\":" + json_num(st.max_rt_px) +
                  ",\"ref_shift_px\":" + json_num(st.max_ref_px) +
                  ",\"nobridge_shift_px\":" + json_num(st.max_nobridge_px) +
                  ",\"double_shift_px\":" + json_num(st.max_double_px) +
                  ",\"nobridge_axis_min\":" + json_num(st.min_nobridge_axis) +
                  ",\"nobridge_axis_max\":" + json_num(st.max_nobridge_axis) +
                  ",\"double_axis_min\":" + json_num(st.min_double_axis) +
                  ",\"double_axis_max\":" + json_num(st.max_double_axis) +
                  ",\"samples\":[";
  for (std::size_t k = 0; k + 3 < st.sample.size(); k += 4) {
    if (k) s += ",";
    s += "{\"x\":" + json_num(st.sample[k]) + ",\"y\":" + json_num(st.sample[k + 1]) +
         ",\"ra\":" + json_num(st.sample[k + 2]) +
         ",\"dec\":" + json_num(st.sample[k + 3]) + "}";
  }
  s += "]}";
  return s;
}

std::string descriptor_json(const astrocs::phase3::P3WcsDescriptor& d,
                            const char* parity, const std::string& keywords) {
  const double det = d.cd[0][0] * d.cd[1][1] - d.cd[0][1] * d.cd[1][0];
  std::string s = "{\"parity\":\"" + std::string(parity) +
                  "\",\"crpix\":[" + json_num(d.crpix_x) + "," + json_num(d.crpix_y) +
                  "],\"crval\":[" + json_num(d.crval_ra_deg) + "," +
                  json_num(d.crval_dec_deg) + "],\"cd\":[[" + json_num(d.cd[0][0]) +
                  "," + json_num(d.cd[0][1]) + "],[" + json_num(d.cd[1][0]) + "," +
                  json_num(d.cd[1][1]) + "]],\"det_cd\":" + json_num(det) +
                  ",\"keywords\":\"" + json_escape(keywords) + "\"}";
  return s;
}

// 九宫格 + 桥接锁主体验: 每个 parity 跑一遍并导出证据 JSON 片段。
std::string run_nine_grid(const char* parity,
                          astrocs::phase3::P3WcsDescriptor* d_out,
                          CellStats stats_out[9]) {
  astrocs::phase3::P3WcsDescriptor d{};
  const bool made = astrocs::phase3::p3_wcs_make(
                        150.0, 2.0, kScale, kFrameW, kFrameH, parity, 0.0, &d) ==
                    astrocs::phase3::P3_WCS_OK;
  CHECK(made);
  if (!made) return {};
  *d_out = d;
  for (int c = 0; c < 9; ++c) evaluate_cell(d, kNineGrid[c], &stats_out[c]);

  for (int c = 0; c < 9; ++c) {
    const CellStats& st = stats_out[c];
    // 断言 1: 九宫格逐格无 1px 偏移 (往返 < 1e-6 px, 每格 10000 像素)
    CHECK(st.n_px == kCell * kCell);
    CHECK(st.max_rt_px < kRoundtripGatePx);
    CHECK(st.n_bridge_px == (kCell / kSampleStride) * (kCell / kSampleStride));
    // 断言 2: 独立第三方参考 (Paper I §2.1.1, xp = x0+1) 与生产逐点一致
    CHECK(st.max_ref_px < kRoundtripGatePx);
    // 断言 3: 负向注入必败 —— 桥接移除/双重桥接的像素偏差恰为 1px/轴 (√2 px)
    CHECK(st.max_nobridge_px > kFrozenGatePx);
    CHECK(st.max_double_px > kFrozenGatePx);
    CHECK(std::fabs(st.max_nobridge_px - std::sqrt(2.0)) < 1e-6);
    CHECK(std::fabs(st.max_double_px - std::sqrt(2.0)) < 1e-6);
    CHECK(std::fabs(st.min_nobridge_axis - 1.0) < 1e-6);
    CHECK(std::fabs(st.max_nobridge_axis - 1.0) < 1e-6);
    CHECK(std::fabs(st.min_double_axis - 1.0) < 1e-6);
    CHECK(std::fabs(st.max_double_axis - 1.0) < 1e-6);
  }

  // 断言 4: parity 手性冻结 (det(CD) < 0), CRPIX 冻结不变量, CD1_1 符号
  const double det = d.cd[0][0] * d.cd[1][1] - d.cd[0][1] * d.cd[1][0];
  CHECK(det < 0.0);
  CHECK(d.crpix_x == (kFrameW + 1) / 2.0);
  CHECK(d.crpix_y == (kFrameH + 1) / 2.0);
  if (std::strcmp(parity, "east_left") == 0) CHECK(d.cd[0][0] < 0.0);
  else CHECK(d.cd[0][0] > 0.0);

  // 断言 5: 确定性 bitwise (同输入重复计算逐位一致)
  for (int c = 0; c < 9; ++c) {
    CellStats again{};
    evaluate_cell(d, kNineGrid[c], &again);
    CHECK(std::memcmp(&again.max_rt_px, &stats_out[c].max_rt_px,
                      sizeof(double)) == 0);
    CHECK(std::memcmp(&again.max_ref_px, &stats_out[c].max_ref_px,
                      sizeof(double)) == 0);
    CHECK(again.sample == stats_out[c].sample);
  }

  std::string body = "{\"parity\":\"" + std::string(parity) + "\",\"descriptor\":" +
                     descriptor_json(d, parity,
                                     astrocs::phase3::p3_wcs_fits_keywords(&d)) +
                     ",\"cells\":[";
  for (int c = 0; c < 9; ++c) {
    if (c) body += ",";
    body += cell_json(kNineGrid[c], stats_out[c]);
  }
  body += "]}";
  return body;
}

}  // namespace

int main() {
  // 1) WCS 输出完整: 输入 HiPS properties/尺度 → WCS/dimensions/pixel scale
  {
    astrocs::phase3::P3WcsDescriptor w{};
    // 输入: 中心 RA/Dec, scale, 尺寸 → 输出 WCS
    CHECK(astrocs::phase3::p3_wcs_make(
              150.0, 2.0, 0.0001389, 1024, 768,
              "east_left", 0.0, &w) == astrocs::phase3::P3_WCS_OK);
    CHECK(w.width_px == 1024);
    CHECK(w.height_px == 768);
    CHECK(w.cd[0][0] != 0);   // pixel scale 存在 (deg/px)
    CHECK(std::fabs(w.cd[0][0] + 0.0001389) < 1e-9);  // east_left: CD1_1=-s
  }

  // 2) 尺寸乘法溢出检查: 极大合法边界内不溢出
  {
    std::uint64_t n = 0;
    CHECK(safe_pixel_count(20000, 20000, &n));        // 4e8 像素 OK
    CHECK(n == 400000000ULL);
    // 超界拒绝 (配置合同上限之外)
    CHECK(!safe_pixel_count(0, 100, &n));             // 零尺寸
    CHECK(!safe_pixel_count(-5, 100, &n));            // 负尺寸
  }

  // 3) 最大尺寸来自配置合同: 超上限拒绝 (WCS 层)
  {
    astrocs::phase3::P3WcsDescriptor w{};
    // 20001 > 默认合同上限 20000 → 拒
    CHECK(astrocs::phase3::p3_wcs_make(
              150.0, 2.0, 0.0001389, 20001, 100,
              "east_left", 0.0, &w) == astrocs::phase3::P3_WCS_PARAM);
    // 边界值 20000 接受
    CHECK(astrocs::phase3::p3_wcs_make(
              150.0, 2.0, 0.0001389, 20000, 20000,
              "east_left", 0.0, &w) == astrocs::phase3::P3_WCS_OK);
  }

  // 4) pix2world roundtrip (kernel 计划前提)
  {
    astrocs::phase3::P3WcsDescriptor w{};
    astrocs::phase3::p3_wcs_make(150.0, 2.0, 0.0001389, 1024, 768,
                                 "east_left", 0.0, &w);
    double ra, dec, x, y;
    CHECK(astrocs::phase3::p3_wcs_pix2world(&w, 512.0, 384.0, &ra, &dec) == astrocs::phase3::P3_WCS_OK);
    CHECK(astrocs::phase3::p3_wcs_world2pix(&w, ra, dec, &x, &y) == astrocs::phase3::P3_WCS_OK);
    CHECK(std::fabs(x - 512.0) < 1e-4);
    CHECK(std::fabs(y - 384.0) < 1e-4);
  }

  // 5) 溢出边界: 乘法保护 (接近 uint64 上限模拟)
  {
    std::uint64_t n = 0;
    // 模拟 50000×50000 = 2.5e9 仍安全 (uint64 域)
    CHECK(safe_pixel_count(50000, 50000, &n));
    CHECK(n == 2500000000ULL);
  }

  // 6) STD-F1 导出边界 +1 桥接锁: 九宫格 (中心 1 + 四角 4 + 四边中点 4,
  //    每格 100x100 px) 逐像素无 1px 偏移 + 独立参考一致性 + 负向注入必败
  //    + parity 手性 + 确定性 bitwise
  {
    astrocs::phase3::P3WcsDescriptor d_left{}, d_right{};
    CellStats st_left[9], st_right[9];
    const std::string j_left = run_nine_grid("east_left", &d_left, st_left);
    const std::string j_right = run_nine_grid("east_right", &d_right, st_right);
    CHECK(!j_left.empty() && !j_right.empty());

    // 九宫格逐格摘要 (stdout, 证据表来源)
    for (int c = 0; c < 9; ++c) {
      std::printf(
          "[STD-F1 nine-grid] %-9s (%-8s) x0=%4d y0=%4d px=%5d rt=%.3e "
          "ref=%.3e nobridge=%.6f double=%.6f px\n",
          kNineGrid[c].name, kNineGrid[c].kind, kNineGrid[c].x0, kNineGrid[c].y0,
          st_left[c].n_px, st_left[c].max_rt_px, st_left[c].max_ref_px,
          st_left[c].max_nobridge_px, st_left[c].max_double_px);
    }

    // 证据 JSON 导出 (astropy 第三方对拍输入; 落 run/, 不落仓库根)
    const char* env = std::getenv("STD_F1_P3_EXPORT");
    const std::string path =
        (env && env[0]) ? std::string(env)
                        : std::string("run/std_f1_adj/p3_nine_grid_export.json");
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path(), ec);
    if (FILE* fp = std::fopen(path.c_str(), "w")) {
      std::fprintf(fp,
                   "{\"schema\":\"std_f1/p3-nine-grid-export-v1\","
                   "\"frame\":{\"width\":%d,\"height\":%d},"
                   "\"cell_px\":%d,\"roundtrip_gate_px\":%.3e,"
                   "\"frozen_gate_px\":%.3e,"
                   "\"crpix_invariant\":\"(W+1)/2\","
                   "\"bridge\":\"p3_wcs.cpp fits_pixel_1based (xp = x + 1)\","
                   "\"bridge_fault_env\":\"%s\","
                   "\"runs\":[%s,%s]}\n",
                   kFrameW, kFrameH, kCell, kRoundtripGatePx, kFrozenGatePx,
                   (std::getenv("STD_F1_BRIDGE_FAULT") ? std::getenv("STD_F1_BRIDGE_FAULT") : ""),
                   j_left.c_str(), j_right.c_str());
      std::fclose(fp);
    } else {
      CHECK(false);   // 证据落盘失败不得静默 (宪章 §14.4)
    }
  }

  if (failures == 0) {
    std::printf("P3-002 TESTS PASS (尺寸溢出检查, 配置合同上限 20000, WCS 输出完整, "
                "roundtrip, STD-F1 九宫格 9x100x100 无 1px 偏移 + 负向注入必败)\n");
    return 0;
  }
  std::fprintf(stderr, "P3-002 TESTS FAIL (%d)\n", failures);
  return 1;
}
