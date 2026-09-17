// ============================================================================
// astro_sphere_sink.cpp - Drizzle TileAccumulator -> AIO HiPS 直写 Sink 实现
// ============================================================================

#include "astro_sphere_sink.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace drizzle {

namespace {

uint32_t ilog2_u64(uint64_t v) {
    uint32_t l = 0;
    while (v > 1) { v >>= 1; ++l; }
    return l;
}

// 有效 tile 深度 (原 legacy 容器 auto 口径): d = min(9, log2(nside)-4)。
uint32_t phase1_tile_depth(uint32_t nside) {
    if (nside < 16) return 0;
    uint32_t l = 0, v = nside;
    while (v > 1) { v >>= 1; ++l; }
    int d = (int)l - 4;
    if (d < 0) d = 0;
    if (d > 9) d = 9;
    return (uint32_t)d;
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
                                             : phase1_tile_depth(nside);
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
    // M9-F-3: provenance 通道为全或无 —— 帧像素角尺度未知/非正 (header 缺
    // src_pixel_scale_arcsec, 或 WCS 退化致 cd[0]=0) 时**不得**传 0 让 writer
    // 猜; 此时整体不写这两键 (legacy 产品面), 由 Phase2 sampler 走"provenance
    // 缺失"回退路径。帧尺度非有限/超物理域由 setter fail-closed。
    double scale_arcsec = 0.0;
    const auto sit = meta.fits_meta.find("src_pixel_scale_arcsec");
    if (sit != meta.fits_meta.end())
        scale_arcsec = std::atof(sit->second.c_str());
    if (std::isfinite(scale_arcsec) && scale_arcsec > 0.0) {
        if (aio_hips_set_drizzle_provenance(ps, config.pixfrac,
                                            scale_arcsec) != 0) {
            err = "aio_hips_set_drizzle_provenance 失败: " +
                  std::string(aio_hips_last_error() ? aio_hips_last_error() : "?");
            aio_hips_abort(ps);
            return false;
        }
    } else {
        std::fprintf(stderr, "[sink] 源帧像素角尺度未知/非正 (src_pixel_scale_arcsec),"
                             " 跳过 drizzle provenance (全或无通道)\n");
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
        aio_hips_tile_view_abi_init(&view);
        int rc = aio_hips_write_signal_support_tile(ps, &view);
        if (rc != 0) {
            err = "aio_hips_write_signal_support_tile rc=" + std::to_string(rc) +
                  ": " + (aio_hips_last_error() ? aio_hips_last_error() : "?");
            aio_hips_abort(ps);
            std::fprintf(stderr, "[sink] %s\n", err.c_str());
            return false;
        }
        if (has_variance) {
            aio_hips_tile_view_abi_init(&view);
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

// ============================================================================
// write_hips_phase1 - Phase1 生产末端: TileAccumulator -> 标准 HiPS 直写
//
// 与旧 writer 节点 (lib/infrastructure/scheduler/src/module_adapters.cpp p1_op_writer 读取中间容器
// 后写 HiPS) 的产物逐字节等价。等价性论证见 astro_sphere_sink.h 与 P23 交付
// 报告的 REPORT.md。
// ============================================================================
template <typename Scalar>
bool write_hips_phase1(const std::vector<TileAccumulatorT<Scalar>>& tiles,
                       const DrizzleConfig& config,
                       const std::string& hips_dir,
                       const std::string& filter_passband,
                       std::string& err) {
    const uint32_t nside = (uint32_t)config.nside;
    const uint32_t depth = config.tile_depth ? config.tile_depth
                                             : phase1_tile_depth(nside);
    if (depth != 9) {
        err = "Phase1 HiPS 直写要求 tile_depth=9 (nside>=512), 当前 depth=" +
              std::to_string(depth);
        std::fprintf(stderr, "[sink][phase1] %s\n", err.c_str());
        return false;
    }
    if (nside < 512) {
        err = "Phase1 HiPS 直写要求 nside>=512";
        std::fprintf(stderr, "[sink][phase1] %s\n", err.c_str());
        return false;
    }
    const size_t n_leaf = 512u * 512u;  // 4^9
    // A_cell = 4π/(12·nside²) sr —— 与旧 writer 的 a_cell 同一字面量常数。
    const double a_cell = 4.0 * 3.14159265358979323846 /
                          (12.0 * (double)nside * (double)nside);
    const uint32_t leaf_order = ilog2_u64(nside);
    const uint64_t std_parent_count = 12ULL * (1ULL << (2 * (leaf_order - 9)));

    // DATA-P1-HIPS §12.1/§12.2 产品集: variance/ivar Image HiPS 由累加器
    // var_num_sum (= Σ v_j·w_jp², SCI-DRZ-014 §5) 经同一 writer 通道落盘。
    // 方差面**不再硬编码为「不请求」**: 累加器携带有限正方差累加量时请求
    // VARIANCE|IVAR 并把 var_num_sum 交给 writer —— 与 write_hips_direct 的
    // has_variance 分支同一冻结通道, 禁止第二套方差语义。方差面全零/全无效
    // (无方差输入) 时保持既有 signal+support 两产品面逐字节不变。
    bool has_variance = false;
    for (const auto& t : tiles) {
        if (t.touched.empty()) continue;
        for (uint32_t local : t.touched) {
            if (local >= n_leaf || local >= t.pixels.size()) continue;
            const double vn = (double)t.pixels[local].sumVarNum;
            if (std::isfinite(vn) && vn > 0.0) { has_variance = true; break; }
        }
        if (has_variance) break;
    }
    // 故障注入面（ENGINEERING_SPEC §8 可执行负例）: ASTROCS_IVAR_FAULT=
    // no_variance_flags 模拟「累加器已含方差却不请求 variance/ivar 产品位」
    // 缺陷 → IVAR-001 新增门必然判红（见 run/PROJECT-GOVERNANCE-01/IVAR-001）。
    {
        const char* sink_fault = std::getenv("ASTROCS_IVAR_FAULT");
        if (sink_fault && std::string(sink_fault) == "no_variance_flags")
            has_variance = false;
    }
    const int prod_flags =
        has_variance ? (AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT |
                        AIO_HIPS_PRODUCT_VARIANCE | AIO_HIPS_PRODUCT_IVAR)
                     : (AIO_HIPS_PRODUCT_SIGNAL | AIO_HIPS_PRODUCT_SUPPORT);

    // product_begin 参数与旧 writer 逐项一致 (creator_did/obs_title/obs_filter/
    // exposure/obs_date/moc_order), 不写 drizzle provenance, 不写 snr。
    AioHipsProductSet* ps = aio_hips_product_begin(
        hips_dir.c_str(), nside, 512, AIO_HIPS_FLOAT32,
        prod_flags,
        "astrocs/phase1", "AstroCS Phase1 single-frame stack",
        filter_passband.c_str(), 0.0, nullptr, 0);
    if (!ps) {
        err = "aio_hips_product_begin 失败: " +
              std::string(aio_hips_last_error() ? aio_hips_last_error() : "?");
        std::fprintf(stderr, "[sink][phase1] %s\n", err.c_str());
        return false;
    }

    // tile 按 parent_ipix 升序 (旧 writer 的 parent 升序归约顺序, 决定
    // finalize 的 hierarchy/覆盖面积浮点累加次序 ⇒ 逐位可复现)。
    std::vector<const TileAccumulatorT<Scalar>*> ordered;
    ordered.reserve(tiles.size());
    for (const auto& t : tiles)
        if (!t.touched.empty()) ordered.push_back(&t);
    std::stable_sort(ordered.begin(), ordered.end(),
        [](const TileAccumulatorT<Scalar>* a, const TileAccumulatorT<Scalar>* b) {
            return a->parent_ipix < b->parent_ipix;
        });

    const auto t_sink0 = std::chrono::steady_clock::now();
    double prof_transform = 0.0, prof_fits_write = 0.0;
    std::vector<float> flux_buf(n_leaf, 0.0f);
    std::vector<float> area_buf(n_leaf, 0.0f);
    std::vector<uint8_t> valid_buf(n_leaf, 0);
    std::vector<float> var_buf(n_leaf, 0.0f);
    size_t n_written = 0;
    uint64_t n_variance_written = 0, n_variance_skipped = 0;
    for (const auto* tp : ordered) {
        if (tp->parent_ipix >= std_parent_count) continue;  // 越界 parent 不写出
        const auto t_tr0 = std::chrono::steady_clock::now();
        std::fill(flux_buf.begin(), flux_buf.end(), 0.0f);
        std::fill(area_buf.begin(), area_buf.end(), 0.0f);
        std::fill(valid_buf.begin(), valid_buf.end(), 0);
        if (has_variance) std::fill(var_buf.begin(), var_buf.end(), 0.0f);
        for (uint32_t local : tp->touched) {
            if (local >= n_leaf || local >= tp->pixels.size()) continue;
            const auto& acc = tp->pixels[local];
            // support 面积比 uint8 量化 (旧容器 finalize_support 同式):
            //   u8 = lround(255·clamp(sumArea/A_cell, 0, 1))
            double S = (double)acc.sumArea / a_cell;
            if (S < 0.0) S = 0.0;
            if (S > 1.0) S = 1.0;
            long q = std::lround(255.0 * S);
            if (q < 0) q = 0;
            if (q > 255) q = 255;
            // signal = 累计通量, HiPS 产品位深 float32 (旧 writer 的显式窄化)
            flux_buf[local] = (float)((double)acc.sumFlux);
            // covered_area = (u8/255)·A_cell (旧 writer 的面积比连续缩放)
            area_buf[local] = (float)(((double)q / 255.0) * a_cell);
            // variance 分子 Σ v_j·w_jp² (ADU², SCI-DRZ-014 §5); writer 归约
            // variance = var_num_sum/covered_area²、ivar = 1/variance (§12.2/§4a)。
            if (has_variance)
                var_buf[local] = (float)((double)acc.sumVarNum);
            valid_buf[local] = 1;
        }
        prof_transform += std::chrono::duration<double>(
            std::chrono::steady_clock::now() - t_tr0).count();
        const auto t_wr0 = std::chrono::steady_clock::now();
        AstroSphereTileView view;
        std::memset(&view, 0, sizeof(view));
        view.parent_ipix = tp->parent_ipix;
        view.leaf_order = leaf_order;
        view.width = 512;
        view.data_type = AIO_HIPS_FLOAT32;
        view.flux_sum = flux_buf.data();
        view.covered_area = area_buf.data();
        view.valid_mask = valid_buf.data();  // 显式 per-parent 有效掩膜
        view.var_num_sum = has_variance ? (const void*)var_buf.data() : nullptr;
        aio_hips_tile_view_abi_init(&view);
        const int rc = aio_hips_write_signal_support_tile(ps, &view);
        if (rc != 0) {
            err = "aio_hips_write_signal_support_tile rc=" + std::to_string(rc) +
                  ": " + (aio_hips_last_error() ? aio_hips_last_error() : "?");
            aio_hips_abort(ps);
            std::fprintf(stderr, "[sink][phase1] %s\n", err.c_str());
            return false;
        }
        if (has_variance) {
            aio_hips_tile_view_abi_init(&view);
            const int vrc = aio_hips_write_variance_tile(ps, &view);
            if (vrc == -5 || vrc == -2) {
                // 该 tile 全无效方差 → 不落盘 (§12.4 显式失败而非空产品),
                // signal/support 已写, 正常收尾。
                ++n_variance_skipped;
            } else if (vrc != 0) {
                err = "aio_hips_write_variance_tile rc=" + std::to_string(vrc) +
                      ": " + (aio_hips_last_error() ? aio_hips_last_error() : "?");
                aio_hips_abort(ps);
                std::fprintf(stderr, "[sink][phase1] %s\n", err.c_str());
                return false;
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
        std::fprintf(stderr, "[sink][phase1] %s\n", err.c_str());
        return false;
    }
    const auto t_fin0 = std::chrono::steady_clock::now();
    if (aio_hips_finalize(ps) != 0) {
        err = "aio_hips_finalize 失败: " +
              std::string(aio_hips_last_error() ? aio_hips_last_error() : "?");
        aio_hips_abort(ps);
        std::fprintf(stderr, "[sink][phase1] %s\n", err.c_str());
        return false;
    }
    const double prof_finalize = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - t_fin0).count();
    std::fprintf(stderr, "[sink][phase1] HiPS 直写完成: %zu tiles -> %s\n",
                 n_written, hips_dir.c_str());
    std::fprintf(stderr,
                 "[sink][phase1] variance/ivar 子产品: has_variance=%d written=%llu"
                 " skipped(no-data)=%llu (DATA-P1-HIPS §12.2)\n",
                 has_variance ? 1 : 0, (unsigned long long)n_variance_written,
                 (unsigned long long)n_variance_skipped);
    std::fprintf(stderr,
                 "[sink][phase1][profile] transform=%.3fs fits_write=%.3fs "
                 "finalize=%.3fs total=%.3fs\n",
                 prof_transform, prof_fits_write, prof_finalize,
                 std::chrono::duration<double>(
                     std::chrono::steady_clock::now() - t_sink0).count());
    return true;
}

template bool write_hips_phase1<float>(
    const std::vector<TileAccumulatorT<float>>&, const DrizzleConfig&,
    const std::string&, const std::string&, std::string&);
template bool write_hips_phase1<double>(
    const std::vector<TileAccumulatorT<double>>&, const DrizzleConfig&,
    const std::string&, const std::string&, std::string&);

} // namespace drizzle
