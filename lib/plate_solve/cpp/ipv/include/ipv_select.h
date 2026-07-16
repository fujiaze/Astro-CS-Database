#ifndef IPV_SELECT_H
#define IPV_SELECT_H

// ============================================================================
// ipv_select.h - IPV StarSelector 模块 (Phase 0) 内部接口
//
// 从 V4.5 vm45_select.cpp 迁移, 仅做 namespace/前缀替换:
//   - namespace v45 -> ipv
//   - 常量前缀 VM45_ -> IPV_
//   - 主入口 vm45_select -> ipv_select
//   - 类型 VM45SolveParams -> IPVSolverParams (字段名一致, 已在 ipv_types.h 定义)
//   - include vm45_internal.h -> ipv_select.h (本文件已 include ipv_types.h/ipv_log.h)
//
// 核心算法 (与 V4.5 一致, 从 V4.2 ss_core.cpp 迁移):
//   - 图像读取 (astro_image_io.dll 动态加载)
//   - 星点检测 (star_detector.dll, 句柄由外部注入)
//   - 图像侧选星 (饱和>50全选 / 饱和+非饱和补足50)
//   - FOV/密度计算 + 自适应步长迭代极限星等
//   - Gaia 锥形查询 (gaia_client.dll, 句柄由外部注入)
//   - Gnomonic 正向投影 + FOV 内过滤
//
// 接口: 内部 C++ 函数, 无 ctypes 边界, 无 JSON 序列化
// 句柄: 通过 get_gaia_client_handle() / get_star_detector_handle() 获取
// ============================================================================

#include "ipv_types.h"
#include "ipv_log.h"
#include <string>
#include <vector>
#include <functional>

namespace ipv {

// ===========================================================================
// 全局句柄访问器 (在 ipv_entry.cpp 中定义, 由外部注入)
// ===========================================================================

// 获取注入的 GaiaClient 句柄 (void* 实际为 GaiaClient*)
// 返回 nullptr 表示未注入, 模块应返回错误
void* get_gaia_client_handle();

// 获取注入的 StarDetector 句柄 (void* 实际为 StarDetectorHandle)
// 返回 nullptr 表示未注入, 模块应返回错误
void* get_star_detector_handle();

// ===========================================================================
// 内部辅助函数 (ipv_select.cpp 中实现, 供单元测试调用)
// 算法与 V4.5 vm45_select.cpp 一致, 仅 namespace/前缀变化
// ===========================================================================

// 计算 FOV 与密度 (从 V4.5 迁移)
void compute_fov_density(
    double focal_length_mm, double pixel_size_um,
    double img_width, double img_height,
    int n_img_bright,
    double gaia_density_ratio, double gaia_query_radius_factor,
    double& s0, double& fov_diag_deg,
    double& query_radius_deg, double& query_area_sqdeg,
    double& img_area_sqdeg, double& rho_img,
    double& rho_target, int& n_target,
    Logger* logger = nullptr);

// 计算初始极限星等 m_cut
double compute_initial_mag_cut(
    double focal_length_mm, double exposure_time_s,
    Logger* logger = nullptr);

// V4.9: 基于天球平均星点密度直接估算极限星等
//   模型: ρ(G) = 5 × 10^(1.3×(G-10))  颗/平方度 (Gaia DR3 G 波段近似)
//   反解: G = 10 + log10(ρ/5) / 1.3
//   输入: n_required=所需星数, area_sqdeg=查询区域面积
//   安全余量: 在估算值基础上 +0.5 mag 保证查出的星数 > n_required
//   返回: 估算的极限星等 (clip 到 [6, 18])
double estimate_mag_lim_by_density(
    int n_required,
    double area_sqdeg,
    Logger* logger = nullptr);

// 自适应步长迭代极限星等
// V4.9: 默认不再使用, 保留作为兜底 (estimate_mag_lim_by_density 失败时)
void density_match_iterate(
    std::function<int(double, double, double, double)> query_func,
    double center_ra, double center_dec, double query_radius_deg,
    int n_target, double m_cut_initial,
    double step_init, int max_iter, double tolerance,
    double& final_mag_lim, int& final_n_gaia,
    int& iterations, bool& converged,
    Logger* logger = nullptr);

// 图像侧选星: V4.28 按 mag(box积分) 升序排序 (mag 越小越亮)
//   flux 参数保留以备后续使用, 当前未使用 (排序基于 mag)
std::vector<int> select_image_stars(
    const std::vector<double>& flux,
    const std::vector<double>& mag,
    const std::vector<bool>& saturated,
    int img_n_target,
    Logger* logger = nullptr);

// Gnomonic 正向投影
void gnomonic_forward_proj(
    double ra_deg, double dec_deg,
    double ra0_deg, double dec0_deg,
    double& xi_asec, double& eta_asec, bool& valid);

// ===========================================================================
// Phase 0: StarSelector (ipv_select.cpp) - 复用 V4.5 算法
// ===========================================================================

// 图像侧选星 + Gaia 侧不对称密度匹配查询
// 输入: FITS 路径 + 中心指向 + 焦距/像元 + 参数
// 输出: StarSelection (U ~50 颗 + W ~75-150 颗 + 元数据)
// 返回: 0=成功, -1=失败
int ipv_select(
    const std::string& image_path,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    StarSelection& output,
    Logger* logger = nullptr
);

// 从内存数据选星 (不读文件, 直接接受 float* 像素数据)
// 输入: float* pixels + 宽高 + 中心指向 + 焦距/像元 + 参数
// 输出: StarSelection (U ~50 颗 + W ~75-150 颗 + 元数据)
// 返回: 0=成功, -1=失败
int ipv_select_from_memory(
    const float* pixels,
    int width, int height,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    StarSelection& output,
    Logger* logger = nullptr
);

} // namespace ipv

#endif // IPV_SELECT_H
