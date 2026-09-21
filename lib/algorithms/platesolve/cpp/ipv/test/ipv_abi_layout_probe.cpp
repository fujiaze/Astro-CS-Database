// ============================================================================
// ipv_abi_layout_probe.cpp — IpvParams / IpvWcsResult C ABI 布局探针 (P18 / V2-N-01)
// ----------------------------------------------------------------------------
// 只读打印公共 C 结构体的 sizeof / alignof / 逐字段 offsetof+sizeof 为 JSON,
// 供 ipv_abi_layout_lock.py 与 ctypes 镜像 (lib/algorithms/platesolve/tools/ipv_abi_mirror.py)
// 逐字段机器对账。
//
// 依赖面: 仅 ipv_api.h (无 windows.h / 不链接生产库) => Linux amd64 控制节点
// 可直接编译, 不触发 eng/tests/unit/p1wcs 注记的 ipv_select/ipv_solver/ipv_entry
// 平台依赖问题。
//
// 同时以 static_assert 锁定宪章 §8.6 的 ABI 头部契约:
// struct_size 在偏移 0, abi_version 在偏移 4, 且二者均为 uint32_t。
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "ipv_api.h"

// --- 宪章 §8.6 ABI 头部契约 (编译期锁) ---
static_assert(offsetof(IpvParams, struct_size) == 0,
              "IpvParams.struct_size 必须在偏移 0 (宪章 §8.6)");
static_assert(offsetof(IpvParams, abi_version) == 4,
              "IpvParams.abi_version 必须在偏移 4 (宪章 §8.6)");
static_assert(std::is_same<decltype(IpvParams::struct_size), uint32_t>::value,
              "IpvParams.struct_size 必须为 uint32_t");
static_assert(std::is_same<decltype(IpvParams::abi_version), uint32_t>::value,
              "IpvParams.abi_version 必须为 uint32_t");

namespace {

template <typename T>
void emit_fields(const char* const* names, const std::size_t* offsets,
                 const std::size_t* sizes, int n) {
    for (int i = 0; i < n; ++i) {
        std::printf("%s\n      {\"name\": \"%s\", \"offset\": %zu, \"size\": %zu}",
                    i == 0 ? "" : ",", names[i], offsets[i], sizes[i]);
    }
    std::printf("\n");
}

}  // namespace

#define IPV_OFF(T, f) offsetof(T, f)
#define IPV_SZ(T, f) sizeof(((T*)0)->f)

int main() {
    std::printf("{\n");

    // ---------------- IpvParams ----------------
    {
        const char* names[] = {
            "struct_size", "abi_version",
            "polygon_sides", "n_pivot", "sigma_d_arcsec", "vote_threshold",
            "ransac_max_iter", "ransac_inlier_threshold_arcsec", "s_min", "s_max",
            "img_n_target", "gaia_density_ratio", "gaia_query_radius_factor",
            "m_lim_alpha_prior", "m_lim_alpha_min", "m_lim_alpha_max",
            "m_lim_safety", "m_lim_m0_exposure_s", "m_lim_m0_offset",
            "m_lim_clamp_lo", "m_lim_clamp_hi", "m_lim_zero_step",
            "m_lim_gaia_cap_per_file", "m_lim_max_iter", "density_tolerance",
            "log_dir"
        };
        const std::size_t offsets[] = {
            IPV_OFF(IpvParams, struct_size), IPV_OFF(IpvParams, abi_version),
            IPV_OFF(IpvParams, polygon_sides), IPV_OFF(IpvParams, n_pivot),
            IPV_OFF(IpvParams, sigma_d_arcsec), IPV_OFF(IpvParams, vote_threshold),
            IPV_OFF(IpvParams, ransac_max_iter), IPV_OFF(IpvParams, ransac_inlier_threshold_arcsec),
            IPV_OFF(IpvParams, s_min), IPV_OFF(IpvParams, s_max),
            IPV_OFF(IpvParams, img_n_target), IPV_OFF(IpvParams, gaia_density_ratio),
            IPV_OFF(IpvParams, gaia_query_radius_factor),
            IPV_OFF(IpvParams, m_lim_alpha_prior), IPV_OFF(IpvParams, m_lim_alpha_min),
            IPV_OFF(IpvParams, m_lim_alpha_max), IPV_OFF(IpvParams, m_lim_safety),
            IPV_OFF(IpvParams, m_lim_m0_exposure_s), IPV_OFF(IpvParams, m_lim_m0_offset),
            IPV_OFF(IpvParams, m_lim_clamp_lo), IPV_OFF(IpvParams, m_lim_clamp_hi),
            IPV_OFF(IpvParams, m_lim_zero_step), IPV_OFF(IpvParams, m_lim_gaia_cap_per_file),
            IPV_OFF(IpvParams, m_lim_max_iter), IPV_OFF(IpvParams, density_tolerance),
            IPV_OFF(IpvParams, log_dir)
        };
        const std::size_t sizes[] = {
            IPV_SZ(IpvParams, struct_size), IPV_SZ(IpvParams, abi_version),
            IPV_SZ(IpvParams, polygon_sides), IPV_SZ(IpvParams, n_pivot),
            IPV_SZ(IpvParams, sigma_d_arcsec), IPV_SZ(IpvParams, vote_threshold),
            IPV_SZ(IpvParams, ransac_max_iter), IPV_SZ(IpvParams, ransac_inlier_threshold_arcsec),
            IPV_SZ(IpvParams, s_min), IPV_SZ(IpvParams, s_max),
            IPV_SZ(IpvParams, img_n_target), IPV_SZ(IpvParams, gaia_density_ratio),
            IPV_SZ(IpvParams, gaia_query_radius_factor),
            IPV_SZ(IpvParams, m_lim_alpha_prior), IPV_SZ(IpvParams, m_lim_alpha_min),
            IPV_SZ(IpvParams, m_lim_alpha_max), IPV_SZ(IpvParams, m_lim_safety),
            IPV_SZ(IpvParams, m_lim_m0_exposure_s), IPV_SZ(IpvParams, m_lim_m0_offset),
            IPV_SZ(IpvParams, m_lim_clamp_lo), IPV_SZ(IpvParams, m_lim_clamp_hi),
            IPV_SZ(IpvParams, m_lim_zero_step), IPV_SZ(IpvParams, m_lim_gaia_cap_per_file),
            IPV_SZ(IpvParams, m_lim_max_iter), IPV_SZ(IpvParams, density_tolerance),
            IPV_SZ(IpvParams, log_dir)
        };
        const int n = static_cast<int>(sizeof(names) / sizeof(names[0]));
        std::printf("  \"IpvParams\": {\n");
        std::printf("    \"sizeof\": %zu,\n", sizeof(IpvParams));
        std::printf("    \"alignof\": %zu,\n", alignof(IpvParams));
        std::printf("    \"abi_version_value\": %u,\n",
                    static_cast<unsigned>(IPV_PARAMS_ABI_VERSION));
        std::printf("    \"fields\": [");
        emit_fields<IpvParams>(names, offsets, sizes, n);
        std::printf("    ]\n  },\n");
    }

    // ---------------- IpvWcsResult ----------------
    {
        const char* names[] = {
            "cd", "crval", "crpix", "sip_order", "sip_a", "sip_b", "sip_ap_order",
            "sip_ap", "sip_bp", "rms_px", "rms_arcsec", "n_pairs", "success",
            "n_detected", "n_catalog", "trans_order", "best_inliers",
            "ctype1", "ctype2", "error_msg"
        };
        const std::size_t offsets[] = {
            IPV_OFF(IpvWcsResult, cd), IPV_OFF(IpvWcsResult, crval), IPV_OFF(IpvWcsResult, crpix),
            IPV_OFF(IpvWcsResult, sip_order), IPV_OFF(IpvWcsResult, sip_a),
            IPV_OFF(IpvWcsResult, sip_b), IPV_OFF(IpvWcsResult, sip_ap_order),
            IPV_OFF(IpvWcsResult, sip_ap), IPV_OFF(IpvWcsResult, sip_bp),
            IPV_OFF(IpvWcsResult, rms_px), IPV_OFF(IpvWcsResult, rms_arcsec),
            IPV_OFF(IpvWcsResult, n_pairs), IPV_OFF(IpvWcsResult, success),
            IPV_OFF(IpvWcsResult, n_detected), IPV_OFF(IpvWcsResult, n_catalog),
            IPV_OFF(IpvWcsResult, trans_order), IPV_OFF(IpvWcsResult, best_inliers),
            IPV_OFF(IpvWcsResult, ctype1), IPV_OFF(IpvWcsResult, ctype2),
            IPV_OFF(IpvWcsResult, error_msg)
        };
        const std::size_t sizes[] = {
            IPV_SZ(IpvWcsResult, cd), IPV_SZ(IpvWcsResult, crval), IPV_SZ(IpvWcsResult, crpix),
            IPV_SZ(IpvWcsResult, sip_order), IPV_SZ(IpvWcsResult, sip_a),
            IPV_SZ(IpvWcsResult, sip_b), IPV_SZ(IpvWcsResult, sip_ap_order),
            IPV_SZ(IpvWcsResult, sip_ap), IPV_SZ(IpvWcsResult, sip_bp),
            IPV_SZ(IpvWcsResult, rms_px), IPV_SZ(IpvWcsResult, rms_arcsec),
            IPV_SZ(IpvWcsResult, n_pairs), IPV_SZ(IpvWcsResult, success),
            IPV_SZ(IpvWcsResult, n_detected), IPV_SZ(IpvWcsResult, n_catalog),
            IPV_SZ(IpvWcsResult, trans_order), IPV_SZ(IpvWcsResult, best_inliers),
            IPV_SZ(IpvWcsResult, ctype1), IPV_SZ(IpvWcsResult, ctype2),
            IPV_SZ(IpvWcsResult, error_msg)
        };
        const int n = static_cast<int>(sizeof(names) / sizeof(names[0]));
        std::printf("  \"IpvWcsResult\": {\n");
        std::printf("    \"sizeof\": %zu,\n", sizeof(IpvWcsResult));
        std::printf("    \"alignof\": %zu,\n", alignof(IpvWcsResult));
        std::printf("    \"fields\": [");
        emit_fields<IpvWcsResult>(names, offsets, sizes, n);
        std::printf("    ]\n  }\n");
    }

    std::printf("}\n");
    return 0;
}
