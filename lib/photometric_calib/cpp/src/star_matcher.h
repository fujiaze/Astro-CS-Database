#ifndef PC_STAR_MATCHER_H
#define PC_STAR_MATCHER_H

#include <vector>
#include "wcs_transform.h"

// P12-001: 前向声明, 定义在 photometric_calib.h (C API 头文件)
struct PhotometricDiag;

namespace pc {

// 单颗星-图匹配结果
struct StarMatch {
    double x;         // 图像像素x (PSF质心)
    double y;         // 图像像素y (PSF质心)
    double f_instr;   // 仪器流量 (PSF flux)
    double f_syn;     // 合成流量 (Gaia)
    double gaia_mag;  // Gaia G 星等 (GAP-013: 用于星等一致性检查与日志)
    int psf_idx = -1; // PSF 星原始行号 (Phase1 v2: 供 per-star lineage 回连 star_id)
    int gaia_idx = -1;// Gaia 星索引 (Phase1 v2: 供 per-star DR3SP lineage)
};

// 星-图匹配器: Gaia星 <-> PSF拟合星
// GAP-013 改进:
//   - KD-tree 最近邻匹配 (替代暴力搜索, 在 Gaia 星像素坐标上建树)
//   - 距离阈值收紧到 2px (默认)
//   - 星等一致性检查 (>3 mag 拒绝)
//   - IRLS + Tukey biweight 稳健清洗 (替代 MAD, c=4.685, 50 次迭代)
//   - 亮度比例一致性日志
class StarMatcher {
public:
    StarMatcher();

    // 匹配 + IRLS/Tukey 清洗 (GAP-013 新版)
    // 参数:
    //   wcs: WCS转换器
    //   gaia_ra/dec/mag/fsyn: Gaia星数组 [n_gaia]
    //   n_gaia: Gaia星数量
    //   psf_cx/cy/flux/status: PSF星数组 [n_psf]
    //   n_psf: PSF星数量
    //   match_radius_px: 匹配半径(像素), 最近邻距离须小于该值 (默认 2.0, GAP-013 收紧)
    //   mag_tolerance: 星等一致性容忍度 (mag, 默认 3.0; |delta - median_delta| > tol 拒绝)
    //   out_scale_factor: 输出 IRLS 稳健 scale = 10^(-location(r)) (可为 nullptr)
    //                     其中 r = log10(F_instr/F_syn); scale 用于 I_cal = I * scale
    //   out_sigma_residual: 输出 sigma_residual = MAD(r_inliers)/0.6745 (可为 nullptr, 向后兼容)
    //                       供 SNR 模块 §14 计算 SNR_phot = 1/(ln10×sigma_residual)
    //   out_diag: P12-001 分阶段诊断结构体 (可为 nullptr, 向后兼容)
    //             填充阶段2/3/4/6/7/8 字段 (阶段1由 pc_api.cpp 填充)
    //   frame_width/frame_height: 图像尺寸 (默认 0, 用于阶段2 统计 gaia_projected_in_frame;
    //                             为 0 时该字段设为 n_gaia, 表示无法判定 frame)
    // 返回: 清洗后的匹配列表 (仅 IRLS inliers, Tukey 权重 > 0)
    std::vector<StarMatch> matchAndClean(
        const WcsTransform& wcs,
        const double* gaia_ra, const double* gaia_dec,
        const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
        const double* psf_cx, const double* psf_cy,
        const double* psf_flux, const int* psf_status, int n_psf,
        double match_radius_px = 2.0,
        double mag_tolerance = 3.0,
        double* out_scale_factor = nullptr,
        double* out_sigma_residual = nullptr,
        PhotometricDiag* out_diag = nullptr,
        int frame_width = 0, int frame_height = 0,
        std::vector<int>* out_match_reasons = nullptr);

    // Phase1 Full Freeze v2: matchWithKdTree / cleanAndScale 提升为 public,
    // 供 pc_api.cpp 的 run_with_gaia_impl 同时获取全量匹配与清洗后 inliers
    // (per-star lineage: star_id + DR3SP id + residual + used/reject)。
    // KD-tree 最近邻匹配: 对 Gaia 星像素坐标建 KD-tree, 对每颗 PSF 有效星找最近邻 Gaia 星
    // 返回匹配列表(未清洗)
    // out_diag (可为 nullptr): 填充阶段2/3/4/6/8 字段
    std::vector<StarMatch> matchWithKdTree(
        const WcsTransform& wcs,
        const double* gaia_ra, const double* gaia_dec,
        const double* gaia_mag, const double* gaia_fsyn, int n_gaia,
        const double* psf_cx, const double* psf_cy,
        const double* psf_flux, const int* psf_status, int n_psf,
        double match_radius_px,
        PhotometricDiag* out_diag = nullptr,
        int frame_width = 0, int frame_height = 0);

    // 星等一致性预过滤 + IRLS+Tukey 稳健清洗
    //   1) 计算每对匹配的 delta = -2.5*log10(F_instr) - gaia_mag (粗略零点差)
    //   2) 用 median(delta) 作为粗略零点, 拒绝 |delta - median_delta| > mag_tolerance 的匹配
    //   3) 对剩余匹配的 r = log10(F_instr/F_syn) 做 IRLS + Tukey biweight 稳健位置估计
    //   4) 输出 scale = 10^(-location), sigma_residual = MAD(r_inliers)/0.6745
    //   返回 IRLS inliers (Tukey 权重 > 0)
    //   out_diag (可为 nullptr): 填充阶段6/7/8 字段
    std::vector<StarMatch> cleanAndScale(
        const std::vector<StarMatch>& matches, double mag_tolerance,
        double* out_scale_factor = nullptr,
        double* out_sigma_residual = nullptr,
        PhotometricDiag* out_diag = nullptr,
        std::vector<int>* out_match_reasons = nullptr);
};

} // namespace pc

#endif // PC_STAR_MATCHER_H
