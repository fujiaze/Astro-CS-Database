#ifndef IPV_API_H
#define IPV_API_H

// ============================================================================
// ipv_api.h - IPV Plate Solver C API
//
// 将 ipv::IPVSolver (C++ 类) 封装为 extern "C" 接口, 供 Python ctypes 调用。
// 所有结构体均为 POD (固定大小数组, 无构造函数), 字符串字段使用 char[]。
//
// 编译宏 IPV_EXPORTS 控制 dllexport/dllimport:
// - 编译 DLL 时定义 IPV_EXPORTS -> dllexport
// - 使用 DLL 时不定义 -> dllimport
//
// 日期: 2026-07-02
// ============================================================================

#ifdef _WIN32
    #ifdef IPV_EXPORTS
        #define IPV_API __declspec(dllexport)
    #else
        #define IPV_API __declspec(dllimport)
    #endif
#else
    #define IPV_API
#endif

// intptr_t 类型 (C/C++ 兼容)
#ifdef __cplusplus
    #include <cstdint>
#else
    #include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// C 友好的 WCS 结果结构体 (POD)
typedef struct {
    double cd[4];           // CD 矩阵 [cd1_1, cd1_2, cd2_1, cd2_2]
    double crval[2];         // CRVAL [ra, dec] (度)
    double crpix[2];         // CRPIX [x, y] (1-based)
    int    sip_order;        // 前向 SIP 阶数 (0=无 SIP)
    double sip_a[36];        // SIP A 系数 (前向)
    double sip_b[36];        // SIP B 系数 (前向)
    int    sip_ap_order;     // 逆向 SIP 阶数 (0=无逆 SIP) //
    double sip_ap[36];       // SIP AP 系数 (逆向) //
    double sip_bp[36];       // SIP BP 系数 (逆向) //
    double rms_px;            // RMS (像素)
    double rms_arcsec;        // RMS (角秒)
    int    n_pairs;          // 匹配对数
    int    success;          // 0=失败, 1=成功
    // 诊断信息
    int    n_detected;       // 检测星数
    int    n_catalog;        // 星表星数
    int    trans_order;      // TRANS 多项式阶数 (1=线性, 2=二次, 3=三次, -1=失败)
    int    best_inliers;     // 最优内点数
    char   ctype1[16];       // "RA---TAN-SIP" / "RA---TAN"
    char   ctype2[16];       // "DEC--TAN-SIP" / "DEC--TAN"
    char   error_msg[256];   // 错误信息
} IpvWcsResult;

// C 友好的参数结构体 (POD)
//
// ABI 契约 (宪章 §8.6 "跨 DLL 边界使用版本化 C ABI ... 结构体带
// struct_size/abi_version"): 首部两个 uint32_t 自描述字段。
//   - 调用方应先调用 ipv_get_default_params() 填充 (该函数会写入这两个字段),
//     或自行构造时显式设置
//         params.struct_size = sizeof(IpvParams);
//         params.abi_version = IPV_PARAMS_ABI_VERSION;
//   - 所有公共入口在 params != NULL 时校验二者; 任一不匹配一律 fail-closed
//     (返回 0 并写 error_msg), 绝不按本结构体尺寸去写更小的调用方缓冲。
//   - 字段增删/重排/改类型 => 必须同步抬高 IPV_PARAMS_ABI_VERSION, 并同步
//     lib/plate_solve/tools/ipv_abi_mirror.py; ctest 门 ipv_abi_layout_lock
//     会用 C 探针 sizeof/offsetof 机器校验二者逐字段一致。
#define IPV_PARAMS_ABI_VERSION 2u

typedef struct {
    /* --- ABI 自描述 (宪章 §8.6; 必须在首部, 偏移 0/4) --- */
    uint32_t struct_size;            /* = sizeof(IpvParams) */
    uint32_t abi_version;            /* = IPV_PARAMS_ABI_VERSION */
    /* --- 求解参数 --- */
    int    polygon_sides;
    int    n_pivot;
    double sigma_d_arcsec;
    int    vote_threshold;
    int    ransac_max_iter;
    double ransac_inlier_threshold_arcsec;
    double s_min;
    double s_max;
    int    img_n_target;
    double gaia_density_ratio;
    double gaia_query_radius_factor;
    /* 极限星等割线迭代参数 (P4-magiter; 与 ipv::IPVSolverParams 逐字段对应)。
     * 原 m_lim_step (线性步长) 已由 m_lim_alpha_prior 替换 (割线迭代无需固定步长)。 */
    double m_lim_alpha_prior;        /* alpha 先验 (dlog10(N)/dmag), 默认 0.2885 */
    double m_lim_alpha_min;          /* alpha 更新限幅下界, 默认 1e-3 */
    double m_lim_alpha_max;          /* alpha 更新限幅上界, 默认 100 */
    double m_lim_safety;             /* N_target = n_target × safety, 默认 3 */
    double m_lim_m0_exposure_s;      /* m0 初值名义曝光(s), 默认 180 */
    double m_lim_m0_offset;          /* 曝光公式偏差修正(mag), 默认 -4.0 */
    double m_lim_clamp_lo;           /* 迭代星等下界, 默认 6 */
    double m_lim_clamp_hi;           /* 迭代星等上界, 默认 22 */
    double m_lim_zero_step;          /* N=0 时 +mag 步长, 默认 3 */
    double m_lim_gaia_cap_per_file;  /* 触顶检测: Gaia 每文件返回上限, 默认 200000 */
    int    m_lim_max_iter;           /* 最大 Gaia 查询次数, 默认 4 */
    double density_tolerance;        /* 迭代终止相对容差, 默认 0.1 */
    char   log_dir[256];     // 空字符串=不写日志
} IpvParams;

// 创建求解器实例
// 返回: 非零=句柄, 0=失败
IPV_API void* ipv_solve_create(void);

// 销毁求解器实例
IPV_API void ipv_solve_destroy(void* solver);

// 设置 GaiaClient 句柄
IPV_API void ipv_set_gaia_handle(void* solver, intptr_t handle);

// 设置 StarDetector 句柄
IPV_API void ipv_set_detector_handle(void* solver, intptr_t handle);

// 执行求解
// 返回: 0=失败, 1=成功 (结果写入 result)
IPV_API int ipv_solve(
    void* solver,
    const char* image_path,      // 图像路径 (UTF-8)
    double ra0,                   // 初始指向 RA (度)
    double dec0,                  // 初始指向 Dec (度)
    double focal_length_mm,       // 焦距 (mm)
    double pixel_size_um,         // 像素尺寸 (um)
    const IpvParams* params,      // 参数 (NULL=用默认值)
    IpvWcsResult* result          // 输出结果
);

// 从内存数据执行求解 (不读文件, 直接接受 float* 像素数据)
// 返回: 0=失败, 1=成功 (结果写入 result)
IPV_API int ipv_solve_from_memory(
    void* solver,
    const float* pixels,          // 像素数据 (float32, row-major)
    int width,                    // 图像宽度
    int height,                   // 图像高度
    double ra0,                   // 初始指向 RA (度)
    double dec0,                  // 初始指向 Dec (度)
    double focal_length_mm,       // 焦距 (mm)
    double pixel_size_um,         // 像素尺寸 (um)
    const IpvParams* params,      // 参数 (NULL=用默认值)
    IpvWcsResult* result          // 输出结果
);

// ============================================================================
// 候选路径 A / 路径 B API (实验性, 生产默认不调用)
//
// 规范: docs/05_STAR_DETECT_PSF_DEDUP_SPEC.md
// star_det v1 格式: FLOAT64 [N,6]
// 0: x_px 1: y_px 2: flux
// 3: mag 4: saturated 5: has_saturated
// ============================================================================

// 路径 B callback: 导出 PlateSolve 内部 sdet_detect_ex 检测结果
// callback 在 sdet_detect_ex 调用后、选星前同步调用
// callback 返回后源指针失效, 调用方必须在 callback 内复制数据
// detections 指向 [N,6] FLOAT64 缓冲区, 由求解器在调用期间拥有
typedef void (*IpvDetectionCallback)(
    const double* detections,     // [N,6] FLOAT64 star_det v1
    int n_detections,
    void* user_data
);

// 路径 A 候选 API: 从外部 detections 求解 (跳过 sdet_detect_ex)
// detections: FLOAT64 [N,6] star_det v1 格式
// 算法逻辑与 ipv_solve_from_memory 完全一致, 仅跳过检测步骤
// 返回: 0=失败, 1=成功 (结果写入 result)
IPV_API int ipv_solve_from_detections_v1(
    void* solver,
    const double* detections,     // [N,6] FLOAT64 star_det v1
    int n_detections,
    int image_width,              // 图像宽度 (像素)
    int image_height,             // 图像高度 (像素)
    double ra0,                   // 初始指向 RA (度)
    double dec0,                  // 初始指向 Dec (度)
    double focal_length_mm,       // 焦距 (mm)
    double pixel_size_um,         // 像素尺寸 (um)
    const IpvParams* params,      // 参数 (NULL=用默认值)
    IpvWcsResult* result          // 输出结果
);

// 路径 B API: 带 callback 的内存求解 (保持原有数据流 + 导出检测)
// 与 ipv_solve_from_memory 算法完全一致, 区别:
// - 在 sdet_detect_ex 调用后、选星前调用 callback 导出完整检测结果
// - callback 为 NULL 时行为与 ipv_solve_from_memory 完全一致
// 返回: 0=失败, 1=成功 (结果写入 result)
IPV_API int ipv_solve_from_memory_with_callback(
    void* solver,
    const float* pixels,          // 像素数据 (float32, row-major)
    int width,                    // 图像宽度
    int height,                   // 图像高度
    double ra0,                   // 初始指向 RA (度)
    double dec0,                  // 初始指向 Dec (度)
    double focal_length_mm,       // 焦距 (mm)
    double pixel_size_um,         // 像素尺寸 (um)
    const IpvParams* params,      // 参数 (NULL=用默认值)
    IpvDetectionCallback callback, // 检测结果导出回调 (NULL=不导出)
    void* user_data,              // 回调用户数据
    IpvWcsResult* result          // 输出结果
);

// FP64 内存求解 (double 图像, 不降级 float/uint16)
// 算法与 ipv_solve_from_memory_with_callback 完全一致
IPV_API int ipv_solve_from_memory_with_callback_d(
    void* solver,
    const double* pixels,         // 像素数据 (float64, row-major)
    int width,
    int height,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IpvParams* params,
    IpvDetectionCallback callback,
    void* user_data,
    IpvWcsResult* result
);

// 获取默认参数
IPV_API void ipv_get_default_params(IpvParams* params);

// ============================================================================
// v1.3: 权威 inlier 导出 C API (供 WCS Gate v2 双层闭环)
//
// 用途: 在 ipv_solve/ipv_solve_from_memory/ipv_solve_from_detections_v1/
// ipv_solve_from_memory_with_callback 成功返回后调用,
// 获取求解器内部最终权威 inlier 对应关系,
// 避免外部诊断工具用 kd-tree 重新匹配导致误配。
//
// 详见 docs/24_WCS_VALIDATION_V2_SPEC.md 与 docs/25_AUTHORITATIVE_MATCH_PAIR_CONTRACT.md
//
// 字段约定 (out_buffer 每行 9 个 double, 行数 = 返回值):
// [0] det_x_px - 检测器 x (像素, 图像中心原点, Y 轴向上)
// [1] det_y_px - 检测器 y
// [2] gaia_ra_deg - Gaia RA (度)
// [3] gaia_dec_deg - Gaia Dec (度)
// [4] pred_x_px - 内部 TRANS 预测 x (像素, 经 s0 缩放)
// [5] pred_y_px - 内部 TRANS 预测 y
// [6] residual_x_px - 残差 x = det_x - pred_x (像素)
// [7] residual_y_px - 残差 y = det_y - pred_y
// [8] residual_dist_px - 残差距离 sqrt(res_x² + res_y²)
// ============================================================================

// 获取最后一次成功求解的 inlier 数量
// 返回: >=0 inlier 数, 0 表示无缓存或求解失败
IPV_API int ipv_get_last_inlier_count(void* solver);

// 获取最后一次成功求解的 inlier 详细数据
// 输入:
// solver - 求解器句柄
// out_buffer - 调用方分配的缓冲区, 大小 = max_count * 9 * sizeof(double)
// max_count - 缓冲区最多容纳的行数
// 返回: >=0 实际写入的行数, <0 表示错误 (如 buffer 为空或 max_count<=0)
IPV_API int ipv_get_last_inliers(void* solver, double* out_buffer, int max_count);

#ifdef __cplusplus
}
#endif

#endif // IPV_API_H
