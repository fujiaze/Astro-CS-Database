// lib/algorithms/coverage/include/astro/phase2/coverage.h
//
// Phase2 W3：输入发现 / 兼容校验 / coverage union / target_order。
//
// 语义（冻结， 34A532A2...B2EB308 + wiki Phase2_Architecture）：
// - 输入为多个 Phase1 单帧 HiPS（signal/support/snr），不重新校准/PlateSolve/PSF/DR3SP/Drizzle；
// - 兼容：同一 equatorial/ICRS、同一 filter/passband、同一 signal/support 语义、NESTED；
// - target_order = min(所有输入最高 leaf order)，禁止低 order 插值伪装分辨率；
// - Ω = MOC_1 ∪ ... ∪ MOC_N（NESTED，允许不连通分量）。
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#ifdef _WIN32
#define P2_API __declspec(dllexport)
#else
#define P2_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    std::uint64_t order;     // leaf order（NESTED）
    std::uint64_t ipix;      // NESTED leaf pixel（tile 父单元）
} P2MocCell;

typedef struct {
    char hips_path[1024];
    char frame_id[64];       // 唯一帧标识（文件名/哈希）
    int  max_leaf_order;
    int  n_tiles;
    char filter_passband[64];
    char frame_type[32];     // equatorial / icrs
    // B2-A8: 输入帧声明的 tile 编号方案。NESTED 是本模块 union 父聚合
    // （t >> 2s）与下游消费的成立前提；显式声明非 NESTED 的输入在
    // inspect_frame 即失败（fail-closed），空串 = 上游未声明（AIO 写侧恒
    // NESTED），仅作审计面登记。
    char hips_ordering[16];
} P2HipsInputInfo;

typedef struct {
    std::uint64_t n_inputs;
    P2HipsInputInfo* inputs;         // 调用方分配 n_inputs
    std::uint64_t n_union_cells;
    P2MocCell* union_cells;          // 调用方分配（先 n_union_cells=0 查询）
    int  target_order;
    int  status;                     // 0=ok
    char error[512];
} P2CoverageResult;

// 发现并校验输入 HiPS，计算 union MOC 与 target_order。
// 两次调用：第一次 n_union_cells=0 查询所需容量；第二次传入容量。
P2_API int p2_coverage_build(
    const char* const* hips_paths, std::uint64_t n_inputs,
    P2CoverageResult* out);

P2_API int p2_coverage_free(P2CoverageResult* out);

// ===========================================================================
// V6 目标态：coverage/support 区分、确定性边界与权重角色门
// ---------------------------------------------------------------------------
// 冻结锚：
//   FZ-GATE-SUPPORT-COVERAGE : support/coverage 只作门（gate），不得作
//                              inverse-variance / SNR / 科学权重 / variance；
//                              进 weight.sources 即 REJECT；
//   FZ-GATE-MEDIAN-SNR       : median(SNR_F)/median_source_snr/fwhm/residual
//                              仅诊断，进权重面即 REJECT（C-004.2）；
//   FZ-WEIGHT-SINGLE-PATH    : 权重只有一个口径，没有可选择项 ——
//                              Phase1 产稀疏 SNR 控制点 → Phase2 重建稠密 SNR 面
//                              → 取逆方差（最优功率）定权 → 叠加；
//                              w = SNR^2 / F_ref^2（ASTROCS_DESIGN.md §3.1、
//                              docs/science/PSF_SIGNAL_WEIGHT.md §4）；
//   FZ-MODE-RETIRED          : psfsw_robust 不得进生产路由 —— psfsw_robust_weight
//                              不是现行对象（ASTROCS_DESIGN.md §3.1；UNIFIED_MODEL.md:58），
//                              显式拒绝 + 迁移提示（唯一口径 = 逆方差）；
//   FZ-FIELD-WEIGHTMODE      : legacy 整数 0(=support x snr^2) 不得进科学
//                              权重面；
//   DESIGN-P2 §8             : 分块/并行只改执行，不改归约次序；跨 tile
//                              边界状态有确定边界协议；缺失不得静默零填。
// 语义源：eng/contracts/data/v6_clause_registry_v1.json
//         （forbidden.weight_source_tokens）
//         + ASTROCS_DESIGN.md §3.1（权重判据）与
//         docs/science/PSF_SIGNAL_WEIGHT.md §1/§4（单一权重口径；退役对象的拒绝面见
//         coverage.cpp 的 RETIRED-OBJECT-REJECT 注释块）。本头文件不得另造词表（C-004.3）。
//
// 单位（冻结表，本模块不重定义）：signal_sb=ADU/sr、sb_variance_out=
// ADU^2/sr^2、sb_ivar_out=sr^2/ADU^2、W_info=ADU^-2。
// 历史单位项 psfsw_robust_weight=1 随该对象退役（本模块不消费、不接受；仅作
// 拒绝面 token 登记）。support/coverage 无量纲且不是 weight。
// ===========================================================================

// 输出元素状态：封闭枚举。缺失/NaN 规范以 P2_CELL_UNAVAILABLE 显式表达，
// 禁止零填成 P2_CELL_COVERED_UNSUPPORTED（否则"未知"被冒充为"已知无支撑"）。
typedef enum {
    P2_CELL_UNCOVERED           = 0,  // 不在 coverage union 内
    P2_CELL_COVERED_UNSUPPORTED = 1,  // 在 union 内，支撑已测知但 < min_support_frames
    P2_CELL_SUPPORTED           = 2,  // 在 union 内，支撑已测知且 >= min_support_frames
    P2_CELL_UNAVAILABLE         = 3   // coverage/support 缺失或 NaN：fail-closed，
                                      // 不得零填、不得当"无支撑"或"有支撑"使用
} P2CellState;

// coverage/support 判定输入。两独立面：
//   - coverage      : union 覆盖指示（1=在 Ω 内）；缺失以 coverage_known=0 表示；
//   - support_frames: 该元素的有效支撑帧计数（来自各帧 support 层）；
//   - support_known : 1=计数已测知；0=缺失/NaN（禁止用 support_frames=0 冒充）。
// support_frames < min_support_frames 只影响门判定，绝不产生任何权重。
typedef struct {
    std::uint64_t n_cells;
    const std::uint8_t*  coverage;        // 长度 n_cells，1=在 union 内
    const std::uint8_t*  coverage_known;  // 长度 n_cells，1=已测知（可空=全部已知）
    const std::uint32_t* support_frames;  // 长度 n_cells，有效支撑帧计数
    const std::uint8_t*  support_known;   // 长度 n_cells，1=已测知（不得可空）
    std::uint32_t min_support_frames;     // 门（只作门，非权重）
} P2CoverageSupportInput;

// 确定性分类。同一输入 → 逐位相同输出，与遍历/分块/worker 数无关。
// 规则（fail-closed）：
//   coverage_known==0 或 support_known==0 → P2_CELL_UNAVAILABLE；
//   coverage==0                            → P2_CELL_UNCOVERED；
//   support_frames < min_support_frames     → P2_CELL_COVERED_UNSUPPORTED；
//   否则                                    → P2_CELL_SUPPORTED。
// out_* 计数均可空；out_n_unavailable 统计缺失（未被零填）。
// 返回 0=ok；1=参数错误（空输入/空 out_state/n_cells==0）。
P2_API int p2_coverage_support_classify(
    const P2CoverageSupportInput* in,
    P2CellState* out_state,
    std::uint64_t* out_n_supported,
    std::uint64_t* out_n_covered_unsupported,
    std::uint64_t* out_n_uncovered,
    std::uint64_t* out_n_unavailable,
    char* err, std::size_t err_size);

// 跨 tile 边界的确定性归属（DESIGN-P2 §8）。给定叶级 ipix 与父聚合位移
// leaf_shift（父 order 比叶 order 粗 leaf_shift 级），owner = ipix >> (2*leaf_shift)。
// 边界（同父）叶节点因此恰好归属同一唯一 owner，与进入方向/worker 无关。
// leaf_shift<0 或 shift 过大（2*leaf_shift>=64）→ 返回 UINT64_MAX（非法哨兵）。
P2_API std::uint64_t p2_tile_boundary_owner(std::uint64_t leaf_ipix,
                                            int leaf_shift);

// 确定性规约序：把（可能无序、可重复的）ipix 集合规范化为升序去重序列。
// 同一集合 → 同一序列（与输入顺序、分块方式、worker 数无关）。
// capacity 不足时返回 1 且 out_n 写真实需求（probe/fill 协议）。
// out_n 可空。输入含重复项不报错（去重）。
P2_API int p2_deterministic_reduction_order(
    const std::uint64_t* ipix_in, std::uint64_t n_in,
    std::uint64_t* out_sorted, std::uint64_t capacity,
    std::uint64_t* out_n,
    char* err, std::size_t err_size);

// 权重来源 token 门（FZ-GATE-SUPPORT-COVERAGE / FZ-GATE-MEDIAN-SNR /
// FZ-FIELD-WEIGHTMODE）。逐 token 做大小写不敏感全等匹配冻结集合
// forbidden.weight_source_tokens（另含 psfsw_robust_weight/psfsw 等）；
// 任一命中 → rc=1 + 原因（token 名）。token==NULL/空串跳过。
// 本函数只做拒绝，不产生权重，也不重排/改写词表。
// 注（退役对象拒绝面）：集合里的 psfsw_robust_weight/psfsw 自 2026-09 负责人裁决
// 起不再表示"在役的相对复合权重"，而是**退役对象的显式拒绝面**——旧产品若把该
// 对象写进 weight.sources/variance_from，必须在此判红（不得静默接受）。该两 token
// 因此不得按"残留清理"删除。依据 ASTROCS_DESIGN.md §3.1（订正后）+
// docs/design/UNIFIED_MODEL.md:58（旧产品声明该对象 ⇒ 显式拒绝 + 迁移提示）。
// 返回 0=通过；1=命中禁止 token；2=参数错误（tokens==NULL 且 n>0）。
P2_API int p2_weight_source_token_reject(
    const char* const* tokens, std::uint64_t n,
    char* err, std::size_t err_size);

#ifdef __cplusplus
}
#endif