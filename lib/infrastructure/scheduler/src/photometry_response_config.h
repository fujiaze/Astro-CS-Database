// ============================================================================
// lib/infrastructure/scheduler/src/photometry_response_config.h
// ----------------------------------------------------------------------------
// Issue #106: 测光**响应来源 one-of** 的配置解析（调度层唯一实现）。
//
// 正本: docs/science/photometry/PHOTOMETRY.md §2.1 与 §2.1.1 +
//       docs/engineering/contracts/CONFIG.md「校准、响应与测光」一节。
//
// 语义（逐条, 与上引正本一致）:
//   · `response` 是 tagged one-of:
//       parametric_narrowband: center_nm + fwhm_nm (+ profile + profile_version)
//       measured_curve:        curve_name（曲线库键）
//   · **同一站点**内 `response` 与旧 `filter` 字段并存 ⇒ `RESPONSE_SOURCE_CONFLICT`
//     （不静默择一）。
//   · 未知 mode / 未知 profile / 未知 profile_version / 非正或越界数值
//     ⇒ `INVALID_PARAMETRIC_RESPONSE`（具名, 不进入任何降级合成）。
//   · 跨站点（帧级 override vs 块级）的替换规则在调用方（p1_op_photometry）执行。
//
// 本头**只做解析与校验**, 不消费配置、不产生科学量、不写盘。
// ============================================================================
#ifndef ACSD_SCHEDULER_PHOTOMETRY_RESPONSE_CONFIG_H
#define ACSD_SCHEDULER_PHOTOMETRY_RESPONSE_CONFIG_H

#include "nlohmann/json.hpp"

#include <cmath>
#include <string>

namespace acsd {
namespace scheduler {

// 一个声明站点的响应来源。
struct ResponseDeclaration {
    bool declared = false;   // 该站点是否声明了响应来源
    bool legacy = false;     // true = 旧字段（fit.filter / filter_passband / override.filter）
    std::string mode;        // parametric_narrowband | measured_curve
    double center_nm = 0.0;
    double fwhm_nm = 0.0;
    std::string profile = "top_hat_approx";
    int profile_version = 1;
    std::string curve_name;  // measured 的曲线库键
};

// 唯一受支持的参数化剖面（PHOTOMETRY.md §2.1）。
inline constexpr const char* kNarrowbandProfileName = "top_hat_approx";
inline constexpr int kNarrowbandProfileVersionValue = 1;

// 解析一个声明站点的响应来源。
//   site: 站点 JSON 对象（`photometry.fit` 或某个 `photometry.frame_overrides[]`）
//   out : 解析结果（失败时被重置为默认值）
//   err : 失败时的具名原因（成功时清空）
// 返回 true = 解析成功；false = 失败（*err 非空, 含
// `RESPONSE_SOURCE_CONFLICT` / `INVALID_PARAMETRIC_RESPONSE` 等稳定前缀）。
inline bool parse_response_declaration(const nlohmann::json& site,
                                       ResponseDeclaration* out,
                                       std::string* err) {
    if (out == nullptr) return false;
    *out = ResponseDeclaration();
    if (err != nullptr) err->clear();

    const bool has_resp =
        site.contains("response") && site["response"].is_object();
    const bool has_legacy =
        site.contains("filter") && site["filter"].is_string() &&
        !site["filter"].get<std::string>().empty();
    if (has_resp && has_legacy) {
        if (err != nullptr) {
            *err =
                "RESPONSE_SOURCE_CONFLICT: the same declaration site provides both"
                " the new `response` object and the legacy `filter` field; exactly one"
                " response source must take effect per frame (no silent choice)";
        }
        return false;
    }
    if (!has_resp) {
        if (!has_legacy) return true;   // 该站点未声明任何响应来源
        out->declared = true;
        out->legacy = true;
        out->mode = "measured_curve";
        out->curve_name = site["filter"].get<std::string>();
        return true;
    }

    const nlohmann::json& r = site["response"];
    const std::string mode =
        r.contains("mode") && r["mode"].is_string() ? r["mode"].get<std::string>()
                                                    : std::string();
    if (mode == "parametric_narrowband") {
        if (!r.contains("center_nm") || !r["center_nm"].is_number() ||
            !r.contains("fwhm_nm") || !r["fwhm_nm"].is_number()) {
            if (err != nullptr) {
                *err = "INVALID_PARAMETRIC_RESPONSE: mode=parametric_narrowband"
                       " requires numeric center_nm and fwhm_nm";
            }
            return false;
        }
        if (r.contains("profile_version") && !r["profile_version"].is_number_integer()) {
            if (err != nullptr) {
                *err = "INVALID_PARAMETRIC_RESPONSE: profile_version must be an integer";
            }
            return false;
        }
        out->declared = true;
        out->mode = mode;
        out->center_nm = r["center_nm"].get<double>();
        out->fwhm_nm = r["fwhm_nm"].get<double>();
        out->profile = (r.contains("profile") && r["profile"].is_string())
                           ? r["profile"].get<std::string>()
                           : std::string(kNarrowbandProfileName);
        out->profile_version =
            r.contains("profile_version") ? r["profile_version"].get<int>()
                                           : kNarrowbandProfileVersionValue;
        if (out->profile != kNarrowbandProfileName ||
            out->profile_version != kNarrowbandProfileVersionValue) {
            if (err != nullptr) {
                *err = "INVALID_PARAMETRIC_RESPONSE: unknown profile '" + out->profile +
                       "'/version " + std::to_string(out->profile_version) +
                       " (only " + kNarrowbandProfileName + "/v" +
                       std::to_string(kNarrowbandProfileVersionValue) + " is defined)";
            }
            return false;
        }
        if (!std::isfinite(out->center_nm) || !std::isfinite(out->fwhm_nm) ||
            !(out->fwhm_nm > 0.0) ||
            !(out->center_nm - out->fwhm_nm / 2.0 > 0.0)) {
            if (err != nullptr) {
                *err = "INVALID_PARAMETRIC_RESPONSE: center_nm/fwhm_nm must be finite,"
                       " fwhm_nm > 0 nm and center_nm - fwhm_nm/2 > 0 nm";
            }
            return false;
        }
        return true;
    }
    if (mode == "measured_curve") {
        if (!r.contains("curve_name") || !r["curve_name"].is_string() ||
            r["curve_name"].get<std::string>().empty()) {
            if (err != nullptr) {
                *err = "measured_curve response requires a non-empty curve_name"
                       " (a scanned library key)";
            }
            return false;
        }
        out->declared = true;
        out->mode = mode;
        out->curve_name = r["curve_name"].get<std::string>();
        out->profile.clear();
        out->profile_version = 0;
        return true;
    }
    if (err != nullptr) {
        *err = "unknown response mode '" + mode +
               "' (expected parametric_narrowband or measured_curve)";
    }
    return false;
}

}  // namespace scheduler
}  // namespace acsd

#endif  // ACSD_SCHEDULER_PHOTOMETRY_RESPONSE_CONFIG_H
