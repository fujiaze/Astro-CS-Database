// ============================================================================
// ipv_entry.cpp - IPV C API 入口实现
//
// 将 ipv::IPVSolver (C++ 类) 封装为 extern "C" 接口, 供 Python ctypes 调用。
//
// 职责:
// 1. 实例生命周期管理 (create/destroy)
// 2. 句柄注入 (GaiaClient / StarDetector)
// 3. POD 参数 <-> IPVSolverParams 转换
// 4. WcsFitResult -> IpvWcsResult 字段映射
// 5. 异常捕获, 防止 C++ 异常泄漏到 C 边界
//
// 注意: IPVSolver 内部诊断信息 (n_detected/n_catalog/best_inliers)
// 未通过 public 接口暴露, 这里填充为安全默认值 (success=false 时 -1/0)。
// 如需暴露, 应在 IPVSolver 增加访问器后此处同步映射。
// best_mode (flip_mode) 已移除, 替换为 trans_order (TRANS 多项式阶数 1/2/3)
//
// 日期: 2026-07-02
// ============================================================================

#include "ipv_api.h"
#include "ipv_solver.h"
#include "ipv_select.h"   // 全局句柄访问器 set_gaia_client_handle / set_star_detector_handle
#include "ipv_types.h"

#include <cstring>
#include <cstdio>   // std::fprintf/std::fflush (崩溃诊断)
#include <string>
#include <new>
#include <memory>

// ---------------------------------------------------------------------------
// 内部辅助: 全局句柄存储
//
// ipv::ipv_select 通过 get_gaia_client_handle() / get_star_detector_handle()
// 读取注入的句柄。IPVSolver 本身也持有副本 (用于日志诊断),
// 这里在 set_xxx_handle 调用时同步设置全局访问器, 保证 select 阶段可用。
// ---------------------------------------------------------------------------

namespace {

// 全局句柄存储 (内部链接)
void* g_gaia_handle    = nullptr;
void* g_detector_handle = nullptr;

} // namespace

// 覆盖 ipv_select.h 中声明的全局访问器 (链接到本文件的内部存储)
namespace ipv {

void* get_gaia_client_handle() {
    return g_gaia_handle;
}

void* get_star_detector_handle() {
    return g_detector_handle;
}

} // namespace ipv

// ---------------------------------------------------------------------------
// 内部辅助: POD 参数 <-> IPVSolverParams 转换
// ---------------------------------------------------------------------------

namespace {

// IpvParams (POD) -> IPVSolverParams
// log_dir 字段: 空字符串视为 nullptr (不写日志)
ipv::IPVSolverParams to_solver_params(const IpvParams* src) {
    ipv::IPVSolverParams dst;
    if (src == nullptr) {
        return dst;  // 使用默认值
    }
    dst.polygon_sides                = src->polygon_sides;
    dst.n_pivot                      = src->n_pivot;
    dst.sigma_d_arcsec               = src->sigma_d_arcsec;
    dst.vote_threshold               = src->vote_threshold;
    dst.ransac_max_iter              = src->ransac_max_iter;
    dst.ransac_inlier_threshold_arcsec = src->ransac_inlier_threshold_arcsec;
    dst.s_min                        = src->s_min;
    dst.s_max                        = src->s_max;
    dst.img_n_target                 = src->img_n_target;
    dst.gaia_density_ratio           = src->gaia_density_ratio;
    dst.gaia_query_radius_factor     = src->gaia_query_radius_factor;
    dst.m_lim_step                   = src->m_lim_step;
    dst.m_lim_max_iter               = src->m_lim_max_iter;
    dst.density_tolerance            = src->density_tolerance;

    // log_dir: 空字符串 -> nullptr
    if (src->log_dir[0] != '\0') {
        dst.log_dir = src->log_dir;  // const char* 指向 POD 内部缓冲区, 调用期间有效
    } else {
        dst.log_dir = nullptr;
    }
    return dst;
}

// WcsFitResult -> IpvWcsResult (字段映射 + 诊断填充)
void to_c_result(const ipv::WcsFitResult& src, IpvWcsResult* dst) {
    // 零初始化 (清空 error_msg 等)
    std::memset(dst, 0, sizeof(IpvWcsResult));

    // CD 矩阵
    dst->cd[0] = src.cd.cd11;
    dst->cd[1] = src.cd.cd12;
    dst->cd[2] = src.cd.cd21;
    dst->cd[3] = src.cd.cd22;

    // CRVAL / CRPIX
    dst->crval[0] = src.crval[0];
    dst->crval[1] = src.crval[1];
    dst->crpix[0] = src.crpix[0];
    dst->crpix[1] = src.crpix[1];

    // SIP (前向 A/B + 逆向 AP/BP)
    dst->sip_order = src.sip.order;
    std::memcpy(dst->sip_a, src.sip.A, sizeof(double) * 36);
    std::memcpy(dst->sip_b, src.sip.B, sizeof(double) * 36);
    dst->sip_ap_order = src.sip.ap_order;
    std::memcpy(dst->sip_ap, src.sip.AP, sizeof(double) * 36);
    std::memcpy(dst->sip_bp, src.sip.BP, sizeof(double) * 36);
    // ctype
    std::memcpy(dst->ctype1, src.ctype[0], 16);
    std::memcpy(dst->ctype2, src.ctype[1], 16);

    // RMS / 对数 / 成功标志
    dst->rms_px     = src.rms_px;
    dst->rms_arcsec = src.rms_arcsec;
    dst->n_pairs    = src.n_pairs;
    dst->success    = src.success ? 1 : 0;

    // 诊断信息: trans_order 已通过 WcsFitResult.trans_order 暴露, 直接映射;
    // n_detected/n_catalog 暂未通过 public 接口暴露, 保留为 0。
    dst->n_detected   = 0;
    dst->n_catalog    = 0;
    dst->trans_order  = src.trans_order;
    dst->best_inliers = src.n_pairs;
}

// 安全写入错误信息到固定大小 char[]
void set_error_msg(char* dst, size_t dst_size, const char* msg) {
    if (dst == nullptr || dst_size == 0) return;
    if (msg == nullptr) {
        dst[0] = '\0';
        return;
    }
    // 截断保护
    size_t n = std::strlen(msg);
    if (n >= dst_size) n = dst_size - 1;
    std::memcpy(dst, msg, n);
    dst[n] = '\0';
}

// 修复: 将 solve() 调用 + try/catch 隔离到独立函数
// 动机: ipv_solve 的 try/catch 在栈上生成 SEH 记录, solve() 内部的大栈使用
// (FlipModeResult results[4] 等) 可能覆盖 SEH 记录, 导致返回到 ctypes 时崩溃。
// 将 try/catch 移到 do_solve_impl, ipv_solve 本身无 try/catch, 栈帧上无 SEH 记录。
// 进一步修复: solve() 改为通过指针返回结果 (void solve(..., WcsFitResult* result)),
// 完全避免 WcsFitResult (~680 字节) 的值传递导致的栈崩溃。
int do_solve_impl(
    ipv::IPVSolver* s,
    const char* image_path,
    double ra0, double dec0,
    double focal_length_mm, double pixel_size_um,
    const ipv::IPVSolverParams& sp,
    IpvWcsResult* result
) {
    try {
        // 堆分配 WcsFitResult 以减少栈使用
        auto wcs_ptr = std::make_unique<ipv::WcsFitResult>();
        ipv::WcsFitResult& wcs = *wcs_ptr;
        s->solve(
            std::string(image_path),
            ra0, dec0,
            focal_length_mm, pixel_size_um,
            sp,
            &wcs
        );
        to_c_result(wcs, result);
    } catch (const std::bad_alloc& e) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), e.what());
        return 0;
    } catch (const std::exception& e) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), e.what());
        return 0;
    } catch (...) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), "未知 C++ 异常");
        return 0;
    }
    return result->success;
}

// 内存接口版本: 与 do_solve_impl 一致, 但调用 s->solve_from_memory
// 避免中间 FITS 文件, 直接接受 float* 像素数据
int do_solve_from_memory_impl(
    ipv::IPVSolver* s,
    const float* pixels,
    int width, int height,
    double ra0, double dec0,
    double focal_length_mm, double pixel_size_um,
    const ipv::IPVSolverParams& sp,
    IpvWcsResult* result
) {
    try {
        // 堆分配 WcsFitResult 以减少栈使用
        auto wcs_ptr = std::make_unique<ipv::WcsFitResult>();
        ipv::WcsFitResult& wcs = *wcs_ptr;
        s->solve_from_memory(
            pixels,
            width, height,
            ra0, dec0,
            focal_length_mm, pixel_size_um,
            sp,
            &wcs
        );
        to_c_result(wcs, result);
    } catch (const std::bad_alloc& e) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), e.what());
        return 0;
    } catch (const std::exception& e) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), e.what());
        return 0;
    } catch (...) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), "未知 C++ 异常");
        return 0;
    }
    return result->success;
}

} // namespace

// ===========================================================================
// C API 实现
// ===========================================================================

// 创建求解器实例
IPV_API void* ipv_solve_create(void) {
    try {
        ipv::IPVSolver* solver = new ipv::IPVSolver();
        return static_cast<void*>(solver);
    } catch (const std::bad_alloc&) {
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

// 销毁求解器实例
IPV_API void ipv_solve_destroy(void* solver) {
    if (solver == nullptr) return;
    try {
        ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
        delete s;
    } catch (...) {
        // 析构不应抛异常, 吞掉以防泄漏到 C 边界
    }
}

// 设置 GaiaClient 句柄
IPV_API void ipv_set_gaia_handle(void* solver, intptr_t handle) {
    if (solver == nullptr) return;
    try {
        ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
        s->set_gaia_handle(handle);
        // 同步设置全局访问器 (供 ipv_select 使用)
        g_gaia_handle = reinterpret_cast<void*>(handle);
    } catch (...) {
        // 吞掉异常, 防止泄漏到 C 边界
    }
}

// 设置 StarDetector 句柄
IPV_API void ipv_set_detector_handle(void* solver, intptr_t handle) {
    if (solver == nullptr) return;
    try {
        ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
        s->set_detector_handle(handle);
        // 同步设置全局访问器 (供 ipv_select 使用)
        g_detector_handle = reinterpret_cast<void*>(handle);
    } catch (...) {
        // 吞掉异常, 防止泄漏到 C 边界
    }
}

// 获取默认参数 (与 IPVSolverParams 默认值一致)
IPV_API void ipv_get_default_params(IpvParams* params) {
    if (params == nullptr) return;
    std::memset(params, 0, sizeof(IpvParams));

    ipv::IPVSolverParams def;  // 使用 C++ 默认值

    params->polygon_sides                  = def.polygon_sides;
    params->n_pivot                        = def.n_pivot;
    params->sigma_d_arcsec                 = def.sigma_d_arcsec;
    params->vote_threshold                 = def.vote_threshold;
    params->ransac_max_iter                = def.ransac_max_iter;
    params->ransac_inlier_threshold_arcsec = def.ransac_inlier_threshold_arcsec;
    params->s_min                          = def.s_min;
    params->s_max                          = def.s_max;
    params->img_n_target                   = def.img_n_target;
    params->gaia_density_ratio             = def.gaia_density_ratio;
    params->gaia_query_radius_factor       = def.gaia_query_radius_factor;
    params->m_lim_step                     = def.m_lim_step;
    params->m_lim_max_iter                 = def.m_lim_max_iter;
    params->density_tolerance              = def.density_tolerance;
    // log_dir 保持 '\0' (空字符串 = 不写日志)
}

// ============================================================================
// P11-004 v1.3: 权威 inlier 导出 C API 实现
// ============================================================================

// 获取最后一次成功求解的 inlier 数量
IPV_API int ipv_get_last_inlier_count(void* solver) {
    if (solver == nullptr) return 0;
    try {
        ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
        return s->get_last_inlier_count();
    } catch (...) {
        // 吞掉异常, 防止泄漏到 C 边界
        return 0;
    }
}

// 获取最后一次成功求解的 inlier 详细数据
// out_buffer: 调用方分配, 大小 = max_count * 9 * sizeof(double)
// 返回: >=0 实际写入行数, <0 表示错误
IPV_API int ipv_get_last_inliers(void* solver, double* out_buffer, int max_count) {
    if (solver == nullptr || out_buffer == nullptr || max_count <= 0) {
        return -1;
    }
    try {
        ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
        return s->get_last_inliers(out_buffer, max_count);
    } catch (const std::exception& e) {
        // 异常不应发生, 但保护 C 边界
        (void)e;
        return -1;
    } catch (...) {
        return -1;
    }
}

// 执行求解
IPV_API int ipv_solve(
    void* solver,
    const char* image_path,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IpvParams* params,
    IpvWcsResult* result
) {
    // 修复: 零初始化 result 防止未定义字段值导致 Python 端崩溃
    if (result) {
        std::memset(result, 0, sizeof(IpvWcsResult));
    }

    if (solver == nullptr || image_path == nullptr || result == nullptr) {
        if (result) {
            set_error_msg(result->error_msg, sizeof(result->error_msg),
                          "无效参数: solver/image_path/result 为空");
        }
        return 0;
    }

    // ipv_solve 本身无 try/catch, 避免 SEH 记录栈损坏
    // 异常捕获由 do_solve_impl 负责
    ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
    ipv::IPVSolverParams sp = to_solver_params(params);
    return do_solve_impl(s, image_path, ra0, dec0,
                         focal_length_mm, pixel_size_um, sp, result);
}

// 从内存数据执行求解 (不读文件, 直接接受 float* 像素数据)
IPV_API int ipv_solve_from_memory(
    void* solver,
    const float* pixels,
    int width,
    int height,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IpvParams* params,
    IpvWcsResult* result
) {
    // 零初始化 result
    if (result) {
        std::memset(result, 0, sizeof(IpvWcsResult));
    }

    if (solver == nullptr || pixels == nullptr || result == nullptr) {
        if (result) {
            set_error_msg(result->error_msg, sizeof(result->error_msg),
                          "无效参数: solver/pixels/result 为空");
        }
        return 0;
    }

    if (width <= 0 || height <= 0) {
        set_error_msg(result->error_msg, sizeof(result->error_msg),
                      "无效参数: width/height 必须为正数");
        return 0;
    }

    // ipv_solve_from_memory 本身无 try/catch, 避免 SEH 记录栈损坏
    // 异常捕获由 do_solve_from_memory_impl 负责
    ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
    ipv::IPVSolverParams sp = to_solver_params(params);
    return do_solve_from_memory_impl(s, pixels, width, height, ra0, dec0,
                                     focal_length_mm, pixel_size_um, sp, result);
}

// ============================================================================
// P09-002 INTERNAL_DETECTION_SHARED_EXPORT: 路径 A / 路径 B C API 实现
// 路径 A (ipv_solve_from_detections_v1): 外部 detections 求解, 跳过 sdet_detect_ex
// 路径 B (ipv_solve_from_memory_with_callback, 正式命名 INTERNAL_DETECTION_SHARED_EXPORT):
// 内部单次检测 + callback 同步导出, 由 PSF 通过 star_det 块复用
//
// 与 ipv_solve_from_memory 一致的异常隔离策略:
// - 入口函数本身无 try/catch (避免 SEH 记录栈损坏)
// - 异常捕获由 do_solve_*_impl 负责
// ============================================================================

namespace {

// 路径 A 实现: 从外部 detections 求解 (跳过 sdet_detect_ex)
int do_solve_from_detections_v1_impl(
    ipv::IPVSolver* s,
    const double* detections,
    int n_detections,
    int image_width, int image_height,
    double ra0, double dec0,
    double focal_length_mm, double pixel_size_um,
    const ipv::IPVSolverParams& sp,
    IpvWcsResult* result
) {
    try {
        auto wcs_ptr = std::make_unique<ipv::WcsFitResult>();
        ipv::WcsFitResult& wcs = *wcs_ptr;
        s->solve_from_detections_v1(
            detections, n_detections,
            image_width, image_height,
            ra0, dec0,
            focal_length_mm, pixel_size_um,
            sp,
            &wcs
        );
        to_c_result(wcs, result);
    } catch (const std::bad_alloc& e) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), e.what());
        return 0;
    } catch (const std::exception& e) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), e.what());
        return 0;
    } catch (...) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), "未知 C++ 异常");
        return 0;
    }
    return result->success;
}

// 路径 B 实现 (INTERNAL_DETECTION_SHARED_EXPORT): 带 callback 的内存求解 (保持原有检测 + 导出检测结果)
template <typename T>
static int do_solve_from_memory_with_callback_impl(
    ipv::IPVSolver* s,
    const T* pixels,
    int width, int height,
    double ra0, double dec0,
    double focal_length_mm, double pixel_size_um,
    const ipv::IPVSolverParams& sp,
    IpvDetectionCallback callback,
    void* user_data,
    IpvWcsResult* result
) {
    try {
        auto wcs_ptr = std::make_unique<ipv::WcsFitResult>();
        ipv::WcsFitResult& wcs = *wcs_ptr;
        // IpvDetectionCallback (C ABI) → ipv::DetectionSinkFn (C++ 内部)
        // 两者签名兼容 (都是 void(const double*, int, void*)), 可直接 reinterpret_cast
        ipv::DetectionSinkFn sink_fn = nullptr;
        if (callback != nullptr) {
            sink_fn = reinterpret_cast<ipv::DetectionSinkFn>(callback);
        }
        if constexpr (std::is_same_v<T, float>) {
            s->solve_from_memory_with_callback(
                pixels, width, height,
                ra0, dec0,
                focal_length_mm, pixel_size_um,
                sp,
                sink_fn, user_data,
                &wcs
            );
        } else {
            s->solve_from_memory_with_callback_f64(
                pixels, width, height,
                ra0, dec0,
                focal_length_mm, pixel_size_um,
                sp,
                sink_fn, user_data,
                &wcs
            );
        }
        to_c_result(wcs, result);
    } catch (const std::bad_alloc& e) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), e.what());
        return 0;
    } catch (const std::exception& e) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), e.what());
        return 0;
    } catch (...) {
        set_error_msg(result->error_msg, sizeof(result->error_msg), "未知 C++ 异常");
        return 0;
    }
    return result->success;
}

} // namespace

// 路径 A: 从外部 detections 求解 (跳过 sdet_detect_ex)
// 注: 生产路径为路径 B (INTERNAL_DETECTION_SHARED_EXPORT), 路径 A 仅用于 A/B 对比测试
IPV_API int ipv_solve_from_detections_v1(
    void* solver,
    const double* detections,
    int n_detections,
    int image_width,
    int image_height,
    double ra0,
    double dec0,
    double focal_length_mm,
    double pixel_size_um,
    const IpvParams* params,
    IpvWcsResult* result
) {
    if (result) {
        std::memset(result, 0, sizeof(IpvWcsResult));
    }

    if (solver == nullptr || detections == nullptr || result == nullptr) {
        if (result) {
            set_error_msg(result->error_msg, sizeof(result->error_msg),
                          "无效参数: solver/detections/result 为空");
        }
        return 0;
    }

    if (n_detections <= 0 || image_width <= 0 || image_height <= 0) {
        set_error_msg(result->error_msg, sizeof(result->error_msg),
                      "无效参数: n_detections/image_width/image_height 必须为正数");
        return 0;
    }

    ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
    ipv::IPVSolverParams sp = to_solver_params(params);
    return do_solve_from_detections_v1_impl(s, detections, n_detections,
                                            image_width, image_height,
                                            ra0, dec0,
                                            focal_length_mm, pixel_size_um,
                                            sp, result);
}

// 路径 B (INTERNAL_DETECTION_SHARED_EXPORT): 带 callback 的内存求解
// 生产路径: 每帧 sdet_detect_ex 仅调用 1 次, callback 同步导出检测结果给 PSF
IPV_API int ipv_solve_from_memory_with_callback(
    void* solver,
    const float* pixels,
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
) {
    if (result) {
        std::memset(result, 0, sizeof(IpvWcsResult));
    }

    if (solver == nullptr || pixels == nullptr || result == nullptr) {
        if (result) {
            set_error_msg(result->error_msg, sizeof(result->error_msg),
                          "无效参数: solver/pixels/result 为空");
        }
        return 0;
    }

    if (width <= 0 || height <= 0) {
        set_error_msg(result->error_msg, sizeof(result->error_msg),
                      "无效参数: width/height 必须为正数");
        return 0;
    }

    ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
    ipv::IPVSolverParams sp = to_solver_params(params);
    return do_solve_from_memory_with_callback_impl(s, pixels, width, height,
                                                   ra0, dec0,
                                                   focal_length_mm, pixel_size_um,
                                                   sp, callback, user_data, result);
}

// ============================================================================
// ipv_solve_from_memory_with_callback_d - FP64 内存求解 (double 图像)
// double 图像直接检测 (sdet_detect_ex_f64), 不降级 float/uint16
// ============================================================================
IPV_API int ipv_solve_from_memory_with_callback_d(
    void* solver,
    const double* pixels,
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
) {
    if (result) {
        std::memset(result, 0, sizeof(IpvWcsResult));
    }

    if (solver == nullptr || pixels == nullptr || result == nullptr) {
        if (result) {
            set_error_msg(result->error_msg, sizeof(result->error_msg),
                          "无效参数: solver/pixels/result 为空");
        }
        return 0;
    }

    if (width <= 0 || height <= 0) {
        set_error_msg(result->error_msg, sizeof(result->error_msg),
                      "无效参数: width/height 必须为正数");
        return 0;
    }

    ipv::IPVSolver* s = static_cast<ipv::IPVSolver*>(solver);
    ipv::IPVSolverParams sp = to_solver_params(params);
    return do_solve_from_memory_with_callback_impl<double>(
        s, pixels, width, height,
        ra0, dec0,
        focal_length_mm, pixel_size_um,
        sp, callback, user_data, result);
}
