#ifndef IPV_TYPES_H
#define IPV_TYPES_H

// ============================================================================
// ipv_types.h - IPV (Iterative Polygon Voting) Phase I MVP 共享数据结构
//
// namespace ipv
// 基础结构体复用自 (vm44_types.h) / (vm45_types.h),
// 新增 IPV 多边形投票匹配专用类型 (HexDescriptor / VoteMap / PROSAC 等)
//
// 日期: 2026-07-02
// ============================================================================

#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <functional>   // std::hash (VoteKeyHash 使用)

namespace ipv {

// ===========================================================================
// 基础数据结构 (复用自 , 字段保持不变)
// ===========================================================================

// 星点 (角秒坐标, 原点在图像中心)
struct StarPoint {
    double x;          // X坐标(角秒)
    double y;          // Y坐标(角秒)
    double flux;       // 流量
    bool   saturated;  // 是否饱和
};

// 匹配对 (图像星索引 u, 星表星索引 w)
struct MatchPair {
    int u;
    int w;
};

// 相似变换参数 (s, θ, tx, ty)
struct SimTransform {
    double s;       // 尺度
    double theta;   // 旋转角(弧度)
    double tx;      // X平移(角秒)
    double ty;      // Y平移(角秒)
    bool   valid;   // 是否有效
};

// CD 矩阵 (2x2, 标准 WCS 格式)
struct CDMatrix {
    double cd11, cd12, cd21, cd22;  // CD矩阵元素
};

// SIP 多项式系数
// 前向 A/B: 6x6=36 项 (i*6+j, i+j<=order<=5), C ABI (IpvWcsResult.sip_a[36])
// 与消费方契约 (wcs_transform evalSip i*6+j) 兼容层, 布局冻结。
// 逆向 AP/BP: 同 36 项兼容层 (WCS-002 整改形态, 41 网格阶 5)。
// 扩展逆向 APx/BPx (WCS-003 布局扩展, owner 裁决 1 选 B): 10x10=100 项,
//   索引 i*10+j, 支持 i+j<=apx_order<=9; 81x81 网格高阶拟合逆映射, 供
//   迭代式反演 (wcs_sky_to_pixel_iterative) 一步初值与高畸变场逆表达。
//   追加尾部字段: A/B/AP/BP/order/ap_order 偏移与 sizeof 兼容面不变
//   (fit_sip 唯一调用点 build_wcs 显式清零全部逆向字段, 零未初始化泄漏)。
struct SIPCoeffs {
    double A[36];    // 前向 SIP A (像素→像素畸变), cd_inv · trans 高阶项
    double B[36];    // 前向 SIP B
    double AP[36];   // 逆向 SIP AP (像素→像素畸变), revtrans 网格反变换
    double BP[36];   // 逆向 SIP BP
    int    order;      // 前向 SIP 阶数 (0=无 SIP, 2/3/4)
    int    ap_order;   // 逆向 SIP 阶数 (0=无逆向 SIP, 2/3/4)
    double APx[100];   // 扩展逆向 SIP AP (i*10+j, i+j<=apx_order, WCS-003)
    double BPx[100];   // 扩展逆向 SIP BP
    int    apx_order;  // 扩展逆向 SIP 阶数 (0=无, 上限 9)
};

// ===========================================================================
// 模块间传递的中间结果结构体
// ===========================================================================

// StarSelector 输出 (Phase 0)
// 注: s0 字段复用自 (vm45_types.h)
struct StarSelection {
    std::vector<StarPoint> U;   // 图像侧星点 (角秒坐标, 50 颗)
    std::vector<StarPoint> W;   // Gaia 侧星点 (角秒坐标, ~75-150 颗)
    // 保存 Gaia 星原始 (ra, dec) 用于迭代重投影 (Task 11)
    // 与 W 一一对应, 长度 = W.size
    std::vector<double> gaia_ra;    // Gaia 星原始 RA(度) - 切平面投影前
    std::vector<double> gaia_dec;   // Gaia 星原始 Dec(度) - 切平面投影前
    // 元数据
    int    img_width;        // 图像宽度(像素)
    int    img_height;       // 图像高度(像素)
    double fov_diag_deg;     // FOV 对角线(度)
    double m_lim_final;      // 最终极限星等
    int    n_gaia_final;     // 最终 Gaia 星数 (= N_returned, 末次查询返回数)
    int    m_lim_iterations; // 极限星等迭代次数 (= query_count, Gaia 查询次数)
    // P4-magiter 新增可观测 (回归/调度用; 均由 ipv_select 各路径填充)
    int    n_fov;            // 投影过滤后 FOV 矩形内 Gaia 星数 (N_fov)
    int    gaia_query_calls; // 本帧 Gaia 圆锥查询调用次数
    double gaia_query_ms;    // 本帧 Gaia 圆锥查询累计墙钟 (ms)
    bool   m_lim_capped;     // 是否触到 Gaia 每文件返回上限 (n_ret >= 每文件上限 => 截断)
    // P14-N-10 (RQS V2-N-10 缺陷 1): 迭代失效面落交付面 (fail-closed 可观测)。
    // 旧实现只落 m_lim_capped, 且把触顶错记为 converged=true; converged /
    // query_failed 不可读 => 下游无法区分「真收敛」「查询失败」「触顶降级」。
    bool   m_lim_converged;  // |N-N_target|/N_target <= tol 且未触顶 (真收敛)
    bool   m_lim_query_failed;// 某次 Gaia 查询返回错误 (末次成功结果被采用)
    double m_lim_alpha_final;// 末次使用的 alpha (dlog10N/dmag)
    double rho_img;          // 图像侧星密度
    double rho_target;       // 目标星密度
    double s0;               // 像素尺度 (arcsec/pixel) - 新增
    bool   success;
    // 鲁棒扩增精化用, 保存全部检测星点 (网格采样选 100-300 颗)
    // 坐标同 U 约定: 像素坐标, 原点图像中心, Y 轴向上
    std::vector<StarPoint> U_full;   // 全部检测星点 (像素坐标, 原点图像中心, Y-up)
    std::vector<double>    mag_full; // 全部检测星点的 mag (box 积分), 与 U_full 一一对应
};

// WcsFitter 输出 (Phase E)
// 移除 best_mode (统一求解, 无 flip_mode 区分)
// 新增 trans_order (TRANS 阶数 1/2/3)
// 新增 ctype[2][16] (RA---TAN[-SIP] / DEC--TAN[-SIP])
struct WcsFitResult {
    CDMatrix cd;             // CD 矩阵
    double   crval[2];       // 中心赤经赤纬(度)
    double   crpix[2];       // 参考像素(1-based)
    SIPCoeffs sip;           // SIP 系数
    double   rms_px;         // RMS(像素)
    double   rms_arcsec;     // RMS(角秒)
    int      n_pairs;        // 拟合用对数
    bool     success;
    int      trans_order;    // TRANS 阶数 (1=线性, 2=二次, 3=三次)
    char     ctype[2][16];   // "RA---TAN-SIP"/"DEC--TAN-SIP" 或 "RA---TAN"/"DEC--TAN"
    // RESCUE-FD-05 / ALG-WCS-001 §11.4 F4: 失败原因 (success=false 时非空),
    // 由各失败点写入, 经 to_c_result 映射到 IpvWcsResult.error_msg。
    char     error[256];
};

// ===========================================================================
// IPV 特有类型 (多边形投票匹配)
// ===========================================================================

// 六边形描述符 (pivot + 5 邻星距离特征)
struct HexDescriptor {
    int    pivot_idx;           // pivot 星在 U 中的索引
    double distances[5];        // 5 邻星距离 (角秒, 升序)
    int    neighbor_idx[5];     // 5 邻星在 U 中的索引
};

// 候选匹配 (图像星 -> 星表星)
struct CandidateMatch {
    int    u_idx;               // 图像星索引
    int    w_idx;               // 星表星索引
    double vote;                // 票数 (: 改为 double 以支持 angle bonus 累加)
    double confidence;          // 置信度 max/(max+second+1)
};

// 投票矩阵键 (u, w) 对
struct VoteKey {
    int u;
    int w;
    bool operator==(const VoteKey& o) const { return u == o.u && w == o.w; }
};

// VoteKey 的 hash 函数
struct VoteKeyHash {
    size_t operator()(const VoteKey& k) const {
        return std::hash<int>()(k.u) * 31 + std::hash<int>()(k.w);
    }
};

// 投票矩阵类型 (稀疏 hash map)
// value 改为 double 以支持 angle bonus 累加 (Phase C 角度循环验证)
using VoteMap = std::unordered_map<VoteKey, double, VoteKeyHash>;

// PolygonMatcher 输出
struct PolygonMatchResult {
    VoteMap votes;                          // 投票矩阵
    std::vector<CandidateMatch> candidates; // 候选匹配列表 (按 vote 降序)
    int    n_pivots;                        // pivot 数
    int    n_polygon_passed;                // 通过多边形验证的候选数
    double max_vote;                        // 最大票数
    bool   success;
};

// PROSAC 验证输出
struct PROSACResult {
    SimTransform transform;                 // 最优相似变换
    std::vector<MatchPair> inliers;         // 内点匹配对
    double rms;                             // RMS (角秒)
    int    n_inliers;                       // 内点数
    int    n_iterations;                    // PROSAC 迭代次数
    double score;                           // score = n_inliers / (1 + RMS)
    bool   success;
};

// 单个 flip_mode 的结果
struct FlipModeResult {
    int            mode;                    // 0/1/2/3
    PolygonMatchResult polygon;             // 多边形匹配结果
    PROSACResult   prosac;                  // PROSAC 结果
    double         score;                   // 综合得分
    bool           success;
};

// IPVSolver 参数
struct IPVSolverParams {
    // --- 多边形匹配 ---
    int    polygon_sides = 6;               // K=6 (含 pivot)
    int    n_pivot = 30;                    // pivot 星数
    double sigma_d_arcsec = 0.0;            // 0=自适应, >0=使用此值
    int    vote_threshold = 2;              // 投票阈值

    // --- RANSAC/PROSAC ---
    int    ransac_max_iter = 2000;          // 最大迭代次数
    double ransac_inlier_threshold_arcsec = 3.0; // 内点阈值 (τ)
    double good_rms_threshold = 1.5;  // 足够好解即停阈值(角秒), best_RMS<此值且n_inliers>=4时立即终止
    double s_min = 0.90;                    // 尺度下限 (±10%, 符合项目约束)
    double s_max = 1.10;                    // 尺度上限 (±10%)

    // --- StarSelector 参数 (复用) ---
    // 默认 20 颗
    // C(60,3)=34220 vs C(20,3)=1140 (30倍), 三角形爆炸稀释投票
    // 自适应扩充 20→40→60 仅在 max_vote < vote_threshold 时触发
    int    img_n_target = 20;               // 图像侧目标星数
    double gaia_density_ratio = 1.5;        // Gaia 密度比
    double gaia_query_radius_factor = 0.55; // Gaia 查询半径因子

    // --- 极限星等割线迭代 (P4-magiter) ---
    // 替换原 m_lim_step=0.5 线性步长 (割线迭代不需要固定步长)。
    // 依据: run/perf-fix/P4-magiter/REPORT.md (实测 alpha 中位 0.2885;
    // "FOV 内 >= n_target 颗" 需 safety>=3, 10/10 帧通过)。
    // 偏差登记: docs/algorithms/IPV_PIPELINE.md proposed patch。
    // alpha = dlog10(N)/dmag 的局部斜率; 先验取自 10 帧真实 N(m) 曲线中位拟合值
    // (实测 0.243–0.456, R^2>0.986), 迭代中用相邻两次查询有限差分更新。
    double m_lim_alpha_prior = 0.2885;      // alpha 先验
    double m_lim_alpha_min = 1e-3;          // alpha 更新限幅下界
    double m_lim_alpha_max = 100.0;         // alpha 更新限幅上界
    // 目标星数倍率: N_target = n_target × m_lim_safety。
    // safety=3 是 10 帧样本上"FOV 内 >= n_target 颗"全部通过的最小值 (余量 +0.36..+1.07 mag)。
    double m_lim_safety = 3.0;
    // 初值 m0 = clamp(6 + 1.5*log10(f_mm) + 2*log10(t_s) + m_lim_m0_offset, clamp_lo, 13)
    // 该曝光公式在真实帧上系统性偏暗 3.5–5.5 mag, 故 offset 默认 -4.0。
    double m_lim_m0_exposure_s = 180.0;     // 名义曝光(s); 调用方未提供真实曝光时使用
    double m_lim_m0_offset = -4.0;          // 曝光公式系统偏差修正 (mag)
    double m_lim_clamp_lo = 6.0;            // 迭代星等下界 (低于此无星)
    double m_lim_clamp_hi = 22.0;           // 迭代星等上界 (仅迭代夹取, 不再作兜底查询值)
    double m_lim_zero_step = 3.0;           // N=0 (初值过亮) 时的 +mag 步长
    // Gaia 客户端"每文件返回上限" (lib/gaia_xpsd_client/src/gaia_client.c MAX_STARS_RESULT,
    // 该常量由 P1 持有, 本模块只读该数量用于饱和检测: 返回数为其整数倍 => collector 截断)。
    // 触顶时停用 alpha 更新并视为已达标 (防 alpha->0 使割线步长发散)。
    double m_lim_gaia_cap_per_file = 200000.0;
    int    m_lim_max_iter = 4;              // 最大 Gaia 查询次数 (原线性步长最大迭代, 语义替换)
    double density_tolerance = 0.1;         // 迭代终止相对容差 (复用)

    // --- 日志 ---
    const char* log_dir = nullptr;          // NULL=不写日志, 否则写到此目录
};

} // namespace ipv

#endif // IPV_TYPES_H
