#ifndef IPV_API_H
#define IPV_API_H

// ============================================================================
// ipv_api.h - IPV Plate Solver C API
//
// 将 ipv::IPVSolver (C++ 类) 封装为 extern "C" 接口, 供 Python ctypes 调用。
// 所有结构体均为 POD (固定大小数组, 无构造函数), 字符串字段使用 char[]。
//
// 编译宏 IPV_EXPORTS 控制 dllexport/dllimport:
//   - 编译 DLL 时定义 IPV_EXPORTS -> dllexport
//   - 使用 DLL 时不定义           -> dllimport
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
    int    sip_ap_order;     // 逆向 SIP 阶数 (0=无逆 SIP)  // V4.20
    double sip_ap[36];       // SIP AP 系数 (逆向)           // V4.20
    double sip_bp[36];       // SIP BP 系数 (逆向)           // V4.20
    double rms_px;            // RMS (像素)
    double rms_arcsec;        // RMS (角秒)
    int    n_pairs;          // 匹配对数
    int    success;          // 0=失败, 1=成功
    // 诊断信息
    int    n_detected;       // 检测星数
    int    n_catalog;        // 星表星数
    int    trans_order;      // TRANS 多项式阶数 (1=线性, 2=二次, 3=三次, -1=失败)
    int    best_inliers;     // 最优内点数
    char   ctype1[16];       // V4.20: "RA---TAN-SIP" / "RA---TAN"
    char   ctype2[16];       // V4.20: "DEC--TAN-SIP" / "DEC--TAN"
    char   error_msg[256];   // 错误信息
} IpvWcsResult;

// C 友好的参数结构体 (POD)
typedef struct {
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
    double m_lim_step;
    int    m_lim_max_iter;
    double density_tolerance;
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

// 获取默认参数
IPV_API void ipv_get_default_params(IpvParams* params);

#ifdef __cplusplus
}
#endif

#endif // IPV_API_H
