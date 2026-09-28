/* exp11_recon_driver.cpp — P2-M5 闭环证据驱动（只读链接生产源，不改 lib/ 一行）
 *
 * 作用：把生产重建器 astrocs::v6::p2weight::SparseSnrReconstructor / reconstruct_sparse_snr
 * （lib/algorithms/integration/phase2_integrate/src/weight_chain.cpp）逐点求值的结果导出，
 * 供 实验/absolute-snr/code/audit/route3/exp11_frozen_operator_transfer.py 计算
 * 「控制点精度 → 稠密 SNR 场」的传递通胀。**不在本驱动里另写任何重建算子**。
 *
 * 用法: exp11_recon_driver <spec.bin> <out.bin>
 *   spec.bin = 本脚本 Python 侧按下方布局写出的二进制规格（小端，逐字段 fread）
 *   out.bin  = double[n_query]：普通模式 = 重建值（求值失败写 NaN）；
 *              impulse 模式 = 每个查询点的 Σ_k w_k(q)²（w 由生产算子对单位脉冲的响应测出）
 *   <out>.mask = uint8[n_query]：1 = 求值成功，0 = fail-closed
 * stdout = key=value 行（诊断量，供 Python 解析）
 *
 * spec 布局（顺序，无 padding）:
 *   char[8] magic = "E11SP1\0\0"
 *   int32 regular_grid; int32 declare_operator; char token[64]
 *   int32 nx, ny
 *   double x0, y0, dx, dy, grid_origin_x, grid_origin_y, max_radius_px, grid_tol
 *   int64 n_ctrl; int64 ctrl_layout(0=values only / 1=triples x,y,v)
 *   int64 n_query
 *   double impulse_eps; int64 impulse_count
 *   ctrl payload; query payload(double x,y 对)
 *
 * 编译（见 exp11_build_driver.sh）:
 *   g++ -O2 -std=c++17 -I lib/algorithms/integration/phase2_integrate/include \
 *       exp11_recon_driver.cpp .../src/weight_chain.cpp -o exp11_recon_driver
 */
#include "astrocs/weight_chain.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using astrocs::v6::p2weight::SparseReconstruction;
using astrocs::v6::p2weight::SparseSnrLayer;
using astrocs::v6::p2weight::SparseSnrPoint;
using astrocs::v6::p2weight::SparseSnrReconstructor;

namespace {

bool rd(std::FILE* f, void* out, std::size_t n) { return std::fread(out, 1, n, f) == n; }

template <typename T>
bool rdv(std::FILE* f, T* out) { return rd(f, out, sizeof(T)); }

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: %s <spec.bin> <out.bin>\n", argv[0]);
    return 2;
  }
  std::FILE* in = std::fopen(argv[1], "rb");
  if (in == nullptr) { std::fprintf(stderr, "cannot open spec %s\n", argv[1]); return 2; }

  char magic[8] = {0};
  if (!rd(in, magic, 8) || std::memcmp(magic, "E11SP1\0\0", 8) != 0) {
    std::fprintf(stderr, "bad spec magic\n");
    std::fclose(in);
    return 2;
  }
  int32_t regular_grid = 0, declare_op = 0;
  char token[64] = {0};
  int32_t nx = 0, ny = 0;
  double x0 = 0, y0 = 0, dx = 1, dy = 1, gox = 0, goy = 0, maxr = -1.0, tol = 1e-6;
  int64_t n_ctrl = 0, ctrl_layout = 0, n_query = 0;
  double impulse_eps = 0.0;
  int64_t impulse_count = 0;
  const bool ok_hdr =
      rdv(in, &regular_grid) && rdv(in, &declare_op) && rd(in, token, 64) &&
      rdv(in, &nx) && rdv(in, &ny) && rdv(in, &x0) && rdv(in, &y0) && rdv(in, &dx) &&
      rdv(in, &dy) && rdv(in, &gox) && rdv(in, &goy) && rdv(in, &maxr) && rdv(in, &tol) &&
      rdv(in, &n_ctrl) && rdv(in, &ctrl_layout) && rdv(in, &n_query) &&
      rdv(in, &impulse_eps) && rdv(in, &impulse_count);
  if (!ok_hdr) { std::fprintf(stderr, "truncated spec header\n"); std::fclose(in); return 2; }
  token[63] = '\0';

  SparseSnrLayer layer;
  layer.present = true;
  layer.regular_grid = (regular_grid != 0);
  layer.nx = nx;
  layer.ny = ny;
  layer.x0 = x0;
  layer.y0 = y0;
  layer.dx = dx;
  layer.dy = dy;
  layer.grid_origin_x = gox;
  layer.grid_origin_y = goy;
  layer.max_radius_px = maxr;
  layer.grid_tol = tol;
  if (declare_op != 0) layer.reconstruction_operator = std::string(token);

  std::vector<SparseSnrPoint> base(static_cast<std::size_t>(n_ctrl));
  if (ctrl_layout == 0) {
    for (int64_t k = 0; k < n_ctrl; ++k) {
      const int64_t j = k / nx;
      const int64_t i = k % nx;
      double v = 0.0;
      if (!rdv(in, &v)) { std::fprintf(stderr, "truncated ctrl values\n"); return 2; }
      SparseSnrPoint p;
      p.x = x0 + static_cast<double>(i) * dx;
      p.y = y0 + static_cast<double>(j) * dy;
      p.snr = v;
      base[static_cast<std::size_t>(k)] = p;
    }
  } else {
    for (int64_t k = 0; k < n_ctrl; ++k) {
      SparseSnrPoint p;
      if (!rdv(in, &p.x) || !rdv(in, &p.y) || !rdv(in, &p.snr)) {
        std::fprintf(stderr, "truncated scattered triples\n");
        return 2;
      }
      base[static_cast<std::size_t>(k)] = p;
    }
  }
  std::vector<double> qx(static_cast<std::size_t>(n_query));
  std::vector<double> qy(static_cast<std::size_t>(n_query));
  for (int64_t k = 0; k < n_query; ++k) {
    if (!rdv(in, &qx[static_cast<std::size_t>(k)]) ||
        !rdv(in, &qy[static_cast<std::size_t>(k)])) {
      std::fprintf(stderr, "truncated query list\n");
      return 2;
    }
  }
  std::fclose(in);

  layer.points = base;
  SparseSnrReconstructor rec;
  std::string err;
  if (!rec.prepare(layer, &err)) {
    std::printf("prepare_ok 0\n");
    std::printf("prepare_err %s\n", err.c_str());
    std::printf("n_ctrl %lld\n", static_cast<long long>(n_ctrl));
    std::printf("n_query %lld\n", static_cast<long long>(n_query));
    return 3;
  }

  std::vector<double> vals(static_cast<std::size_t>(n_query),
                           std::numeric_limits<double>::quiet_NaN());
  std::vector<unsigned char> mask(static_cast<std::size_t>(n_query), 0);
  int64_t n_ok = 0, n_err = 0, n_ood = 0;
  std::string first_err;
  double vmin = std::numeric_limits<double>::infinity();
  double vmax = -std::numeric_limits<double>::infinity();
  double vsum = 0.0;
  for (int64_t k = 0; k < n_query; ++k) {
    double v = 0.0;
    SparseReconstruction info;
    std::string e;
    const bool ok = rec.eval(qx[static_cast<std::size_t>(k)], qy[static_cast<std::size_t>(k)],
                            &v, &info, &e);
    if (ok) {
      vals[static_cast<std::size_t>(k)] = v;
      mask[static_cast<std::size_t>(k)] = 1;
      ++n_ok;
      vmin = std::min(vmin, v);
      vmax = std::max(vmax, v);
      vsum += v;
    } else {
      ++n_err;
      if (info.out_of_domain) ++n_ood;
      if (first_err.empty()) first_err = e;
    }
  }

  std::printf("prepare_ok 1\n");
  std::printf("operator_id %s\n", rec.operator_id());
  std::printf("clipped %d\n", rec.value_range_clipped() ? 1 : 0);
  std::printf("clip_low %.17g\n", rec.clip_low());
  std::printf("clip_high %.17g\n", rec.clip_high());
  std::printf("mesh_median %d\n", rec.mesh_median_applied() ? 1 : 0);
  std::printf("n_filled %lld\n", static_cast<long long>(rec.n_invalid_control_points_filled()));
  std::printf("cell_center_offset %.17g\n", rec.cell_center_offset_max_abs());
  std::printf("node_residual %.17g\n", rec.node_reproduction_max_abs());
  std::printf("n_ctrl %lld\n", static_cast<long long>(n_ctrl));
  std::printf("n_query %lld\n", static_cast<long long>(n_query));
  std::printf("n_ok %lld\n", static_cast<long long>(n_ok));
  std::printf("n_err %lld\n", static_cast<long long>(n_err));
  std::printf("n_out_of_domain %lld\n", static_cast<long long>(n_ood));
  std::printf("min_v %.17g\n", n_ok > 0 ? vmin : std::numeric_limits<double>::quiet_NaN());
  std::printf("max_v %.17g\n", n_ok > 0 ? vmax : std::numeric_limits<double>::quiet_NaN());
  std::printf("mean_v %.17g\n",
              n_ok > 0 ? vsum / static_cast<double>(n_ok)
                       : std::numeric_limits<double>::quiet_NaN());
  std::printf("first_err %s\n", first_err.c_str());

  /* ---- impulse 模式：用生产算子对单位脉冲的响应测 Σ_k w_k(q)² ----
     基场 R[v0] 已在上面的循环里算出（vals）。对节点 k 加 +eps 后重 prepare + 重新逐点求值，
     累加 ((R[v0+eps*e_k](q) - R[v0](q))/eps)²。**除法由生产算子自身完成**（无需假设线性）。 */
  std::vector<double> acc(static_cast<std::size_t>(n_query), 0.0);
  int64_t impl_used = 0;
  if (impulse_count > 0) {
    for (int64_t k = 0; k < impulse_count; ++k) {
      layer.points = base;
      layer.points[static_cast<std::size_t>(k)].snr += impulse_eps;
      SparseSnrReconstructor rk;
      std::string e2;
      if (!rk.prepare(layer, &e2)) {
        std::printf("impulse_prepare_fail_at %lld\n", static_cast<long long>(k));
        return 4;
      }
      for (int64_t q = 0; q < n_query; ++q) {
        if (mask[static_cast<std::size_t>(q)] == 0) continue;
        double v = 0.0;
        SparseReconstruction info;
        std::string e3;
        if (!rk.eval(qx[static_cast<std::size_t>(q)], qy[static_cast<std::size_t>(q)], &v,
                     &info, &e3)) {
          continue;
        }
        const double w = (v - vals[static_cast<std::size_t>(q)]) / impulse_eps;
        acc[static_cast<std::size_t>(q)] += w * w;
      }
      ++impl_used;
    }
    double s = 0.0;
    int64_t c = 0;
    for (int64_t q = 0; q < n_query; ++q) {
      if (mask[static_cast<std::size_t>(q)] == 0) continue;
      s += acc[static_cast<std::size_t>(q)];
      ++c;
    }
    std::printf("impulse_used %lld\n", static_cast<long long>(impl_used));
    std::printf("impulse_eps %.17g\n", impulse_eps);
    std::printf("impulse_mean_sumw2 %.17g\n",
                c > 0 ? s / static_cast<double>(c)
                      : std::numeric_limits<double>::quiet_NaN());
  }

  const double* outv = (impulse_count > 0) ? acc.data() : vals.data();
  std::FILE* of = std::fopen(argv[2], "wb");
  if (of == nullptr) { std::fprintf(stderr, "cannot write %s\n", argv[2]); return 2; }
  std::fwrite(outv, sizeof(double), static_cast<std::size_t>(n_query), of);
  std::fclose(of);

  const std::string mpath = std::string(argv[2]) + ".mask";
  std::FILE* mf = std::fopen(mpath.c_str(), "wb");
  if (mf == nullptr) { std::fprintf(stderr, "cannot write %s\n", mpath.c_str()); return 2; }
  std::fwrite(mask.data(), 1, static_cast<std::size_t>(n_query), mf);
  std::fclose(mf);
  return 0;
}
