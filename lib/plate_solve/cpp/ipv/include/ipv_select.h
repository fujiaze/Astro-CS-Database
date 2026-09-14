#ifndef IPV_SELECT_H
#define IPV_SELECT_H

// ============================================================================
// ipv_select.h - IPV StarSelector 模块 (Phase 0) 内部接口
//
// 从 vm45_select.cpp 迁移, 仅做 namespace/前缀替换:
// - namespace v45 -> ipv
// - 常量前缀 VM45_ -> IPV_
// - 主入口 vm45_select -> ipv_select
// - 类型 VM45SolveParams -> IPVSolverParams (字段名一致, 已在 ipv_types.h 定义)
// - include vm45_internal.h -> ipv_select.h (本文件已 include ipv_types.h/ipv_log.h)
//
// 核心算法 (与 一致, 从 ss_core.cpp 迁移):
// - 图像读取 (astro_image_io.dll 动态加载)
// - 星点检测 (star_detector.dll, 句柄由外部注入)
// - 图像侧选星 (饱和>50全选 / 饱和+非饱和补足50)
// - FOV/密度计算 + 自适应步长迭代极限星等
// - Gaia 锥形查询 (gaia_client.dll, 句柄由外部注入)
// - Gnomonic 正向投影 + FOV 内过滤
//
// 接口: 内部 C++ 函数, 无 ctypes 边界, 无 JSON 序列化
// 句柄: 通过 get_gaia_client_handle / get_star_detector_handle 获取
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
// 算法与 vm45_select.cpp 一致, 仅 namespace/前缀变化
// ===========================================================================

// 计算 FOV 与密度 (从 迁移)
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

// ===========================================================================
// 极限星等割线迭代 (P4-magiter) —— 替换原 estimate_mag_lim_by_density
// ---------------------------------------------------------------------------
// 背景: 原一次性密度公式 ρ(G) = 5 × 10^(1.3×(G-10)) 的 α=1.3 与真实 Gaia
// 密度实测差 4–5 个数量级 (实测 α 中位 0.2885), 导致 4/10 真实帧"FOV 内
// 不足 n_target 颗"; 且 mag=22 兜底会触发 Gaia 每文件 200000 条的顺序截断
// (科学有偏)。改用割线迭代 + 触顶防御; 证据见 run/perf-fix/P4-magiter/REPORT.md。
// ===========================================================================

// Gaia 查询回调: 对给定极限星等执行一次圆锥查询。
// 返回 0=成功 / 非 0=失败; n_returned 写出本次返回星数 (失败时可为 0)。
// 星表数组由实现捕获的引用写出 (须与调用点末次成功查询一致)。
using MagQueryFn = std::function<int(double mag, int& n_returned)>;

// 割线迭代结果 (逐项可观测, 供回归与调度)
struct MagIterOutcome {
    double m_lim_final = 0.0;    // 末次成功查询使用的极限星等 (valid 时有效)
    int    query_count = 0;      // Gaia 查询次数 (<= params.m_lim_max_iter)
    int    n_returned = 0;       // 末次成功查询返回星数 (N_returned)
    int    n_target = 0;         // 输入 n_target
    double n_target_eff = 0.0;   // n_target × m_lim_safety
    bool   converged = false;    // |N-N_target|/N_target <= tol, 或触顶视为达标
    bool   capped = false;       // 触到 Gaia 每文件返回上限 (截断, 已停用 alpha 更新)
    bool   query_failed = false; // 查询返回错误
    bool   valid = false;        // 至少有一次成功且非空查询
    double alpha_final = 0.0;    // 末次使用的 alpha
};

// 割线迭代: m_next = m + (log10(N_target) - log10(N)) / alpha
// - 初值 m0 = clamp(6 + 1.5*log10(f_mm) + 2*log10(t_s) + m_lim_m0_offset, clamp_lo, 13)
// - N_target = n_target × m_lim_safety
// - alpha 先验 m_lim_alpha_prior, 用相邻两次查询有限差分更新, 限幅 [min, max]
// - 终止: |N - N_target|/N_target <= density_tolerance, 或查询次数达 m_lim_max_iter
// - N == 0 -> m += m_lim_zero_step; 触顶 -> 停用 alpha 更新且视为达标
// - m 全程夹取 [m_lim_clamp_lo, m_lim_clamp_hi]
MagIterOutcome estimate_mag_lim_iterative(
    const MagQueryFn& query_func,
    int n_target,
    double focal_length_mm,
    double exposure_s,
    const IPVSolverParams& params,
    Logger* logger = nullptr);

// 自适应步长迭代极限星等 (线性步长, 遗留; P4-magiter 起生产路径不再使用)
void density_match_iterate(
    std::function<int(double, double, double, double)> query_func,
    double center_ra, double center_dec, double query_radius_deg,
    int n_target, double m_cut_initial,
    double step_init, int max_iter, double tolerance,
    double& final_mag_lim, int& final_n_gaia,
    int& iterations, bool& converged,
    Logger* logger = nullptr);

// 图像侧选星: 按 mag(box积分) 升序排序 (mag 越小越亮)
// flux 参数保留以备后续使用, 当前未使用 (排序基于 mag)
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
// Phase 0: StarSelector (ipv_select.cpp) - 复用 算法
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

// ============================================================================
// 候选路径 A / 路径 B 选星接口 (实验性)
//
// star_det v1 格式: FLOAT64 [N,6]
// 0: x_px 1: y_px 2: flux 3: mag 4: saturated 5: has_saturated
// ============================================================================

// 路径 B 检测结果导出 callback (C ABI 兼容)
// callback 在 sdet_detect_ex 调用后、选星前同步调用
// detections 指向 [N,6] FLOAT64 缓冲区, 调用期间有效, callback 返回后失效
typedef void (*DetectionSinkFn)(
    const double* detections,     // [N,6] FLOAT64 star_det v1
    int n_detections,
    void* user_data
);

// 路径 A: 从外部 detections 选星 (跳过 sdet_detect_ex)
// 输入: FLOAT64 [N,6] detections + 图像尺寸 + 中心指向 + 焦距/像元 + 参数
// 输出: StarSelection (U ~50 颗 + W ~75-150 颗 + 元数据)
// 返回: 0=成功, -1=失败
// 算法与 ipv_select_from_memory 一致, 区别: 跳过 float→uint16 转换和 sdet_detect_ex
int ipv_select_from_detections(
    const double* detections,     // [N,6] FLOAT64 star_det v1
    int n_detections,
    int image_width, int image_height,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    StarSelection& output,
    Logger* logger = nullptr
);

// 路径 B: 带 callback 的内存选星 (保持原有检测 + 导出检测结果)
// 输入: float* pixels + 宽高 + 中心指向 + 焦距/像元 + 参数 + callback
// 输出: StarSelection (U ~50 颗 + W ~75-150 颗 + 元数据)
// 返回: 0=成功, -1=失败
// 算法与 ipv_select_from_memory 一致, 区别: sdet_detect_ex 后调用 callback 导出检测结果
// callback 为 NULL 时行为与 ipv_select_from_memory 完全一致
int ipv_select_from_memory_with_callback(
    const float* pixels,
    int width, int height,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    DetectionSinkFn callback,
    void* user_data,
    StarSelection& output,
    Logger* logger = nullptr
);

// FP64 选星 (double 图像, 不降级 uint16/float)
int ipv_select_from_memory_with_callback_f64(
    const double* pixels,
    int width, int height,
    double ra, double dec,
    double focal_length_mm,
    double pixel_size_um,
    const IPVSolverParams& params,
    DetectionSinkFn callback,
    void* user_data,
    StarSelection& output,
    Logger* logger = nullptr
);

} // namespace ipv

#endif // IPV_SELECT_H
