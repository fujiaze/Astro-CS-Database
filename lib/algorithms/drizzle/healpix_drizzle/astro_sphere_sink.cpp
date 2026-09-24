// ============================================================================
// astro_sphere_sink.cpp - Drizzle TileAccumulator -> AIO HiPS 直写 Sink 实现
// ============================================================================

#include "astro_sphere_sink.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// ASTROCS_DESIGN §9「aio 是文件级唯一 I/O 边界」+ §9.73 裁决 U5:
// 整文件读取机制经 aio 唯一实现 (aio_file::read_all), 本 TU 不自持 ifstream 通道。
#include "aio_file_io.h"

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

// RELEASE-02 SD-15: 由 light 基名派生 frame_key（与 module_adapters
// p1_frame_key 同口径: 去扩展名 + 字符白名单 alnum/_/-/. → '_'; 空/./.. → "frame"）。
// 输入为 p1_snr.json 的 file 字段（cleaned_<base>/calibrated_<base>/<base>）。
std::string phase1_stem_key(const std::string& base_in) {
    const size_t slash = base_in.find_last_of("/\\");
    const std::string base =
        (slash == std::string::npos) ? base_in : base_in.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    const std::string stem =
        (dot == std::string::npos || dot == 0) ? base : base.substr(0, dot);
    std::string out;
    out.reserve(stem.size());
    for (unsigned char c : stem) {
        if (std::isalnum(c) || c == '_' || c == '-' || c == '.')
            out.push_back(static_cast<char>(c));
        else
            out.push_back('_');
    }
    if (out.empty() || out == "." || out == "..") out = "frame";
    return out;
}

// p1_snr.json 的 file 字段是否对应当前帧（frame_key = hips_dir 基名）。
// 同时试「原样」与「去 cleaned_/calibrated_ 前缀」两种 stem，避免前缀歧义。
bool phase1_frame_matches(const std::string& file, const std::string& frame_key) {
    const size_t slash = file.find_last_of("/\\");
    const std::string base =
        (slash == std::string::npos) ? file : file.substr(slash + 1);
    if (phase1_stem_key(base) == frame_key) return true;
    if (base.rfind("cleaned_", 0) == 0 &&
        phase1_stem_key(base.substr(8)) == frame_key)
        return true;
    if (base.rfind("calibrated_", 0) == 0 &&
        phase1_stem_key(base.substr(11)) == frame_key)
        return true;
    return false;
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
    const std::string title = meta.filter.empty() ? "ACSD Phase1" : ("ACSD " + meta.filter);

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
                       int has_variance,
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
    // **产品集判据 = 本帧是否携带方差输入 (has_variance 形参)**，与
    // write_hips_direct 的同一形参同口径（禁止第二套方差语义）：
    //   1 ⇒ 请求 VARIANCE|IVAR 并把 var_num_sum 交给 writer；逐像素三态由
    //       writer 判定（有覆盖∧方差可用 → vnum/area²；有覆盖∧方差不可用 →
    //       0/0 显式不可用，§4a:49；无覆盖 → NaN）。
    //       **「整帧方差不可用」也必须产出 variance/ivar 子产品**（全 0/0）：
    //       不产会让阶段二 ivar_product_missing>0 ⇒ rc=7（A44 已删除 legacy
    //       fallback），把「像素级不可用」升级为「产品级拒绝」。
    //   0 ⇒ 无方差输入，只写 signal+support（与旧 writer 逐字节等价）。
    // 故障注入面（ENGINEERING_SPEC §8 可执行负例）: ASTROCS_IVAR_FAULT=
    // no_variance_flags 模拟「帧已带方差却不请求 variance/ivar 产品位」
    // 缺陷 → IVAR-001 门必然判红（见 run/PROJECT-GOVERNANCE-01/IVAR-001）。
    {
        const char* sink_fault = std::getenv("ASTROCS_IVAR_FAULT");
        if (sink_fault && std::string(sink_fault) == "no_variance_flags")
            has_variance = 0;
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
        "astrocs/phase1", "ACSD Phase1 single-frame stack",
        filter_passband.c_str(), 0.0, nullptr, 0);
    if (!ps) {
        err = "aio_hips_product_begin 失败: " +
              std::string(aio_hips_last_error() ? aio_hips_last_error() : "?");
        std::fprintf(stderr, "[sink][phase1] %s\n", err.c_str());
        return false;
    }

    // ── RELEASE-02 SD-15: 帧级未加权通量型 SNR → HiPS properties ──────────
    // 值来源 = 上游 snr 节点产物 <output_dir>/p1_snr.json 的该帧
    // snr_reference.{snr_f,flux_adu}（F_ref/σ_F; ASTROCS_DESIGN §3.4 /
    // 07_noise_snr.md §4.1）。本帧产品目录 = hips_dir（= <output_dir>/<frame_key>）
    // ⇒ sidecar 在父目录，按 frame_key 匹配本帧。
    // 缺失/不匹配/非有限 → **不写键**（Phase2 权重链据此 fail-closed; 禁伪造,
    // 禁用受天光影响的普通 SNR 代替）。
    {
        std::string hd = hips_dir;
        while (!hd.empty() && (hd.back() == '/' || hd.back() == '\\')) hd.pop_back();
        const size_t slash = hd.find_last_of("/\\");
        if (slash != std::string::npos) {
            const std::string parent = hd.substr(0, slash);
            const std::string frame_key = hd.substr(slash + 1);
            // 读取经 aio (aio_file::read_all, aio_fopen_utf8 通道);
            // 读取失败 = 不写键 (与原 ifstream 打开失败同语义, fail-closed)。
            std::string snr_json_text;
            if (aio_file::read_all((parent + "/p1_snr.json").c_str(),
                                   &snr_json_text)) {
                try {
                    const nlohmann::json sj =
                        nlohmann::json::parse(snr_json_text);
                    if (sj.is_object() && sj.contains("frames") &&
                        sj["frames"].is_array()) {
                        // P2b-4（RELEASE-02）: 写侧**组内公共 F_ref** 闸门。
                        // p1_snr.json 逐帧 snr_reference.flux_adu 必须组内公共
                        // （相对容差 1e-9）；逐帧 F_ref 会丢掉帧间标度 a_f² 并使
                        // Phase2 权重链 fail-closed（WEIGHT-SCI-001 配对性定理:
                        // SNR_f=a_f·F_ref/σ_f ⇒ SNR_f²/F_ref²=a_f²/σ_f²=w_f 当且
                        // 仅当分子分母同源）。数据违反 → **不写帧级 SNR 键**
                        // （fail-closed；禁写非配对键冒充合法权重链）。
                        // FREF-BASELINE-001: 两种合法写侧约定。
                        //   scope="group" ⇒ 公共量 = flux_adu（块级公共 F0, ADU）;
                        //   scope="frame_independent_fixed_magnitude" ⇒ 公共量 =
                        //     flux_common（固定参考星等的**物理**公共锚 F0, 对同
                        //     波段同星场恒为同一数）。此时逐帧 flux_adu 是**有意**
                        //     逐帧的（F_ref,k = 10^(-0.4(m_ref-ZP_k))）, 不是缺陷;
                        //     头部必须写 flux_common, 使配对性 w=SNR²/F0²=a_f²/σ_f²
                        //     成立（WEIGHT-SCI-001 Convention A）。
                        std::string scope_str = "group";
                        if (sj.contains("snr_reference_scope") &&
                            sj["snr_reference_scope"].is_string())
                            scope_str = sj["snr_reference_scope"].get<std::string>();
                        const bool use_common =
                            (scope_str == "frame_independent_fixed_magnitude");
                        const char* fref_key = use_common ? "flux_common" : "flux_adu";
                        bool group_fref_ok =
                            (scope_str == "group" || use_common);
                        double group_fref = 0.0;
                        bool group_fref_set = false;
                        for (const auto& fr : sj["frames"]) {
                            if (!fr.is_object()) continue;
                            if (!fr.contains("snr_reference") ||
                                !fr["snr_reference"].is_object()) continue;
                            const double fv =
                                fr["snr_reference"].value(fref_key, 0.0);
                            if (!std::isfinite(fv) || !(fv > 0.0)) continue;
                            if (!group_fref_set) {
                                group_fref = fv;
                                group_fref_set = true;
                            } else if (std::fabs(fv - group_fref) >
                                       1e-9 * std::fabs(group_fref)) {
                                group_fref_ok = false;
                            }
                        }
                        if (!group_fref_ok) {
                            std::fprintf(stderr,
                                "[sink][phase1] p1_snr.json F_ref 非组内公共"
                                " → 不写帧级 SNR 键 (Phase2 权重链 fail-closed;"
                                " P2b-4)\n");
                        }
                        for (const auto& fr : sj["frames"]) {
                            if (!fr.is_object()) continue;
                            if (!phase1_frame_matches(
                                    fr.value("file", std::string()), frame_key))
                                continue;
                            double snr = 0.0, fref = 0.0;
                            if (fr.contains("snr_reference") &&
                                fr["snr_reference"].is_object()) {
                                snr = fr["snr_reference"].value("snr_f", 0.0);
                                fref = fr["snr_reference"].value(fref_key, 0.0);
                            }
                            if (std::isfinite(snr) && snr > 0.0 &&
                                std::isfinite(fref) && fref > 0.0 &&
                                group_fref_ok) {
                                if (aio_hips_set_frame_snr(ps, snr, fref) != 0) {
                                    err = "aio_hips_set_frame_snr 失败: " +
                                          std::string(aio_hips_last_error()
                                                          ? aio_hips_last_error() : "?");
                                    aio_hips_abort(ps);
                                    std::fprintf(stderr, "[sink][phase1] %s\n", err.c_str());
                                    return false;
                                }
                                std::fprintf(stderr,
                                    "[sink][phase1] frame-SNR keys written:"
                                    " frame=%s snr=%.10g F_ref=%.10g\n",
                                    frame_key.c_str(), snr, fref);
                            } else {
                                std::fprintf(stderr,
                                    "[sink][phase1] frame %s: p1_snr.json"
                                    " snr_reference 非有限/非正 → 不写帧级 SNR 键"
                                    " (Phase2 权重链 fail-closed)\n",
                                    frame_key.c_str());
                            }
                            break;
                        }
                    }
                } catch (const std::exception& e) {
                    std::fprintf(stderr,
                        "[sink][phase1] p1_snr.json 解析失败 (%s) → 不写帧级 SNR 键\n",
                        e.what());
                }
            }
        }
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
            // 面亮度换算因子 k = sumArea/sumNorm = D_p/(Σ_j w_jp·A_pixel,j)。
            // 核按 **drop 面积** 归一 (drizzlepac dover/=jaco; F&H 2002 §7.2), 故
            // acc.sumFlux 是"分配通量"(Σ_p sumFlux = Σ_j x_j, 与 pixfrac 无关),
            // 而 writer 发布的 signal 面语义是**面亮度** (sig = flux/covered_area)。
            // 把 flux 折算回覆盖面积口径后:
            //   sig = sumFlux·D_p/(sumNorm·A_cov) = Σ_j B_j a_jp / Σ_j a_jp
            // (B_j = x_j/A_pixel,j), 与 pixfrac 无关 = 常量面亮度场时恒 B0。
            // pixfrac==1 时 sumNorm ≡ sumArea (逐位) ⇒ k == 1.0 ⇒ 产品逐位不变。
            const double k = ((double)acc.sumNorm > 0.0)
                                 ? (double)acc.sumArea / (double)acc.sumNorm
                                 : 1.0;
            // signal = 累计通量(折算后), HiPS 产品位深 float32 (旧 writer 的显式窄化)
            flux_buf[local] = (float)((double)acc.sumFlux * k);
            // covered_area = (u8/255)·A_cell (旧 writer 的面积比连续缩放)
            area_buf[local] = (float)(((double)q / 255.0) * a_cell);
            // variance 分子 Σ v_j·w_jp² (ADU², SCI-DRZ-014 §5); writer 归约
            // variance = var_num_sum/covered_area²、ivar = 1/variance (§12.2/§4a)。
            // 方差与信号共用同一归一分母 ⇒ 同步乘 k² (pixfrac=1 时 k≡1 逐位不变)。
            if (has_variance)
                var_buf[local] = (float)((double)acc.sumVarNum * k * k);
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
    const std::string&, const std::string&, int, std::string&);
template bool write_hips_phase1<double>(
    const std::vector<TileAccumulatorT<double>>&, const DrizzleConfig&,
    const std::string&, const std::string&, int, std::string&);

} // namespace drizzle
