#ifndef IPV_TYPES_H
#define IPV_TYPES_H

// ============================================================================
// ipv_types.h - IPV (Iterative Polygon Voting) Phase I MVP 共享数据结构
//
// namespace ipv
// 基础结构体复用自 V4.4 (vm44_types.h) / V4.5 (vm45_types.h),
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
// 基础数据结构 (复用自 V4.4, 字段保持不变)
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

// SIP 多项式系数 (最多 5 阶, 6x6=36 项)
// V4.20: 新增逆向 AP/BP
struct SIPCoeffs {
    double A[36];    // 前向 SIP A (像素→像素畸变), cd_inv · trans 高阶项
    double B[36];    // 前向 SIP B
    double AP[36];   // 逆向 SIP AP (像素→像素畸变), revtrans 网格反变换
    double BP[36];   // 逆向 SIP BP
    int    order;      // 前向 SIP 阶数 (0=无 SIP, 2/3/4)
    int    ap_order;   // 逆向 SIP 阶数 (0=无逆向 SIP, 2/3/4)
};

// ===========================================================================
// 模块间传递的中间结果结构体
// ===========================================================================

// StarSelector 输出 (Phase 0)
// 注: s0 字段复用自 V4.5 (vm45_types.h)
struct StarSelection {
    std::vector<StarPoint> U;   // 图像侧星点 (角秒坐标, 50 颗)
    std::vector<StarPoint> W;   // Gaia 侧星点 (角秒坐标, ~75-150 颗)
    // V4.16: 保存 Gaia 星原始 (ra, dec) 用于迭代重投影 (Task 11)
    // 与 W 一一对应, 长度 = W.size()
    std::vector<double> gaia_ra;    // Gaia 星原始 RA(度) - 切平面投影前
    std::vector<double> gaia_dec;   // Gaia 星原始 Dec(度) - 切平面投影前
    // 元数据
    int    img_width;        // 图像宽度(像素)
    int    img_height;       // 图像高度(像素)
    double fov_diag_deg;     // FOV 对角线(度)
    double m_lim_final;      // 最终极限星等
    int    n_gaia_final;     // 最终 Gaia 星数
    int    m_lim_iterations; // 极限星等迭代次数
    double rho_img;          // 图像侧星密度
    double rho_target;       // 目标星密度
    double s0;               // 像素尺度 (arcsec/pixel) - V4.5 新增
    bool   success;
    // V4.30: 鲁棒扩增精化用, 保存全部检测星点 (网格采样选 100-300 颗)
    // 坐标同 U 约定: 像素坐标, 原点图像中心, Y 轴向上
    std::vector<StarPoint> U_full;   // 全部检测星点 (像素坐标, 原点图像中心, Y-up)
    std::vector<double>    mag_full; // 全部检测星点的 mag (box 积分), 与 U_full 一一对应
};

// WcsFitter 输出 (Phase E)
// V4.19: 移除 best_mode (统一求解, 无 flip_mode 区分)
//        新增 trans_order (TRANS 阶数 1/2/3)
// V4.20: 新增 ctype[2][16] (RA---TAN[-SIP] / DEC--TAN[-SIP])
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
    char     ctype[2][16];   // V4.20: "RA---TAN-SIP"/"DEC--TAN-SIP" 或 "RA---TAN"/"DEC--TAN"
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
    double vote;                // 票数 (V4.11: 改为 double 以支持 angle bonus 累加)
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
// V4.11: value 改为 double 以支持 angle bonus 累加 (Phase C 角度循环验证)
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

    // --- StarSelector 参数 (复用 V4.5) ---
    // V4.22: 默认 20 颗
    //        C(60,3)=34220 vs C(20,3)=1140 (30倍), 三角形爆炸稀释投票
    //        自适应扩充 20→40→60 仅在 max_vote < vote_threshold 时触发
    int    img_n_target = 20;               // 图像侧目标星数
    double gaia_density_ratio = 1.5;        // Gaia 密度比
    double gaia_query_radius_factor = 0.55; // Gaia 查询半径因子
    double m_lim_step = 0.5;                // 极限星等步长
    int    m_lim_max_iter = 10;             // 极限星等最大迭代
    double density_tolerance = 0.1;         // 密度容差

    // --- 日志 ---
    const char* log_dir = nullptr;          // NULL=不写日志, 否则写到此目录
};

} // namespace ipv

#endif // IPV_TYPES_H
