#ifndef IPV_SOLVER_H
#define IPV_SOLVER_H

#include <vector>
#include <string>
#include "ipv_types.h"
#include "ipv_itertrans.h"   // Trans, IterTransResult, MatchPair
#include "ipv_select.h"      // DetectionSinkFn, ipv_select_from_detections, ipv_select_from_memory_with_callback
#include "ipv_log.h"

namespace ipv {

// ---------------------------------------------------------------------------
// 迭代重投影结果 (固定索引策略)
// ---------------------------------------------------------------------------
struct IterativeReprojectResult {
    Trans                   trans;         // 收敛后的 TRANS
    std::vector<MatchPair>  matched;       // 固定索引匹配对
    double                  ra0 = 0.0;     // 收敛后中心 RA (度)
    double                  dec0 = 0.0;    // 收敛后中心 Dec (度)
    int                     n_matched = 0; // 匹配对数
    int                     n_iterations = 0; // 迭代次数
    double                  convergence = 0.0; // 最终收敛值 (角秒)
    bool                    success = false;
};

// ---------------------------------------------------------------------------
// iterative_reproject: 迭代重投影 (固定索引策略)
//
// 流程:
// 1. apply_match: 用 TRANS 常数项 x00/y00 反推新中心
// (: TRANS 常数项已是角秒, 直接用, 不乘 s0)
// 2. project_catalog_stars: 用新中心重新 gnomonic 投影 Gaia
// 3. update_stars_positions: 按固定索引更新 W 坐标 (不重新匹配!)
// 4. atRecalcTrans: 用相同匹配对重拟合 TRANS
// 5. 收敛判定: sqrt(x00² + y00²) < 0.01" (角秒, 直接, 不乘 s0)
//
// 输入:
// U - 图像侧星点 (像素坐标, 原点图像中心)
// gaia_ra/dec - Gaia 星原始 (RA, Dec) 度
// initial_trans - 初始 TRANS (来自 iter_trans)
// initial_inliers - 初始匹配对 (固定索引, 不重新匹配)
// ra0, dec0 - 初始中心 (度)
// s0 - 像素尺度 (角秒/像素)
// img_width/height- 图像尺寸
// logger - 日志器 (可选)
//
// 输出: IterativeReprojectResult
// ---------------------------------------------------------------------------
IterativeReprojectResult iterative_reproject(
    const std::vector<StarPoint>& U,
    const std::vector<double>& gaia_ra,
    const std::vector<double>& gaia_dec,
    const Trans& initial_trans,
    const std::vector<MatchPair>& initial_inliers,
    double ra0, double dec0,
    double s0,
    int img_width, int img_height,
    Logger* logger = nullptr
);

// ---------------------------------------------------------------------------
// extract_wcs_sip: 从 TRANS 提取 WCS + SIP
//
// 流程:
// 1. CD 矩阵: (s0/3600) * M^-1, M = TRANS 线性项
// (因为 TRANS: W->U, W=xin, 所以 d(world)/d(pixel) = s0 * M^-1)
// 2. CRVAL = 收敛后中心
// 3. CRPIX = 图像中心 (1-based)
// 4. SIP: 从 TRANS 高阶项提取 (order >= 2 时)
// 5. RMS: 用最终匹配对计算
//
// 输入:
// trans - 收敛后的 TRANS
// ra0, dec0 - 收敛后中心 (度)
// img_width/height- 图像尺寸
// s0 - 像素尺度 (角秒/像素)
// U - 图像侧星点 (用于 RMS 计算)
// W - 星表侧星点 (像素, 用于 RMS 计算)
// matched - 匹配对 (用于 RMS 计算)
// logger - 日志器 (可选)
//
// 输出: *result (WcsFitResult)
// ---------------------------------------------------------------------------
void extract_wcs_sip(
    const Trans& trans,
    double ra0, double dec0,
    int img_width, int img_height,
    double s0,
    const std::vector<StarPoint>& U,
    const std::vector<StarPoint>& W,
    const std::vector<MatchPair>& matched,
    WcsFitResult* result,
    Logger* logger = nullptr
);

// ---------------------------------------------------------------------------
// v1.3: 求解器权威 inlier 缓存 (供 WCS Gate v2 双层闭环使用)
//
// 在 solve()/solve_from_memory()/solve_post_select() 末尾填充,
// 通过 C API (ipv_get_last_inlier_count / ipv_get_last_inliers) 导出,
// 用于 wcs_closure_diagnostic.py 的 --authoritative-pairs 模式。
//
// 字段对应 25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md 契约:
// matched - 最终 inlier 匹配对 (u 索引指向 U_snapshot, w 索引指向 gaia_ra/dec)
// U_snapshot - 求解用图像侧星点 (像素坐标, 原点图像中心, Y 轴向上)
// 若 robust_refine 应用, 为 selection.U_full, 否则为 selection.U
// gaia_ra/dec - Gaia 星原始 (RA, Dec) 度, 与 W 一一对应
// trans - 最终 TRANS (用于内部预测)
// s0 - 像素尺度 (角秒/像素)
// img_w/h - 图像尺寸
// ra0/dec0 - 收敛后中心 (度)
// robust_applied - 是否应用了 robust_refine_wcs (影响 u 索引空间)
// ---------------------------------------------------------------------------
struct SolveInlierCache {
    std::vector<MatchPair>  matched;
    std::vector<StarPoint>  U_snapshot;
    std::vector<double>     gaia_ra;
    std::vector<double>     gaia_dec;
    Trans                   trans;
    double                  s0 = 0.0;
    double                  ra0 = 0.0;
    double                  dec0 = 0.0;
    int                     img_width = 0;
    int                     img_height = 0;
    bool                    robust_applied = false;
    bool                    valid = false;
};

// ===========================================================================
// IPVSolver 主类
// 统一求解 (无 flip_mode 区分)
// 流程: select → triangle_match → iter_trans → iterative_reproject → extract_wcs_sip
// ===========================================================================
class IPVSolver {
public:
    IPVSolver();
    ~IPVSolver();

    // 设置 GaiaClient 句柄（由外部注入）
    void set_gaia_handle(intptr_t handle);

    // 设置 StarDetector 句柄（由外部注入）
    void set_detector_handle(intptr_t handle);

    // 主求解函数 (统一路径)
    // 输入:
    // image_path - 图像文件路径
    // ra0 - 初始指向 RA (度)
    // dec0 - 初始指向 Dec (度)
    // focal_length_mm - 焦距 (mm)
    // pixel_size_um - 像素尺寸 (um)
    // params - 求解参数
    // 输出:
    // *result - WCS 拟合结果 (通过指针返回, 避免大结构体值传递)
    void solve(
        const std::string& image_path,
        double ra0,
        double dec0,
        double focal_length_mm,
        double pixel_size_um,
        const IPVSolverParams& params,
        WcsFitResult* result
    );

    // 从内存数据求解 (不读文件, 直接接受 float* 像素数据)
    void solve_from_memory(
        const float* pixels,
        int width, int height,
        double ra0,
        double dec0,
        double focal_length_mm,
        double pixel_size_um,
        const IPVSolverParams& params,
        WcsFitResult* result
    );

    // ========================================================================
    // 候选路径 A / 路径 B 求解接口 (实验性)
    // ========================================================================

    // 路径 A: 从外部 detections 求解 (跳过 sdet_detect_ex)
    // detections: FLOAT64 [N,6] star_det v1 格式
    // 算法与 solve_from_memory 一致, 仅跳过检测步骤
    void solve_from_detections_v1(
        const double* detections,
        int n_detections,
        int image_width, int image_height,
        double ra0,
        double dec0,
        double focal_length_mm,
        double pixel_size_um,
        const IPVSolverParams& params,
        WcsFitResult* result
    );

    // 路径 B: 带 callback 的内存求解 (保持原有检测 + 导出检测结果)
    // 与 solve_from_memory 一致, 区别: sdet_detect_ex 后调用 callback 导出检测结果
    // callback 为 NULL 时行为与 solve_from_memory 完全一致
    void solve_from_memory_with_callback(
        const float* pixels,
        int width, int height,
        double ra0,
        double dec0,
        double focal_length_mm,
        double pixel_size_um,
        const IPVSolverParams& params,
        DetectionSinkFn callback,
        void* user_data,
        WcsFitResult* result
    );

    // FP64 内存求解 (double 图像, 不降级 float/uint16)
    void solve_from_memory_with_callback_f64(
        const double* pixels,
        int width, int height,
        double ra0,
        double dec0,
        double focal_length_mm,
        double pixel_size_um,
        const IPVSolverParams& params,
        DetectionSinkFn callback,
        void* user_data,
        WcsFitResult* result
    );

    // ========================================================================
    // v1.3: 权威 inlier 导出接口 (供 WCS Gate v2 双层闭环)
    //
    // 用途: 在 solve_* 之后调用, 获取求解器内部最终 inlier 对应关系,
    // 避免外部诊断工具用 kd-tree 重新匹配导致误配。
    // 详见 docs/24_WCS_VALIDATION_V2_SPEC.md 与 docs/25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md
    // ========================================================================

    // 获取最后一次成功求解的 inlier 数量
    // 返回 0 表示无缓存或求解失败
    int get_last_inlier_count() const;

    // 获取最后一次成功求解的 inlier 详细数据
    // 输出 out_buffer: 调用方分配的缓冲区, 大小 = max_count * 9 个 double
    // 每行 9 个 double 字段:
    // [0] det_x_px - 检测器 x (像素, 图像中心原点, Y 轴向上)
    // [1] det_y_px - 检测器 y
    // [2] gaia_ra_deg - Gaia RA (度)
    // [3] gaia_dec_deg - Gaia Dec (度)
    // [4] pred_x_px - 内部 TRANS 预测 x (像素, 经 s0 缩放)
    // [5] pred_y_px - 内部 TRANS 预测 y
    // [6] residual_x_px - 残差 x = det_x - pred_x (像素)
    // [7] residual_y_px - 残差 y = det_y - pred_y
    // [8] residual_dist_px - 残差距离 sqrt(res_x² + res_y²)
    // 返回实际写入的行数 (<= max_count), <0 表示错误
    int get_last_inliers(double* out_buffer, int max_count) const;

private:
    // 选星后通用求解流程 (triangle_match → iter_trans →
    // iterative_reproject → hi_order_rematch → robust_refine → extract_wcs_sip)
    // 供 solve_from_detections_v1 和 solve_from_memory_with_callback 共享
    void solve_post_select(
        StarSelection& selection,
        const IPVSolverParams& params,
        double ra0,
        double dec0,
        WcsFitResult* result
    );

    // v1.3: 填充 last_inliers_ 缓存 (供 get_last_inlier_* 读取)
    // 在每次 solve_* 成功末尾调用
    void cache_last_inliers_(
        const std::vector<MatchPair>& matched,
        const std::vector<StarPoint>& U_snapshot,
        const std::vector<double>& gaia_ra,
        const std::vector<double>& gaia_dec,
        const Trans& trans,
        double s0,
        double ra0, double dec0,
        int img_width, int img_height,
        bool robust_applied);

private:
    intptr_t gaia_handle_ = 0;
    intptr_t detector_handle_ = 0;
    Logger   logger_;

    // v1.3: 求解器最终权威 inlier 缓存
    SolveInlierCache last_inliers_;
};

} // namespace ipv

#endif // IPV_SOLVER_H
