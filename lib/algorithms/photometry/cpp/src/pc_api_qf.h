#ifndef ASTROCS_PHOTOMETRY_PC_API_QF_H
#define ASTROCS_PHOTOMETRY_PC_API_QF_H
// ============================================================================
// pc_api_qf.h - 质量位感知的测光定标入口（**非 C ABI 导出**）
//
// 背景（RELEASE-02 FIX-P1 / P1-2）:
//   冻结的 6 个 PC_API 导出（photometric_calib.h）不携带逐星质量位，故
//   pc_api.cpp 内部一律以 quality_flags=nullptr 调 cleanAndScale ⇒
//   star_matcher.cpp:428-438 的 SCI-PHOT-001 §4/§10 饱和/质量位有效域过滤
//   （PC_QF_SATURATED|PC_QF_HAS_SATURATED）在**生产路径上不生效**。
//   依据 docs/contracts/PUBLIC_API.md §「C ABI（extern "C"…）」+ ASTROCS_DESIGN.md
//   §8.5「模块与 ABI」（版本化头以 C ABI 标记声明的函数面）: 不得新增/修改任何
//   **C 导出 ABI**（orchestrator 用 raw 函数指针按位置调用既有导出）。
//
// 本头声明一个 **C++ 链接期** 入口（非 extern "C"、无 PC_API 宏），
// 参数与 pc_calibrate_simple_with_gaia_f64_v2 完全一致，仅追加尾部
// const uint32_t* quality_flags（长度 = n_psf，按 PSF 行对齐）。既有 6 个
// 导出签名/行为逐行不变；生产 Phase1 节点（module_adapters）经本入口把
// p1_sources[].quality 的饱和位送达 cleanAndScale。
//
// quality_flags 语义: 位定义同 star_matcher.h PC_QF_*（与 photometric_calib.h
// 的 PC_QF_*/SNR_QF_* 同值）；含 PC_QF_SATURATED_MASK 的星不进入零点拟合。
// ============================================================================

#include "../include/photometric_calib.h"

#include <cstdint>

int pc_calibrate_simple_with_gaia_f64_v2_qf(
    void* gaia_client_handle,
    double ra_center, double dec_center, double radius_deg,
    double mag_min, double mag_max,
    const double* filter_wl, const double* filter_trans, int filter_count,
    const double* qe_wl, const double* qe_trans, int qe_count,
    const double* spectrum_wl, int spectrum_count,
    const double* pixels, int width, int height,
    const double* psf_cx, const double* psf_cy,
    const double* psf_flux, const int* psf_status, int n_psf,
    const int64_t* psf_star_ids, PcMatchRecord* out_records,
    double crval1, double crval2, double crpix1, double crpix2,
    double cd11, double cd12, double cd21, double cd22,
    int sip_order,
    const double* sip_a, const double* sip_b,
    const double* sip_ap, const double* sip_bp,
    double* out_pixels, int* out_n_matched, double* out_scale_factor,
    double* out_sigma_residual,
    PhotometricDiag* out_diag,
    const uint32_t* quality_flags);

#endif  // ASTROCS_PHOTOMETRY_PC_API_QF_H
