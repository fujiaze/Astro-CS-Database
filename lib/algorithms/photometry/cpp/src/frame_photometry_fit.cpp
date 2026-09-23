// ============================================================================
// frame_photometry_fit.cpp - 单帧 k_photo 装配（见 frame_photometry_fit.h）
//
// 装配步骤与 orchestrator::run_stage_photometric（orchestrator.cpp:2699-2850）
// 同源同口径:
//   1. FILTER 名称映射 (map_filter_name)
//   2. filters.json / qe_curves.json 曲线加载 (与 load_filter_curve 同解析)
//   3. gaia_client 锥形搜索光谱参数 → spectrum_wl 网格
//   4. FOV 半径 = pixel_scale * sqrt(W²+H²)/2 * 1.2, 钳位 [1,10] 度
//   5. pc_calibrate_simple_with_gaia_f64_v2_qf（生产星匹配 + IRLS/Tukey）
//
// 本文件不改任何科学公式/默认容差; 只把既有生产实现装配成"给定帧 + 配置 →
// k_photo"的一个入口。
// ============================================================================

#include "frame_photometry_fit.h"

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

// FITS FILTER → filters.json 键（与 orchestrator.cpp:1342 map_filter_name 同表）
std::string map_filter_name(const std::string& f) {
    auto ieq = [](const std::string& a, const char* b) {
        return std::equal(a.begin(), a.end(), b, b + std::strlen(b),
            [](char c1, char c2) { return std::tolower(c1) == std::tolower(c2); });
    };
    if (ieq(f, "Red") || ieq(f, "R")) return "Baader R";
    if (ieq(f, "Green") || ieq(f, "G")) return "Baader G";
    if (ieq(f, "Blue") || ieq(f, "B")) return "Baader B";
    if (ieq(f, "Lum") || ieq(f, "L") || ieq(f, "Luminance"))
        return "Baader UV/IR Cut / L CMOS Optimized";
    if (ieq(f, "H-alpha") || ieq(f, "Ha") || ieq(f, "HA")) return "Baader 7nm H-alpha";
    if (ieq(f, "OIII") || ieq(f, "Oiii")) return "Baader 8.5nm OIII";
    return f;
}

// 定位曲线对象并抽数组。返回 false 表示文件/键/数组任一缺失。
//
// ── P1-PHOT-CURVE-RESOLVE 修复（2026-09，M42 真实数据根因调查）─────────────
// 缺陷（修复前）: 只做 content.find("\"name\"") 取**第一次**文本出现，再从该偏移
//   往后找第一个 "wavelength_nm"。当 filters_json 指向**转录版**
//   eng/packaging/config/filters.json（顶层顺序 [..., provenance, lookup, filters]，
//   曲线定义在 filters 段）时，名字会先在 provenance.per_filter 段命中 ⇒ 偏移落在
//   filters 段之前 ⇒ 取到 filters 段的**第一个**滤镜曲线。
//   实测（run/M42-SCIA-ROOTCAUSE-01）：配置声明 filter="Baader R" 而实际取到
//   "Antlia V Pro Series B"（53 点 / 420–524 nm），使 M42 Red 帧的 F_syn 用蓝端通带
//   合成 ⇒ 逐星残差散度从 0.019 dex 膨胀到 0.204 dex（10.7×），
//   2.5σ = 0.51 mag（EXP-04 三帧量级 0.045–0.057 mag 的 8.9 倍）。
// 修复: 只接受**同时**满足 (a) 键后紧跟 ':' 与 '{'（是对象键而非任意文本）、
//   (b) 该对象内**含 wavelength_nm 数组** 的候选；取第一个满足者。
//   provenance/其他段落的对象不含 wavelength_nm ⇒ 被跳过。
//   这是曲线**解析**修复，不改任何科学公式、常数与容差。
bool load_curve(const std::string& json_path, const std::string& curve_name,
                std::vector<double>* out_wl, std::vector<double>* out_trans) {
    // CLEAN-403: 读取经 aio; 打开/读取失败 ⇒ false (与原 !ifs.is_open() 同语义)。
    std::string content;
    if (!aio_file::read_all(json_path.c_str(), &content)) return false;
    auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    };
    const std::string key = "\"" + curve_name + "\"";
    std::string obj;
    for (size_t p = content.find(key); p != std::string::npos; p = content.find(key, p + 1)) {
        size_t q = p + key.size();
        while (q < content.size() && is_space(content[q])) ++q;
        if (q >= content.size() || content[q] != ':') continue;   // 非对象键 (如别名表/字符串值)
        ++q;
        while (q < content.size() && is_space(content[q])) ++q;
        if (q >= content.size() || content[q] != '{') continue;   // 非对象值
        // 括号配平取该对象的完整文本
        size_t depth = 0, end = std::string::npos;
        for (size_t i = q; i < content.size(); ++i) {
            if (content[i] == '{') ++depth;
            else if (content[i] == '}') { if (--depth == 0) { end = i; break; } }
        }
        if (end == std::string::npos) continue;
        std::string cand = content.substr(q, end - q + 1);
        if (cand.find("\"wavelength_nm\"") == std::string::npos) continue;  // 曲线对象判据
        obj = std::move(cand);
        break;
    }
    if (obj.empty()) return false;
    size_t pos = 0;
    auto extract_array = [&obj, &pos](const std::string& arr_key,
                                      std::vector<double>* out) -> bool {
        size_t kpos = obj.find(arr_key, pos);
        if (kpos == std::string::npos) return false;
        size_t b0 = obj.find('[', kpos);
        if (b0 == std::string::npos) return false;
        size_t b1 = obj.find(']', b0);
        if (b1 == std::string::npos) return false;
        std::string arr = obj.substr(b0 + 1, b1 - b0 - 1);
        std::replace(arr.begin(), arr.end(), ',', ' ');
        std::istringstream iss(arr);
        out->clear();
        double v = 0.0;
        while (iss >> v) out->push_back(v);
        return !out->empty();
    };
    if (!extract_array("\"wavelength_nm\"", out_wl) ||
        !extract_array("\"value\"", out_trans)) {
        return false;
    }
    return out_wl->size() == out_trans->size();
}

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

    const std::string filter_key = map_filter_name(req.filter_name);
    std::vector<double> filter_wl, filter_trans;
    if (!load_curve(req.filters_json, filter_key, &filter_wl, &filter_trans)) {
        out.error = "filter curve load failed: '" + filter_key + "' in " + req.filters_json;
        out.failure_scope = FitFailureScope::kEnvironment;   // 程序级配置输入, 非帧数据
        return out;
    }
    std::vector<double> qe_wl, qe_trans;
    if (!req.qe_json.empty() && !req.qe_name.empty()) {
        if (!load_curve(req.qe_json, req.qe_name, &qe_wl, &qe_trans)) {
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
        nullptr, nullptr,
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
    return out;
}

}  // namespace photometry
}  // namespace astrocs
