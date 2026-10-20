// ============================================================================
// frame_photometry_fit.cpp - 单帧 k_photo 装配（见 frame_photometry_fit.h）
//
// 装配步骤:
//   1. 通带名 = 配置声明值（`photometry.fit.filter`，否则块级 `filter_passband`），
//      在**运行期扫描集**里按库键**字节精确**解析（无归一、无别名；曲线库 =
//      <可执行文件目录>/config/response_curves/，唯一实现见 filter_curve_json.h）
//   2. 滤镜曲线与 QE 曲线都从同一扫描集装载（同一文件格式、同一身份核对）
//   3. gaia_client 锥形搜索光谱参数 → spectrum_wl 网格
//   4. FOV 半径 = pixel_scale * sqrt(W²+H²)/2 * 1.2；越界保护（≤0 或 ≥30 deg）
//      触发时钳到 [1,10] deg（区间内取值原样透传，非区间外才钳）
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
#include "crypto/sha256.h"     // acsd::crypto::sha256_hex（响应身份摘要, 单一实现）
#include "canonical_double.h"  // 无损浮点规范串（身份串的唯一浮点写法, 单一实现）

extern "C" {
#include "gaia_client.h"
}

// CLEAN-403 (docs/ACSD_DESIGN §10「统一 I/O 是文件级唯一边界」): filters.json 的
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

namespace acsd {
namespace photometry {
namespace {

// ── 曲线解析: **唯一实现** = lib/algorithms/photometry/cpp/src/filter_curve_json.h ──
// 依据 docs/engineering/standards/CODE.md「MUST」一节「禁止重复 production science
// implementation（单一实现 + oracle）」。本 TU 只做委托：曲线库 = 运行期扫描
// `<可执行文件目录>/config/response_curves/`（`curve_json::scan_curve_library`），
// 通带名与 QE 名都按库键**字节精确**命中扫描集（无归一、无别名）。

// 规范浮点序列化（响应身份串的**唯一**浮点写法）: 无损 hexfloat
// （IEEE-754 值 ↔ 文本一一对应, strtod 可逐位回读, 与 locale 无关）。
// 旧写法 "%.12g" **不是单射**（656.28 与 nextafter(656.28,+∞) 同串）⇒
// 不同响应参数会共享同一身份串（#106 独立审核 P2-1）。
inline std::string canon_num(double v) {
    return acsd::shared::canonical_double_hex(v);
}

// ── 物理响应身份（response:v1:…）──────────────────────────────────────────
// 规范串冻结: 模式/剖面/版本/中心/带宽/解析端点/QE 身份与有序内容 hash/
// 滤镜身份与有序内容 hash。**不含** XP 覆盖状态、求积约定与 XP 网格
// ⇒ 换 XP 网格不换「滤镜身份」（#106 独立审核 P2-2）。
inline std::string response_canonical_string(const ResponseIdentity& id) {
    std::string canon;
    canon += "mode=" + id.mode;
    canon += "|profile=" + id.profile;
    canon += "|profile_version=" + std::to_string(id.profile_version);
    canon += "|center_nm=" + canon_num(id.center_nm);
    canon += "|fwhm_nm=" + canon_num(id.fwhm_nm);
    canon += "|band_lo_nm=" + canon_num(id.band_lo_nm);
    canon += "|band_hi_nm=" + canon_num(id.band_hi_nm);
    canon += "|qe_key=" + id.qe_key;
    canon += "|qe_sha256=" + id.qe_content_sha256;
    canon += "|filter_key=" + id.filter_key;
    canon += "|filter_sha256=" + id.filter_content_sha256;
    return canon;
}

inline std::string response_id_of(const ResponseIdentity& id) {
    const std::string canon = response_canonical_string(id);
    const std::string hex = acsd::crypto::sha256_hex(canon.data(), canon.size());
    return "response:v1:" + hex.substr(0, 32);
}

// ── 合成模型身份（synthesis:v1:…）──────────────────────────────────────────
// = 物理响应身份 + XP 覆盖状态 + 求积约定 + XP 可重建谱网格。
// 它是**标定 provenance**（这一步合成所用的模型状态），与「仪器响应身份」
// 分列发布: 换 XP 网格 ⇒ synthesis_id 变、response_id 不变。
inline std::string synthesis_id_of(const ResponseIdentity& id) {
    std::string canon = response_canonical_string(id);
    canon += "|integration=" + id.integration;
    canon += "|coverage=" + id.coverage;
    canon += "|xp_wl_start=" + std::to_string(id.xp_wl_start);
    canon += "|xp_wl_step=" + std::to_string(id.xp_wl_step);
    canon += "|xp_wl_count=" + std::to_string(id.xp_wl_count);
    const std::string hex = acsd::crypto::sha256_hex(canon.data(), canon.size());
    return "synthesis:v1:" + hex.substr(0, 32);
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
    if (req.filter_name.empty() && req.response.mode == ResponseMode::kUnspecified) {
        out.error = "filter_name required (photometry.fit.filter / filter_passband)";
        out.failure_scope = FitFailureScope::kEnvironment;
        return out;
    }
    // ── Issue #106: 响应来源 one-of 的分发 ─────────────────────────────────
    //   kUnspecified          ⇒ 历史 measured 路径（req.filter_name）
    //   kMeasuredCurve        ⇒ 新对象的 measured 形式（req.response.curve_name）
    //   kParametricNarrowband ⇒ 参数化窄带（center/fwhm; **不查滤镜库**）
    const ResponseMode mode = (req.response.mode == ResponseMode::kUnspecified)
                                  ? ResponseMode::kMeasuredCurve
                                  : req.response.mode;
    const bool parametric = (mode == ResponseMode::kParametricNarrowband);
    photo_calib::ParametricBand band;
    if (parametric) {
        if (req.response.profile != kNarrowbandProfile ||
            req.response.profile_version != kNarrowbandProfileVersion) {
            out.error = "INVALID_PARAMETRIC_RESPONSE (unsupported profile '" +
                        req.response.profile + "'/version " +
                        std::to_string(req.response.profile_version) +
                        "; only " + kNarrowbandProfile + "/v" +
                        std::to_string(kNarrowbandProfileVersion) + " is defined)";
            out.failure_scope = FitFailureScope::kEnvironment;   // 配置缺项/未知剖面
            return out;
        }
        band = photo_calib::make_parametric_band(req.response.center_nm,
                                                  req.response.fwhm_nm);
        if (!band.valid) {
            out.error = band.error;
            out.failure_scope = FitFailureScope::kEnvironment;   // 配置数值非法
            return out;
        }
    }
    // 滤镜曲线名取自**请求声明的来源**（不是归一后的 mode）:
    //   · 旧路径（req.response.mode == kUnspecified）⇒ `req.filter_name`
    //     （photometry.fit.filter / 块级 filter_passband / 逐帧 override.filter）;
    //   · 新对象（req.response.mode == kMeasuredCurve）⇒ `req.response.curve_name`。
    // ⚠ 归一后的局部 `mode`（kUnspecified → kMeasuredCurve）**不能**用来区分这两
    //   条来源: 按归一 mode 取 curve_name 会让**旧 measured 路径**读到空的
    //   curve_name ⇒ 具名失败「filter_name required」（#106 第二轮生产 E2E 实测
    //   发现的回归; 第一轮「旧路径逐位不变」的登记在此订正）。
    const std::string filter_key =
        parametric ? std::string()
                   : (req.response.mode == ResponseMode::kMeasuredCurve
                          ? req.response.curve_name
                          : req.filter_name);
    if (!parametric && filter_key.empty()) {
        out.error = "filter_name required (photometry.fit.filter / filter_passband)";
        out.failure_scope = FitFailureScope::kEnvironment;
        return out;
    }

    // ── 曲线库 = 运行期扫描（**唯一解析实现**，见 filter_curve_json.h）─────────
    // 目录 = <可执行文件目录>/config/response_curves/；目录不可达/空、文件不可解析、
    // 重名、身份核对失败 ⇒ 具名 fail-closed（不回退到任何内建清单，也不当作「无滤镜」）。
    curve_json::CurveLibrary lib;
    std::string scan_detail;
    const curve_json::ScanStatus scan_st = curve_json::scan_curve_library(&lib, &scan_detail);
    if (scan_st != curve_json::ScanStatus::kOk) {
        out.error = curve_json::scan_failure_message(scan_st, lib, scan_detail);
        out.failure_scope = FitFailureScope::kEnvironment;   // 程序级配置输入, 非帧数据
        return out;
    }

    // 曲线名**只**来自配置声明（req.filter_name = photometry.fit.filter / 块级
    // filter_passband，或新对象的 measured_curve.curve_name），在扫描集里按库键
    // 字节精确解析：不折叠大小写/空白、不做 Unicode 归一、不解析别名
    // （docs/engineering/contracts/CONFIG.md「响应曲线库」一节）。
    // 参数化窄带模式**不查滤镜库**（用户无需提供滤镜型号条目）。
    std::vector<double> filter_wl, filter_trans;
    if (!parametric) {
    // ── 通带身份门（装配期 fail-closed；PASSBAND-IDENTITY-GATE-01）──────────
    // (a) 声明名核对：块级 `filter_passband` 与 `photometry.fit.filter` 必须
    //     **字节相等**（同一配置内的两条声明必须指向同一条曲线）。不一致 ⇒
    //     合成 F_syn 用的通带与声明不符 ⇒ 拒绝产出标度（具名，环境作用域）。
    //     依据 docs/science/PHOTOMETRY.md §2a.4「比较不同帧/不同模型的
    //     sigma_residual 时必须声明所用模型通带」+ §2a.5（通带形状不被零点吸收）
    //     + 合同「响应曲线库」的匹配语义（字节精确、无归一、无别名）。
    //     **这不是新的科学判据**：它只核对"用的是不是声明的那条曲线"，不改任何
    //     公式、阈值、容差与权重。
    //     仅历史 measured 路径适用: 新 `response` 对象与旧滤镜字段并存由配置层
    //     判 RESPONSE_SOURCE_CONFLICT（module_adapters），此处不再重复核对。
    if (mode == ResponseMode::kMeasuredCurve && req.response.mode == ResponseMode::kUnspecified &&
        !req.declared_filter_passband.empty() &&
        req.declared_filter_passband != filter_key) {
        out.error = "passband identity mismatch: declared filter_passband '" +
                    req.declared_filter_passband + "' but photometry.fit.filter '" +
                    req.filter_name + "' (library key '" + filter_key +
                    "') -- refusing to synthesize F_syn with a passband other than the"
                    " declared one";
        out.failure_scope = FitFailureScope::kEnvironment;   // 配置声明矛盾, 非帧数据
        return out;
    }
    const curve_json::CurveEntry* fe = lib.find_class(filter_key, /*want_qe=*/false);
    if (fe == nullptr) {
        // 名字不在扫描集（或命中的是 QE 曲线）⇒ 具名 `unknown_filter` + 候选 + 总数。
        if (lib.find(filter_key) != nullptr) {
            out.error = "curve_channel_class_mismatch: '" + filter_key +
                        "' is a QE curve (channel=\"Q\") in the scanned library (dir='" +
                        lib.dir + "'); a passband name must hit a non-QE curve";
        } else {
            out.error = curve_json::unknown_curve_message(lib, filter_key, /*want_qe=*/false);
        }
        out.failure_scope = FitFailureScope::kEnvironment;   // 程序级配置输入, 非帧数据
        return out;
    }
    // channel 校验（合同）：`channel == ""` 的曲线在 F_syn 通带积分路径 fail-closed。
    if (fe->channel.empty()) {
        out.error = "curve channel undeclared (channel=\"\") for passband '" + filter_key +
                    "' in " + fe->file +
                    " -- refusing to integrate F_syn with an unclassified curve";
        out.failure_scope = FitFailureScope::kEnvironment;
        return out;
    }
    filter_wl = fe->wl;
    filter_trans = fe->value;
    // 身份自述（由曲线对象自身与实际数组算出；扫描期身份核对已保证 name == 库键）。
    out.filter_key = filter_key;
    out.filter_curve_name = filter_key;
    out.filter_curve_source = fe->file;
    out.filter_n_points = static_cast<int>(filter_wl.size());
    out.filter_wl_min_nm = *std::min_element(filter_wl.begin(), filter_wl.end());
    out.filter_wl_max_nm = *std::max_element(filter_wl.begin(), filter_wl.end());
    out.filter_val_min = *std::min_element(filter_trans.begin(), filter_trans.end());
    out.filter_val_max = *std::max_element(filter_trans.begin(), filter_trans.end());
    }
    std::vector<double> qe_wl, qe_trans;
    if (!req.qe_name.empty()) {
        // QE 与滤镜**同一套规则、同一份解析实现**：同一个扫描集里按库键字节精确解析
        // （无归一、无别名；QE = `channel == "Q"`），身份核对已在扫描期完成。
        // 不得静默退化为 Q(λ)≡1（通带错配是合成测光定标的主误差项，见
        // 实验/photometric-magnitude/docs/PHOTOMETRY_LITERATURE_REVIEW_ARCHIVE.md §1.4）。
        const curve_json::CurveEntry* qe = lib.find_class(req.qe_name, /*want_qe=*/true);
        if (qe == nullptr) {
            if (lib.find(req.qe_name) != nullptr) {
                out.error = "curve_channel_class_mismatch: '" + req.qe_name +
                            "' is not a QE curve (channel != \"Q\") in the scanned"
                            " library (dir='" + lib.dir + "')";
            } else {
                out.error = curve_json::unknown_curve_message(lib, req.qe_name, /*want_qe=*/true);
            }
            out.failure_scope = FitFailureScope::kEnvironment;   // 配置输入, 非帧数据
            return out;
        }
        qe_wl = qe->wl;
        qe_trans = qe->value;
        out.qe_key = qe->key;
        out.qe_curve_name = qe->key;
        out.qe_curve_source = qe->file;
        out.qe_n_points = static_cast<int>(qe_wl.size());
        out.qe_wl_min_nm = *std::min_element(qe_wl.begin(), qe_wl.end());
        out.qe_wl_max_nm = *std::max_element(qe_wl.begin(), qe_wl.end());
        out.qe_val_min = *std::min_element(qe_trans.begin(), qe_trans.end());
        out.qe_val_max = *std::max_element(qe_trans.begin(), qe_trans.end());
    } else {
        // 未声明 QE ⇒ **具名失败**，不得静默按 Q(λ)≡1 合成。
        // 依据 docs/science/photometry/PHOTOMETRY.md §2（Q 是通带的组成部分而非可选项；
        // 必须显式声明并命中曲线库；确需理想平坦探测器时显式声明库内 `Ideal QE curve`）
        // 与负责人的运行合同「QE 和滤镜是一样的要求」——静默回退就是第二条路径。
        // 生产运行另有同一语义的配置校验点（module_adapters 的 PHOT_CONFIG_QE_MISSING）。
        out.error = "qe_name required (Q is part of the passband, not optional;"
                    " declare a scanned library key, e.g. KAF-16803, or `Ideal QE curve`"
                    " for a flat detector)";
        out.failure_scope = FitFailureScope::kEnvironment;   // 配置输入, 非帧数据
        return out;
    }

    // ── Issue #106: 响应身份（provenance）──────────────────────────────────
    // 冻结: 模式/剖面/版本/中心/带宽/解析端点/求积约定/QE 身份与**有序内容**
    // hash/（measured）滤镜身份与有序内容 hash/覆盖状态/已知程度/来源。
    // PARAMETRIC_APPROX ≠ MEASURED_CURVE: 即使中心与 FWHM 相同, 两者也**不是**
    // 同一物理响应（top-hat 是具名形状假设, 不是实测透过率）。
    {
        ResponseIdentity& rid = out.response;
        rid.mode = parametric ? "parametric_narrowband" : "measured_curve";
        rid.source = req.response.source.empty() ? std::string("block_inherit")
                                                 : req.response.source;
        rid.qe_key = out.qe_key;
        rid.qe_content_sha256 =
            curve_json::curve_content_sha256(qe_wl, qe_trans);
        if (parametric) {
            rid.profile = req.response.profile;
            rid.profile_version = req.response.profile_version;
            rid.center_nm = req.response.center_nm;
            rid.fwhm_nm = req.response.fwhm_nm;
            rid.band_lo_nm = band.lo_nm;
            rid.band_hi_nm = band.hi_nm;
            rid.integration = "segmented_gauss_legendre4_v1";
            rid.knowledge = "parametric_approx";
            // 覆盖状态（#44）在拿到 XP 网格后填充（见下方 spectrum_wl 之后）。
        } else {
            rid.integration = "simpson_xp_grid_v1";
            rid.knowledge = "measured";
            rid.filter_key = out.filter_key;
            rid.filter_content_sha256 =
                curve_json::curve_content_sha256(filter_wl, filter_trans);
        }
    }

    // FOV 半径（与 Orchestrator::run_stage_photometric 的 "计算 FOV 半径" 段同式同钳位）
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

    // ── Issue #106: 覆盖状态 + 响应身份 id（需要 XP 网格）──────────────────
    // 覆盖状态 = 响应带与 (XP 可重建域 ∩ QE 覆盖域) 的关系（#44）:
    //   full / xp_truncated / qe_truncated / no_overlap。
    // no_overlap ⇒ 该响应在有效谱域内**无支持** ⇒ F_syn ≡ 0 ⇒ 拟合无法产出
    // 标度（由下游 F_syn>0 判据拒绝）, 此处如实登记, 不静默当"零光子"。
    //
    // **身份分列**（#106 独立审核 P2-2）:
    //   response_id  = 物理响应身份（形状 + 滤镜/QE 内容）: 换 XP 网格不变;
    //   synthesis_id = 合成模型身份（+ 覆盖 + 求积 + XP 网格）: 标定 provenance。
    // XP 网格在此登记（合成模型身份的一部分）。
    out.response.xp_wl_start = wl_start;
    out.response.xp_wl_step = wl_step;
    out.response.xp_wl_count = wl_count;
    if (parametric) {
        const photo_calib::SegmentedResponseCache probe =
            photo_calib::prepare_segmented_response_cache(
                band, qe_wl.empty() ? nullptr : qe_wl.data(),
                qe_trans.empty() ? nullptr : qe_trans.data(),
                static_cast<int>(qe_wl.size()),
                spectrum_wl.data(), static_cast<int>(spectrum_wl.size()));
        out.response.coverage = probe.valid
            ? photo_calib::response_coverage_name(probe.coverage)
            : "invalid";
    }
    out.response.response_id = response_id_of(out.response);
    out.response.synthesis_id = synthesis_id_of(out.response);

    const size_t npix = static_cast<size_t>(req.width) * static_cast<size_t>(req.height);
    std::vector<double> out_pixels(npix, 0.0);

    int n_matched = 0;
    double scale = 1.0;
    double sigma_residual = 0.0;
    PhotometricDiag diag;
    std::memset(&diag, 0, sizeof(diag));

    // ── PER-STAR-EXPORT-01: 逐星 inlier 记录（**无条件申请**）──────────────
    // records[i] 与 PSF 行 i 对齐（pc_api.cpp 的 per-star 记录块）；status==1 表示
    // matched+used（IRLS inlier），residual = r_i = log10(F_instr/F_syn)。
    // 逐星导出是**独立记录面**，与空间增益是否开启无关；此前"仅空间增益开启时申请"
    // 会让未开空间增益的帧没有逐星记录。下游消费（:355 段）仍只在 spatial_gain_order>0
    // 时读取本容器，故放宽申请条件不改变既有行为。
    std::vector<PcMatchRecord> records(static_cast<std::size_t>(req.n_psf));
    PcMatchRecord* records_out = records.empty() ? nullptr : records.data();
    // NaN 占位：records 经 vector 值初始化读出为 0.0，而 0.0 会被下游误读成
    // "通量为 0 的星"。**在逐星填充路径落地之前，f_instr/gaia_mag 不得被消费**；
    // 填充路径必须对未匹配/非有限值显式写 NaN，不得留 0.0。
    for (PcMatchRecord& rec : records) {
        rec.f_instr = NAN;
        rec.gaia_mag = NAN;
    }

    // ── PER-STAR-EXPORT-01: 零点路逐星记录 ─────────────────────────────────
    // **与 records 不是同一索引域**：本容器按零点锥形搜索返回的星表 `zp_stars`
    // 索引（长度 `zp_n`），records 按 PSF 行索引（长度 `req.n_psf`）。
    // 两者**不得混用索引**，也不得假定二者等长或同序。
    struct ZpStarRow {
        double fsyn;     // 该星按 (filter, QE, 本帧光谱模板) 正向合成的 F_syn；非有限 = NaN
        double mag_g;    // 该星 Gaia G 星等；非有限 = NaN
        bool in_stats;   // 是否真正进入零点统计样本（进入 zp_vals 且样本量达 kMinFitStars）
    };
    std::vector<ZpStarRow> zp_rows;

    const int rc = pc_calibrate_simple_with_gaia_f64_v2_qf(
        reinterpret_cast<void*>(client),
        req.crval1, req.crval2, fov_radius_deg,
        req.mag_min, req.mag_max,
        parametric ? nullptr : filter_wl.data(),
        parametric ? nullptr : filter_trans.data(),
        parametric ? 0 : static_cast<int>(filter_wl.size()),
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
        req.psf_quality,
        parametric ? &band : nullptr);

    // ── FREF-BASELINE-001: 绝对合成星等零点 ZP_syn ──────────────────────
    // 目的: 给帧级 SNR 提供一个**跨帧公共**的绝对参考锚（基准口径:
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
            // 颜色项的 F_syn 必须与**拟合通道同口径**（同一响应、同一求积约定）。
            photo_calib::SpectrumIntegratorCache zcache;
            photo_calib::SegmentedResponseCache zseg;
            bool zcache_ok = false;
            if (parametric) {
                zseg = photo_calib::prepare_segmented_response_cache(
                    band, qe_wl.empty() ? nullptr : qe_wl.data(),
                    qe_trans.empty() ? nullptr : qe_trans.data(),
                    static_cast<int>(qe_wl.size()),
                    spectrum_wl.data(), static_cast<int>(spectrum_wl.size()));
                zcache_ok = zseg.valid;
            } else {
                zcache = photo_calib::prepare_filter_cache(
                    filter_wl.data(), filter_trans.data(),
                    static_cast<int>(filter_wl.size()),
                    qe_wl.empty() ? nullptr : qe_wl.data(),
                    qe_trans.empty() ? nullptr : qe_trans.data(),
                    static_cast<int>(qe_wl.size()),
                    spectrum_wl.data(), static_cast<int>(spectrum_wl.size()));
                zcache_ok = !zcache.spectrum_wl.empty();
            }
            if (zcache_ok) {
                std::vector<double> zp_vals;
                zp_vals.reserve(static_cast<size_t>(zp_n));
                zp_rows.clear();
                zp_rows.reserve(static_cast<size_t>(zp_n));
                for (int i = 0; i < zp_n; ++i) {
                    const uint8_t* sp_i =
                        zp_spectra + static_cast<size_t>(i) * static_cast<size_t>(wl_count);
                    const double fsyn = parametric
                        ? photo_calib::compute_f_syn_segmented_xpsd(
                              zseg, sp_i, wl_count,
                              zp_stars[i].flux_min, zp_stars[i].flux_mul)
                        : photo_calib::compute_f_syn_cached_xpsd(
                              zcache, sp_i, wl_count,
                              zp_stars[i].flux_min, zp_stars[i].flux_mul);
                    // 逐星登记（与 records 不同索引域，见上方声明处）：值一律如实登记，
                    // 非有限写 NaN；in_stats 先置 false，仅当该星真正进入 zp_vals 才置 true。
                    ZpStarRow row;
                    row.fsyn = (std::isfinite(fsyn) && fsyn > 0.0) ? fsyn : NAN;
                    row.mag_g = std::isfinite(zp_stars[i].magG) ? zp_stars[i].magG : NAN;
                    row.in_stats = false;
                    zp_rows.push_back(row);
                    if (!(std::isfinite(fsyn) && fsyn > 0.0)) continue;
                    if (!std::isfinite(zp_stars[i].magG)) continue;
                    const double z = zp_stars[i].magG + 2.5 * std::log10(fsyn);
                    if (std::isfinite(z)) {
                        zp_vals.push_back(z);
                        zp_rows.back().in_stats = true;
                    }
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
                    out.synthetic_colour_median_mag = zmed;
                    out.synthetic_colour_n_stars = static_cast<int>(zn);
                    out.synthetic_colour_scatter_mag = 1.482602218505602 * zmad;
                    out.synthetic_colour_valid = true;
                } else {
                    // 样本量不足 ⇒ 颜色项未成立，**没有任何星进入统计样本**：
                    // 逐星标记必须如实清掉，否则会与 synthetic_colour_valid=false 自相矛盾。
                    for (ZpStarRow& row : zp_rows) row.in_stats = false;
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
    // A8/A10：逐星诊断（**非产品面**）——本处只把已填充的逐星记录随结果带出，
    // 不参与任何数值/判据；是否落盘由调用方按调试开关决定（写工作区）。
    out.stars = records;
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
}  // namespace acsd
