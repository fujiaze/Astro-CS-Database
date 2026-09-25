// ============================================================================
// frame_photometry_fit.cpp - 单帧 k_photo 装配（见 frame_photometry_fit.h）
//
// 装配步骤与 orchestrator::run_stage_photometric 同源同口径
//   （PHOTOCURVE-ORCH-01 后该阶段的曲线装载段见 orchestrator.cpp 的
//    "辅助: 滤光片名称映射 + 响应曲线装载" 注释块；两处都只做委托）:
//   1. FILTER 名称映射 (curve_json::map_filter_name, 唯一实现)
//   2. filters.json / qe_curves.json 曲线加载 (curve_json::load_curve, 唯一实现)
//   3. gaia_client 锥形搜索光谱参数 → spectrum_wl 网格
//   4. FOV 半径 = pixel_scale * sqrt(W²+H²)/2 * 1.2, 钳位 [1,10] 度
//   5. pc_calibrate_simple_with_gaia_f64_v2_qf（生产星匹配 + IRLS/Tukey）
//
// 本文件不改任何科学公式/默认容差; 只把既有生产实现装配成"给定帧 + 配置 →
// k_photo"的一个入口。
// ============================================================================

#include "frame_photometry_fit.h"

#include "filter_curve_json.h" // 曲线解析唯一实现 (结构化对象解析)
#include "pc_api_qf.h"          // pc_calibrate_simple_with_gaia_f64_v2_qf
#include "spectrum_integrator.h" // prepare_filter_cache / compute_f_syn_cached_xpsd
#include "../include/photometric_calib.h"

extern "C" {
#include "gaia_client.h"
}

// CLEAN-403 (ASTROCS_DESIGN §10「aio 是文件级唯一 I/O 边界」): filters.json 的
// 整文件读取经 aio 唯一实现 (aio_file::read_all), 本 TU 不自持 ifstream 通道。
#include "aio_file_io.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace astrocs {
namespace photometry {
namespace {

// ── 曲线解析: **唯一实现** = lib/algorithms/photometry/cpp/src/filter_curve_json.h ──
// 依据 docs/standards/CODE_STANDARD.md §MUST「禁止重复 production science
// implementation（单一实现 + oracle）」。本 TU 与 orchestrator 路径
// (lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp) 共用同一份
// 定位/抽取代码与同一张 FILTER→库键表（curve_json::map_filter_name）。
// 修复前两处各持一份**文本搜索**实现：在转录版 eng/packaging/config/filters.json
// 上会把 "Baader R" 解析成 filters 段的第一条曲线 "Antlia V Pro Series B"
// （53 点 / 420–524 nm）—— 缺陷记录与规范依据见 filter_curve_json.h 文件头。

}  // namespace

FramePhotFitResult fit_frame_photometry(const FramePhotFitRequest& req) {
    FramePhotFitResult out;
    if (req.pixels == nullptr || req.width <= 0 || req.height <= 0) {
        out.error = "invalid frame pixels/size";
        out.failure_scope = FitFailureScope::kEnvironment;   // 调用方装配错误, 与帧数据无关
        return out;
    }
    // F-INSTR-CONFORM-FIX: psf_flux 的域契约 = PSF 拟合域解析通量
    // F_instr = 2πA·s_x·s_y/3 (β=4, ADU; SCI-PSF-001 §2/§5, SCI-PHOT-001 §9a)。
    // 见 frame_photometry_fit.h 的逐字段契约 —— 调用方禁止传检测域 5×5 盒和。
    // 域外输入（status!=0）由下游 matchWithKdTree 的有效域门剔除, 不在此处
    // 静默降级: 有效星不足 ⇒ SCI-PHOT-001 §4/§8 的 NO_DATA（fail-closed）。
    if (req.psf_cx == nullptr || req.psf_cy == nullptr ||
        req.psf_flux == nullptr || req.psf_status == nullptr) {
        out.error = "null PSF star arrays";
        out.failure_scope = FitFailureScope::kEnvironment;   // 调用方装配错误
        return out;
    }
    // 本帧没有可用的 PSF 星 ⇒ **该帧**的测光定标无从谈起（其他帧不受影响）。
    if (req.n_psf <= 0) {
        out.error = "no PSF stars (n_psf<=0)";
        out.failure_scope = FitFailureScope::kFrame;
        return out;
    }
    if (req.gaia_data_dir.empty()) {
        out.error = "gaia_data_dir empty (spectra required for F_syn)";
        out.failure_scope = FitFailureScope::kEnvironment;
        return out;
    }
    if (req.filter_name.empty() || req.filters_json.empty()) {
        out.error = "filter_name / filters_json required";
        out.failure_scope = FitFailureScope::kEnvironment;
        return out;
    }

    const std::string filter_key = curve_json::map_filter_name(req.filter_name);
    // ── 通带身份门（装配期 fail-closed；PASSBAND-IDENTITY-GATE-01）──────────
    // (a) 声明名核对：配置**声明**的通带（块级 filter_passband）必须与
    //     FILTER 关键字解析出的库键一致。不一致 ⇒ 两者不是同一条曲线，
    //     合成 F_syn 用的通带与配置声明不符 ⇒ 拒绝产出标度（具名，环境作用域）。
    //     依据 docs/science/PHOTOMETRY.md §2a.4「比较不同帧/不同模型的
    //     sigma_residual 时必须声明所用模型通带」+ §2a.5（通带形状不被零点吸收）
    //     + eng/packaging/config/filters.json#lookup.resolution_rule（名字解析
    //     为字节精确、无别名）。**这不是新的科学判据**：它只核对"用的是不是
    //     声明的那条曲线"，不改任何公式、阈值、容差与权重。
    if (!req.declared_filter_passband.empty() &&
        req.declared_filter_passband != filter_key) {
        out.error = "passband identity mismatch: declared filter_passband '" +
                    req.declared_filter_passband + "' but FILTER '" + req.filter_name +
                    "' resolves to library key '" + filter_key +
                    "' -- refusing to synthesize F_syn with a passband other than the"
                    " declared one";
        out.failure_scope = FitFailureScope::kEnvironment;   // 配置声明矛盾, 非帧数据
        return out;
    }
    std::vector<double> filter_wl, filter_trans;
    curve_json::IdentityMismatch filter_id;
    const curve_json::LoadStatus filter_st =
        curve_json::load_curve(req.filters_json, filter_key, &filter_wl, &filter_trans,
                               &filter_id);
    if (filter_st != curve_json::LoadStatus::kOk) {
        // 曲线名解析不到 ⇒ 具名报错 (不静默取到别的曲线, CODE_STANDARD §MUST
        // 「禁止 silent config fallback 改变科学语义」)。
        // kCurveIdentityMismatch ⇒ 曲线对象的自述身份与请求名/provenance 声明
        // 不符：**这正是「按文本位置取错通带」的判别式**，必须把实际取到的身份
        // 写进判词（否则下游只能看到一个"某条曲线"）。
        out.error = "filter curve load failed: '" + filter_key + "' in " + req.filters_json +
                    " (" + curve_json::status_name(filter_st) + ")";
        if (filter_st == curve_json::LoadStatus::kCurveIdentityMismatch) {
            out.error += " [passband identity: requested='" + filter_id.requested +
                         "' object_name='" +
                         (filter_id.object_name_present ? filter_id.object_name
                                                        : std::string("<absent>")) +
                         "' reason=" + filter_id.reason +
                         " detail=" + filter_id.detail + "]";
        }
        out.failure_scope = FitFailureScope::kEnvironment;   // 程序级配置输入, 非帧数据
        return out;
    }
    // 身份自述（由曲线对象自身与实际数组算出；身份门已保证 name == filter_key）。
    out.filter_key = filter_key;
    out.filter_curve_name = filter_key;
    out.filter_n_points = static_cast<int>(filter_wl.size());
    out.filter_wl_min_nm = *std::min_element(filter_wl.begin(), filter_wl.end());
    out.filter_wl_max_nm = *std::max_element(filter_wl.begin(), filter_wl.end());
    out.filter_val_min = *std::min_element(filter_trans.begin(), filter_trans.end());
    out.filter_val_max = *std::max_element(filter_trans.begin(), filter_trans.end());
    std::vector<double> qe_wl, qe_trans;
    if (!req.qe_json.empty() && !req.qe_name.empty()) {
        const curve_json::LoadStatus qe_st =
            curve_json::load_curve(req.qe_json, req.qe_name, &qe_wl, &qe_trans);
        if (qe_st != curve_json::LoadStatus::kOk) {
            qe_wl.clear();
            qe_trans.clear();
            // 曲线名/文件存在但解析失败 ⇒ 不得静默退化为 Q(λ)≡1（通带错配是合成
            // 测光定标的主误差项，见 docs/references/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md
            // §1.4）；如实报出，由调用方决定是否判红。
            std::fprintf(stderr, "[photometry] WARNING: QE 曲线 '%s' 在 %s 中解析失败, "
                                 "按 Q(lambda)=1 继续 (显式未建模项)\n",
                         req.qe_name.c_str(), req.qe_json.c_str());
        }
    } else {
        // 配置未提供 QE ⇒ Q(λ)≡1（既有语义，不阻断）；补一条显式告警，避免"没配 QE"
        // 与"QE 已计入"在下游不可区分。
        std::fprintf(stderr, "[photometry] WARNING: qe_json/qe_name 未配置, "
                             "F_syn 按 Q(lambda)=1 合成 (显式未建模项)\n");
    }

    // FOV 半径（与 orchestrator.cpp:2771-2778 同式同钳位）
    const double cd_det = std::fabs(req.cd11 * req.cd22 - req.cd12 * req.cd21);
    const double pixel_scale_deg = (cd_det > 0.0) ? std::sqrt(cd_det) : 0.0;
    double fov_radius_deg =
        pixel_scale_deg *
        std::sqrt(static_cast<double>(req.width) * req.width +
                  static_cast<double>(req.height) * req.height) / 2.0 * 1.2;
    if (fov_radius_deg <= 0.0 || fov_radius_deg >= 30.0) {
        fov_radius_deg = std::min(std::max(fov_radius_deg, 1.0), 10.0);
    }

    GaiaClient* client = gaia_client_create(req.gaia_data_dir.c_str());
    if (client == nullptr) {
        out.error = "gaia_client_create failed: " + req.gaia_data_dir;
        out.failure_scope = FitFailureScope::kEnvironment;   // 星表目录不可打开
        return out;
    }

    int wl_start = 0, wl_step = 0, wl_count = 0;
    const int prc = gaia_client_get_spectrum_params(client, &wl_start, &wl_step, &wl_count);
    if (prc != 1 || wl_count <= 0 || wl_step <= 0) {
        gaia_client_destroy(client);
        out.error = "gaia_client_get_spectrum_params failed (rc=" + std::to_string(prc) +
                    ", count=" + std::to_string(wl_count) + ")";
        out.failure_scope = FitFailureScope::kEnvironment;   // 星表不可读
        return out;
    }
    std::vector<double> spectrum_wl(static_cast<size_t>(wl_count));
    for (int i = 0; i < wl_count; ++i) {
        spectrum_wl[static_cast<size_t>(i)] = static_cast<double>(wl_start + i * wl_step);
    }

    const size_t npix = static_cast<size_t>(req.width) * static_cast<size_t>(req.height);
    std::vector<double> out_pixels(npix, 0.0);

    int n_matched = 0;
    double scale = 1.0;
    double sigma_residual = 0.0;
    PhotometricDiag diag;
    std::memset(&diag, 0, sizeof(diag));

    // ── PHOT-MXY-01: 逐星 inlier 记录（仅空间增益开启时申请）─────────────────
    // records[i] 与 PSF 行 i 对齐（pc_api.cpp 的 per-star 记录块）；status==1 表示
    // matched+used（IRLS inlier），residual = r_i = log10(F_instr/F_syn)。
    // **关闭空间增益时传 nullptr**：C 入口调用参数与改动前逐位一致。
    std::vector<PcMatchRecord> records;
    PcMatchRecord* records_out = nullptr;
    if (req.spatial_gain_order > 0) {
        records.resize(static_cast<std::size_t>(req.n_psf));
        records_out = records.data();
    }

    const int rc = pc_calibrate_simple_with_gaia_f64_v2_qf(
        reinterpret_cast<void*>(client),
        req.crval1, req.crval2, fov_radius_deg,
        req.mag_min, req.mag_max,
        filter_wl.data(), filter_trans.data(), static_cast<int>(filter_wl.size()),
        qe_wl.empty() ? nullptr : qe_wl.data(),
        qe_trans.empty() ? nullptr : qe_trans.data(),
        static_cast<int>(qe_wl.size()),
        spectrum_wl.data(), static_cast<int>(spectrum_wl.size()),
        req.pixels, req.width, req.height,
        req.psf_cx, req.psf_cy, req.psf_flux, req.psf_status, req.n_psf,
        nullptr, records_out,
        req.crval1, req.crval2, req.crpix1, req.crpix2,
        req.cd11, req.cd12, req.cd21, req.cd22,
        req.sip_order, req.sip_a, req.sip_b, req.sip_ap, req.sip_bp,
        out_pixels.data(), &n_matched, &scale, &sigma_residual, &diag,
        req.psf_quality);

    // ── FREF-BASELINE-001: 绝对合成星等零点 ZP_syn ──────────────────────
    // 目的: 给帧级 SNR 提供一个**跨帧公共**的绝对参考锚（负责人裁决:
    // "直接用 6 等星/一个数值表示比较正常的星等来做基准"）。定义与
    // snr_science.cpp:234 的 m_5 约定一致: mag = ZP_syn - 2.5*log10(F_syn)。
    //
    // 取值 = median_i( magG_i + 2.5*log10 F_syn,i )，对**锥形搜索星族**统计。
    // 由 Gaia DR3 XP 绝对谱 (XPSD) + 本帧滤光片/QE 曲线**正向**合成 ⇒ 只依赖
    // (filter, QE, 天区星族)，与帧的噪声/检出深度/曝光无关。同一波段同一星场的
    // 各帧得到同一个 ZP_syn（散布仅来自锥形边界处的星族抽样）。
    //
    // 严禁用它反推增益/口径/曝光（§9.42 物理闭合禁令）。
    {
        GaiaSpectrumStar* zp_stars = nullptr;
        uint8_t* zp_spectra = nullptr;
        int zp_n = 0;
        const int zrc = gaia_client_cone_search_with_spectrum(
            client, req.crval1, req.crval2, fov_radius_deg,
            req.mag_min, req.mag_max, &zp_stars, &zp_spectra, &zp_n);
        if (zrc == 0 && zp_stars != nullptr && zp_spectra != nullptr && zp_n > 0) {
            const photo_calib::SpectrumIntegratorCache zcache =
                photo_calib::prepare_filter_cache(
                    filter_wl.data(), filter_trans.data(),
                    static_cast<int>(filter_wl.size()),
                    qe_wl.empty() ? nullptr : qe_wl.data(),
                    qe_trans.empty() ? nullptr : qe_trans.data(),
                    static_cast<int>(qe_wl.size()),
                    spectrum_wl.data(), static_cast<int>(spectrum_wl.size()));
            if (!zcache.spectrum_wl.empty()) {
                std::vector<double> zp_vals;
                zp_vals.reserve(static_cast<size_t>(zp_n));
                for (int i = 0; i < zp_n; ++i) {
                    const uint8_t* sp_i =
                        zp_spectra + static_cast<size_t>(i) * static_cast<size_t>(wl_count);
                    const double fsyn = photo_calib::compute_f_syn_cached_xpsd(
                        zcache, sp_i, wl_count,
                        zp_stars[i].flux_min, zp_stars[i].flux_mul);
                    if (!(std::isfinite(fsyn) && fsyn > 0.0)) continue;
                    if (!std::isfinite(zp_stars[i].magG)) continue;
                    const double z = zp_stars[i].magG + 2.5 * std::log10(fsyn);
                    if (std::isfinite(z)) zp_vals.push_back(z);
                }
                if (static_cast<int>(zp_vals.size()) >= kMinFitStars) {
                    std::sort(zp_vals.begin(), zp_vals.end());
                    const std::size_t zn = zp_vals.size();
                    const double zmed = (zn % 2 == 1)
                        ? zp_vals[zn / 2]
                        : 0.5 * (zp_vals[zn / 2 - 1] + zp_vals[zn / 2]);
                    std::vector<double> zdev;
                    zdev.reserve(zn);
                    for (std::size_t k = 0; k < zn; ++k)
                        zdev.push_back(std::fabs(zp_vals[k] - zmed));
                    std::sort(zdev.begin(), zdev.end());
                    const double zmad = (zn % 2 == 1)
                        ? zdev[zn / 2]
                        : 0.5 * (zdev[zn / 2 - 1] + zdev[zn / 2]);
                    out.zero_point_mag = zmed;
                    out.zero_point_n_stars = static_cast<int>(zn);
                    out.zero_point_scatter_mag = 1.4826 * zmad;
                    out.zero_point_valid = true;
                }
            }
        }
        if (zp_stars) free(zp_stars);
        if (zp_spectra) free(zp_spectra);
    }

    gaia_client_destroy(client);

    out.rc = rc;
    out.k_photo = scale;
    out.n_matched = n_matched;
    out.sigma_residual_dex = sigma_residual;
    out.n_gaia = diag.spectrum_rows_total;
    out.psf_valid = diag.psf_valid;
    out.robust_iterations = diag.robust_iterations;
    // ── P1-PHOT-BROKEN: 如实上报"拟合是否真的产出标度" ────────────────────
    // 冻结 C 入口在 NO_DATA/退化分支（无 PSF 星 / 无光谱星 / 滤光片缓存失败 /
    // SCI-PHOT-001 §4 冻结门 |r_consistent|<3）返回 rc==0 且 scale=1.0、
    // fit_used=0。这些分支**没有产出标度**, 1.0 是占位值。修复前调用方只查
    // finite&&>0 就施加并声明 applied=true（伪造 1.0）。
    // 此处把退化显式化: rc<0 + fit_ok=false + degraded_reason（机器可读），
    // k_photo 保留 1.0 但**不得**被施加。
    if (rc != 0) {
        out.error = "pc_calibrate_simple_with_gaia_f64_v2_qf rc=" + std::to_string(rc);
        out.degraded_reason = "c_api_rc_" + std::to_string(rc);
        out.k_photo = 1.0;
        out.fit_ok = false;
        // 冻结 C 入口的非零返回全部是**入口自身**的判决（参数装配 / 锥形搜索失败 /
        // C 边界异常）—— 不是"本帧星少"。换一帧不会变好 ⇒ 环境作用域, 调用方中止。
        out.failure_scope = FitFailureScope::kEnvironment;
    } else if (!(std::isfinite(scale) && scale > 0.0)) {
        out.rc = -5;
        out.k_photo = 1.0;
        out.error = "non-physical scale from fit";
        out.degraded_reason = "non_physical_scale";
        out.fit_ok = false;
        out.failure_scope = FitFailureScope::kFrame;
    } else if (n_matched < kMinFitStars) {
        // SCI-PHOT-001 §8「无星/星数不足 → NO_DATA」: 没有可施加的标度。
        out.rc = -6;
        out.k_photo = 1.0;
        out.fit_ok = false;
        out.degraded_reason = "no_data_n_matched_" + std::to_string(n_matched);
        out.error = "photometry fit produced no scale (NO_DATA): n_matched=" +
                    std::to_string(n_matched) + " < " + std::to_string(kMinFitStars) +
                    " (SCI-PHOT-001 §4/§8)";
        // NO_DATA = 本帧的星点/匹配结果不满足 §4 求解前提 ⇒ 该帧 fail（帧间独立）。
        out.failure_scope = FitFailureScope::kFrame;
    } else {
        out.fit_ok = true;
        out.failure_scope = FitFailureScope::kNone;
    }

    // ── PHOT-MXY-01: 低阶乘性空间增益 m(x,y)（SCI-PHOT-001 §16.1 ④）─────────
    // 只在标度成立时做（标度不成立时该帧已判 fail，空间项无意义）。空间项是
    // **零均值**修正：全局项仍是既有 k_photo，故 order=0 时与改动前逐位一致。
    // location_dex 由 scale 反算（−log10 k_photo）：这是 1 ulp 量级的往返，且
    // 只进入阶段 3 的 Tukey 权重与残差诊断——空间系数对 location 的**常数平移
    // 严格不变**（基函数按样本加权中心化），故不改变发布结果。
    if (req.spatial_gain_order > 0) {
        if (!out.fit_ok) {
            out.spatial.order = 0;
            out.spatial.status = SpatialGainStatus::kDisabled;
            out.spatial.degraded_reason = "scalar_fit_not_ok";
        } else {
            std::vector<SpatialGainSample> samples;
            samples.reserve(static_cast<std::size_t>(req.n_psf));
            for (int i = 0; i < req.n_psf; ++i) {
                if (records[static_cast<std::size_t>(i)].status != 1) continue;  // 非 inlier
                const double r = records[static_cast<std::size_t>(i)].residual;
                if (!std::isfinite(r)) continue;
                SpatialGainSample sm;
                sm.x = req.psf_cx[i];
                sm.y = req.psf_cy[i];
                sm.r = r;
                samples.push_back(sm);
            }
            SpatialGainParams sp;
            sp.order_requested = req.spatial_gain_order;
            sp.location_dex = -std::log10(scale);
            sp.width = req.width;
            sp.height = req.height;
            sp.min_stars_order1 = req.spatial_gain_min_stars_order1;
            sp.min_stars_order2 = req.spatial_gain_min_stars_order2;
            sp.coverage_block_grid = req.spatial_gain_coverage_block_grid;
            sp.coverage_min_stars_per_block = req.spatial_gain_coverage_min_stars_per_block;
            sp.coverage_min_blocks = req.spatial_gain_coverage_min_blocks;
            sp.coverage_min_bbox_frac = req.spatial_gain_coverage_min_bbox_frac;
            out.spatial = fit_spatial_gain(samples.empty() ? nullptr : samples.data(),
                                           static_cast<int>(samples.size()), sp);
            out.spatial.sigma_global_dex = sigma_residual;
            out.spatial_frame_fail = out.spatial.frame_fail;
        }
    }
    return out;
}

}  // namespace photometry
}  // namespace astrocs
