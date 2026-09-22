/* recon_dump.cpp — 稀疏重建算子 dump harness（对照实验用，非生产路径）
 *
 * 作用：把 SparseSnrReconstructor 在给定控制网格上的重建结果落盘，供
 * 与实验单元 EXP-04 的 Python 算子实现（实验/absolute-snr/code/exp04/operators.py）
 * 做逐像素对拍，证明生产实现与实验被测对象是同一个算子。
 *
 * 用法: recon_dump <spec.txt> <out.txt>
 * spec 格式（纯文本，'#' 起注释）:
 *   operator <token>                 # '-' = 不声明（走默认算子）
 *   grid <nx> <ny> <x0> <y0> <dx> <dy> <origin_x> <origin_y>
 *   values <ny*nx 个 double，行主序 j*nx+i>
 *   field <H> <W>                    # 逐像素求值，查询坐标 = 像素中心坐标 (x=j, y=i)
 *   point <x> <y>                    # 追加单点查询（可多条）
 * out 格式:
 *   第 1 行 = 头部（算子/几何/自检量）
 *   其后每行 = "i j value"（field）或 "x y value"（point）
 * 求值失败的行写 "ERR <message>"。
 */
#include "astrocs/v6/weight_chain.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using astrocs::v6::p2weight::SparseReconstruction;
using astrocs::v6::p2weight::SparseSnrLayer;
using astrocs::v6::p2weight::SparseSnrPoint;
using astrocs::v6::p2weight::SparseSnrReconstructor;

int main(int argc, char** argv) {
  if (argc < 3) {
    std::fprintf(stderr, "usage: recon_dump <spec.txt> <out.txt>\n");
    return 2;
  }
  std::ifstream in(argv[1]);
  if (!in) { std::fprintf(stderr, "cannot open spec %s\n", argv[1]); return 2; }
  std::ofstream out(argv[2]);
  if (!out) { std::fprintf(stderr, "cannot open out %s\n", argv[2]); return 2; }
  out << std::setprecision(17);

  SparseSnrLayer layer;
  layer.present = true;
  layer.regular_grid = true;
  std::vector<std::pair<double, double>> points;
  int H = 0, W = 0;

  std::string line;
  while (std::getline(in, line)) {
    const std::size_t hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    std::istringstream ss(line);
    std::string key;
    if (!(ss >> key)) continue;
    if (key == "operator") {
      std::string tok;
      ss >> tok;
      layer.reconstruction_operator = (tok == "-") ? std::string() : tok;
    } else if (key == "grid") {
      ss >> layer.nx >> layer.ny >> layer.x0 >> layer.y0 >> layer.dx >> layer.dy >>
          layer.grid_origin_x >> layer.grid_origin_y;
    } else if (key == "values") {
      const std::size_t n =
          static_cast<std::size_t>(layer.nx) * static_cast<std::size_t>(layer.ny);
      layer.points.assign(n, SparseSnrPoint());
      for (std::size_t k = 0; k < n; ++k) {
        double v = 0.0;
        ss >> v;
        const int j = static_cast<int>(k / static_cast<std::size_t>(layer.nx));
        const int i = static_cast<int>(k % static_cast<std::size_t>(layer.nx));
        SparseSnrPoint p;
        p.x = layer.x0 + static_cast<double>(i) * layer.dx;
        p.y = layer.y0 + static_cast<double>(j) * layer.dy;
        p.snr = v;
        layer.points[k] = p;
      }
    } else if (key == "field") {
      ss >> H >> W;
    } else if (key == "point") {
      double x = 0.0, y = 0.0;
      ss >> x >> y;
      points.emplace_back(x, y);
    }
  }

  SparseSnrReconstructor rec;
  std::string err;
  if (!rec.prepare(layer, &err)) {
    out << "PREPARE_ERR " << err << "\n";
    return 1;
  }
  out << "operator_id " << rec.operator_id() << "\n";
  out << "mesh_median " << (rec.mesh_median_applied() ? 1 : 0) << "\n";
  out << "clipped " << (rec.value_range_clipped() ? 1 : 0) << "\n";
  out << "clip_low " << rec.clip_low() << "\n";
  out << "clip_high " << rec.clip_high() << "\n";
  out << "n_filled " << rec.n_invalid_control_points_filled() << "\n";
  out << "cell_center_offset " << rec.cell_center_offset_max_abs() << "\n";
  out << "node_residual " << rec.node_reproduction_max_abs() << "\n";
  out << "nx " << layer.nx << " ny " << layer.ny << " H " << H << " W " << W << "\n";

  auto emit = [&](double x, double y, long long i, long long j) {
    double v = 0.0;
    SparseReconstruction info;
    std::string e;
    if (rec.eval(x, y, &v, &info, &e)) {
      out << i << " " << j << " " << v << "\n";
    } else {
      out << i << " " << j << " ERR " << e << "\n";
    }
  };
  for (int j = 0; j < H; ++j) {
    for (int i = 0; i < W; ++i) {
      emit(static_cast<double>(i), static_cast<double>(j), i, j);
    }
  }
  /* 单点查询单独成行（"P <序号> <值>" 或 "P <序号> ERR <原因>"），
     避免多条查询互相覆盖。 */
  for (std::size_t k = 0; k < points.size(); ++k) {
    double v = 0.0;
    SparseReconstruction info;
    std::string e;
    if (rec.eval(points[k].first, points[k].second, &v, &info, &e)) {
      out << "P " << k << " " << v << "\n";
    } else {
      out << "P " << k << " ERR " << e << "\n";
    }
  }
  return 0;
}
