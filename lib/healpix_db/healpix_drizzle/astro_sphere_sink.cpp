// ============================================================================
// astro_sphere_sink.cpp - Drizzle TileAccumulator -> AIO HiPS 直写 Sink 实现
// ============================================================================

#include "astro_sphere_sink.h"
#include "../../astro_image_io/include/hiss_format.h"  // hiss::compute_tile_depth

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace drizzle {

namespace {

uint32_t ilog2_u64(uint64_t v) {
    uint32_t l = 0;
    while (v > 1) { v >>= 1; ++l; }
    return l;
}

} // namespace

template <typename Scalar>
bool write_hips_direct(const std::vector<TileAccumulatorT<Scalar>>& tiles,
                       const DrizzleConfig& config,
                       const DrizzleMeta& meta,
                       const std::string& hips_dir,
                       const std::vector<AioHipsSnrPoint>& snr_pts,
                       int has_variance,
                       std::string& err) {
    const uint32_t nside = (uint32_t)config.nside;
    const uint32_t depth = config.tile_depth ? config.tile_depth
                                             : hiss::compute_tile_depth(nside);
    if (depth != 9) {
        err = "HiPS 直写要求 tile_depth=9 (nside>=512), 当前 depth=" + std::to_string(depth);
        std::fprintf(stderr, "[sink] %s\n", err.c_str());
        return false;
    }
    if (nside < 512) {
        err = "HiPS 直写要求 nside>=512";
        return false;
    }
    const size_t n_leaf = 512 * 512;  // 4^9
    const int dtype = (sizeof(Scalar) == sizeof(float)) ? AIO_HIPS_FLOAT32
                                                        : AIO_HIPS_FLOAT64;
    const std::string title = meta.filter.empty() ? "AstroCS Phase1" : ("AstroCS " + meta.filter);

    const int prod_flags = has_variance ? AIO_HIPS_PRODUCT_ALL_V19
                                        : AIO_HIPS_PRODUCT_ALL;
    AioHipsProductSet* ps = aio_hips_product_begin(
        hips_dir.c_str(), nside, 512, dtype, prod_flags,
        "ivo://astrocs/phase1", title.c_str(),
        meta.filter.empty() ? nullptr : meta.filter.c_str(),
        meta.exposure_s,
        meta.obs_time.empty() ? nullptr : meta.obs_time.c_str(),
        0);
    if (!ps) {
        err = "aio_hips_product_begin 失败: " +
              std::string(aio_hips_last_error() ? aio_hips_last_error() : "?");
        std::fprintf(stderr, "[sink] %s\n", err.c_str());
        return false;
    }
    // （K_CORR_DOMAIN 选项 B）：Drizzle provenance 写入 properties，
    // Phase2 sampler 按帧选择 control-ivar 的 k_corr 标定值。
    double scale_arcsec = 0.0;
    const auto sit = meta.fits_meta.find("src_pixel_scale_arcsec");
    if (sit != meta.fits_meta.end())
        scale_arcsec = std::atof(sit->second.c_str());
    if (aio_hips_set_drizzle_provenance(ps, config.pixfrac,
                                        scale_arcsec) != 0) {
        err = "aio_hips_set_drizzle_provenance 失败";
        aio_hips_abort(ps);
        return false;
    }

    const uint32_t leaf_order = ilog2_u64(nside);
    // HiPS 直写分段计时（每段一次 clock，低开销）
    const auto t_sink0 = std::chrono::steady_clock::now();
    double prof_transform = 0.0, prof_fits_write = 0.0;
    std::vector<Scalar> dense_flux(n_leaf, Scalar(0));
    std::vector<Scalar> dense_area(n_leaf, Scalar(0));
    std::vector<Scalar> dense_var(n_leaf, Scalar(0));
    size_t n_written = 0;
    std::uint64_t n_variance_skipped = 0;
    std::uint64_t n_variance_written = 0;
    for (const auto& tile : tiles) {
        if (tile.touched.empty()) continue;
        const auto t_tr0 = std::chrono::steady_clock::now();
        std::fill(dense_flux.begin(), dense_flux.end(), Scalar(0));
        std::fill(dense_area.begin(), dense_area.end(), Scalar(0));
        if (has_variance)
            std::fill(dense_var.begin(), dense_var.end(), Scalar(0));
        for (uint32_t local : tile.touched) {
            if (local >= n_leaf || local >= tile.pixels.size()) continue;
            const auto& acc = tile.pixels[local];
            dense_flux[local] = acc.sumFlux;
            dense_area[local] = acc.sumArea;
            if (has_variance) dense_var[local] = acc.sumVarNum;
        }
        prof_transform += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_tr0).count();
        const auto t_wr0 = std::chrono::steady_clock::now();
        AstroSphereTileView view;
        std::memset(&view, 0, sizeof(view));
        view.parent_ipix = tile.parent_ipix;
        view.leaf_order = leaf_order;
        view.width = 512;
        view.data_type = dtype;
        view.flux_sum = dense_flux.data();
        view.covered_area = dense_area.data();
        view.valid_mask = nullptr;
        view.var_num_sum = has_variance ? (const void*)dense_var.data() : nullptr;
        int rc = aio_hips_write_signal_support_tile(ps, &view);
        if (rc != 0) {
            err = "aio_hips_write_signal_support_tile rc=" + std::to_string(rc) +
                  ": " + (aio_hips_last_error() ? aio_hips_last_error() : "?");
            aio_hips_abort(ps);
            std::fprintf(stderr, "[sink] %s\n", err.c_str());
            return false;
        }
        if (has_variance) {
            rc = aio_hips_write_variance_tile(ps, &view);
            if (rc != 0) {
                // 该 tile 全零/无有效方差 → 跳过本 tile variance (不中止)
                if (rc == -5 || rc == -2) {
                    std::fprintf(stderr,
                                 "[sink] variance tile %llu 无有效数据 (rc=%d), "
                                 "跳过该 tile (signal 不受影响)\n",
                                 (unsigned long long)view.parent_ipix, rc);
                    ++n_variance_skipped;
                    // 仅跳过 variance 计数, signal/support 已写, 正常收尾
                } else {
                    err = "aio_hips_write_variance_tile rc=" +
                          std::to_string(rc) + ": " +
                          (aio_hips_last_error() ? aio_hips_last_error() : "?");
                    aio_hips_abort(ps);
                    std::fprintf(stderr, "[sink] %s\n", err.c_str());
                    return false;
                }
            } else {
                ++n_variance_written;
            }
        }
        prof_fits_write += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_wr0).count();
        ++n_written;
    }
    if (n_written == 0) {
        err = "无有效 tile 可写入 HiPS";
        aio_hips_abort(ps);
        return false;
    }
    if (!snr_pts.empty()) {
        const auto t_snr0 = std::chrono::steady_clock::now();
        if (aio_hips_write_snr_points(ps, snr_pts.data(), (int)snr_pts.size()) != 0) {
            err = "aio_hips_write_snr_points 失败";
            aio_hips_abort(ps);
            return false;
        }
        const double s = std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_snr0).count();
        std::fprintf(stderr, "[sink][profile] snr_points=%.3fs\n", s);
    }
    const auto t_fin0 = std::chrono::steady_clock::now();
    int rc = aio_hips_finalize(ps);
    if (rc != 0) {
        err = "aio_hips_finalize rc=" + std::to_string(rc) + ": " +
              (aio_hips_last_error() ? aio_hips_last_error() : "?");
        std::fprintf(stderr, "[sink] %s\n", err.c_str());
        return false;
    }
    const double prof_finalize = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_fin0).count();
    std::fprintf(stderr, "[sink] HiPS 直写完成: %zu tiles -> %s\n",
                 n_written, hips_dir.c_str());
    std::fprintf(stderr,
                 "[sink] variance tiles written=%llu skipped(no-data)=%llu\n",
                 (unsigned long long)n_variance_written,
                 (unsigned long long)n_variance_skipped);
    std::fprintf(stderr,
                 "[sink][profile] transform=%.3fs fits_write=%.3fs "
                 "finalize=%.3fs total=%.3fs\n",
                 prof_transform, prof_fits_write, prof_finalize,
                 std::chrono::duration<double>(
                     std::chrono::steady_clock::now() - t_sink0).count());
    return true;
}

template bool write_hips_direct<float>(
    const std::vector<TileAccumulatorT<float>>&, const DrizzleConfig&, const DrizzleMeta&,
    const std::string&, const std::vector<AioHipsSnrPoint>&, int, std::string&);
template bool write_hips_direct<double>(
    const std::vector<TileAccumulatorT<double>>&, const DrizzleConfig&, const DrizzleMeta&,
    const std::string&, const std::vector<AioHipsSnrPoint>&, int, std::string&);

} // namespace drizzle
