// ACSD PSF — 空间 Moffat4 模型（PSF.md §4「空间积分响应的基础完成模型」）
//
// 正本: docs/science/psf/PSF.md §4
//   「局部拟合给出各有效星位置的 Moffat4 模型。先将每个局部模型按解析总积分
//     归一……基础空间求值采用有效星位置三角剖分内的重心凸组合: 在包含位置 u
//     的有效三角形中, 权重 lambda_s 非负且和为1, 取 P_i(u)=sum_s lambda_s(u)
//     P_i(theta_s;u), 即先把各顶点的单位通量模型平移到同一参考中心, 再按同一
//     像元边界积分并混合。不直接线性插值有周期简并的 theta。三角形只能使用已
//     通过星点质量与空间支持检查的顶点, 跨无效区、支持空洞或模型质量不足的
//     单元不得自动填满; 凸包外不外推。」
//
// 边界: 数学唯一实现仍在 PSF 库; scheduler 只消费本接口。顶点规范化
// （sx>=sy 轴约定 + theta mod pi 归约）在本模块内完成, 与 dpsf_psf.cpp 的
// 四候选收尾段不同——此处不选残差最小候选, 只做确定性重参数化。
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "acsd/pixel_integrated_psf.h"

namespace acsd {
namespace psf {

// 单个有效星顶点（调用方负责质量过滤: 拟合成功、未饱和、带宽/边界检查通过）。
struct PsfVertex {
  double x = 0.0;      // detector 0-based 中心
  double y = 0.0;
  double sx = 0.0;     // σ 口径（PSF.md §3.1）; 任意顺序, 模块内规范化
  double sy = 0.0;
  double theta = 0.0;  // rad; 任意实数, 模块内规范到 [0,pi)
  std::uint64_t star_id = 0;   // 溯源用（诊断字段, 不参与计算）
};

// Delaunay 三角剖分（Bowyer–Watson 增量 Insert + 超级三角形）。
// 退化输入（顶点 <3、共线、重合点）⇒ build 返回 false 或不产生三角形;
// 查询只接受凸包内的位置（凸包外不外推, 正本明示）。
struct DelaunayTriangulation {
  std::vector<PsfVertex> vertices;      // 输入顶点（规范化后副本）
  std::vector<std::array<int, 3>> tris; // 顶点下标三元组
  bool ok = false;
  std::string reject;                   // ok=false 时的具名原因
  // 数值退化被忽略/剔除的顶点数（重合点剔除 + Insert 未找到 bad 三角形的点）。
  // >0 表示该剖分未覆盖全部输入顶点, 必须如实登记而非静默丢弃
  // （Bowyer–Watson 审计: 极端坐标/近共圆时的可见性）。
  int dropped_points = 0;

  bool empty() const { return tris.empty(); }
};

// 顶点规范化拒绝原因（质量过滤也在此强制: 非正/非有限 sx,sy）。
// 合法顶点的规范化不改变模型值: (sx,sy,θ) 与 (sy,sx,θ+π/2) 同一模型。
bool canonicalize_vertex(PsfVertex* v, std::string* why);

// 构建三角剖分。重合点（距离 < coincident_tol）被剔除并登记。
bool build_spatial_model(const std::vector<PsfVertex>& in,
                         DelaunayTriangulation* out,
                         double coincident_tol = 1e-9);

// 空间像素响应查询请求（像元集与 pixel_integrated_moffat4_profile 同口径）。
struct SpatialResponseRequest {
  double cx = 0.0;   // 查询参考中心（亚像元相位进入求积）
  double cy = 0.0;
  const double* pixel_x = nullptr;
  const double* pixel_y = nullptr;
  std::size_t n_pixels = 0;
  int order = 10;    // 每维 Gauss-Legendre 节点数（与逐顶点求积一致）
};

struct SpatialResponseDiagnostics {
  bool ok = false;
  std::string reject;          // "outside_hull" | "no_triangle" | ...
  int tri = -1;                // 命中的三角形下标
  double lambda[3] = {0, 0, 0};  // 重心权重（非负, 和为1）
  int star_ids[3] = {0, 0, 0};   // 顶点 star_id（溯源）
  // 逐顶点模型在查询中心的采样统计（诊断; 不进入计算路径）
  double max_vertex_sigma = 0.0;   // 三顶点 sqrt(sx*sy) 最大值
  double min_vertex_sigma = 0.0;
};

// P_i(u) = Σ_s λ_s · P_i^{θ_s}(u): 每个顶点的单位总通量模型平移到同一中心 u,
// 对同一像元边界求积后按重心权重混合。Σ_j P_j(u) 不重归一（f_enc 由实际像元
// 集合决定; PSF.md §4）。
bool spatial_pixel_response(const DelaunayTriangulation& model,
                            const SpatialResponseRequest& req,
                            std::vector<double>* out_p,
                            PixelIntegrationDiagnostics* diag,
                            SpatialResponseDiagnostics* sdiag);

}  // namespace psf
}  // namespace acsd
