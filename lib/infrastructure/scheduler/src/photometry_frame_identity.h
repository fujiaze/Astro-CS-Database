// ============================================================================
// lib/infrastructure/scheduler/src/photometry_frame_identity.h
// ----------------------------------------------------------------------------
// Issue #106 独立源码审核 P1（跨帧响应身份被首帧组级值污染）的**修复载体**:
// **逐帧**响应身份的落盘格式与解析规则（调度层唯一实现）。
//
// 问题（审核原文要点）:
//   · 光度阶段 `p1_phot.json#photscale_fit[frame].response.response_id` 已是
//     **逐帧**正确值; 但 Drizzle 阶段读的是**块级**
//     `passband_identity.response.response_id`（首个拟合成功帧冻结）⇒ 同一
//     block 内两帧不同响应时, 两张 `frame_science` 会拿到**同一**响应 id。
//   · 差异诊断 `n_passband_identity_mismatch` 只比较 filter_key/n/range ⇒
//     参数化窄带（filter_key 空、数量/范围全 0）**检不出**混响应。
//
// 本头的合同（逐条）:
//   1. **逐帧权威身份** = `photscale_fit[frame_key].response` 块（由光度节点
//      逐帧写入; 序列化与解析都在本头, 生产与单测共用同一实现）。
//   2. **组级** `passband_identity` 只作**摘要**与**旧产物兼容**; 不得作为
//      混响应 block 的逐帧真相。
//   3. 逐帧权威身份**缺失**时: 具名不可用（`unavailable`）, **不得**用别的帧
//      （例如首帧）的值静默代入。
//   4. 旧产物（整份 p1_phot.json **没有**任何逐帧响应记录）⇒ 允许**一次**
//      组级回退, 但必须标记 `group_legacy`（可机检, 不静默）。
//
// 身份口径（与 acsd::phase1::precision::photo_response_identity /
// photo_application_identity 一致）:
//   · 滤镜/QE 的内容身份 = **有序样本内容 sha256**（#42/#104 口径）; 曲线
//     **文件**字节摘要另存为 `*_file_sha256`（provenance, 不进身份串）。
//   · 物理响应 id = `response:v1:…`; 合成模型 id = `synthesis:v1:…`
//     （含 XP 覆盖/求积/XP 网格; 标定 provenance, 与响应身份分列）。
//
// 本头**只做序列化与解析**, 不做科学计算、不写盘、不做 I/O。
// ============================================================================
#ifndef ACSD_SCHEDULER_PHOTOMETRY_FRAME_IDENTITY_H
#define ACSD_SCHEDULER_PHOTOMETRY_FRAME_IDENTITY_H

#include "nlohmann/json.hpp"
#include "frame_photometry_fit.h"   // acsd::photometry::ResponseIdentity（拟合入口产出）

#include <map>
#include <string>

namespace acsd {
namespace scheduler {

using photometry_frame_identity_json_t = nlohmann::json;

// ── 逐帧响应身份（p1_phot.json#photscale_fit[frame].response 的权威内容）────
struct FrameResponseIdentity {
    // 物理响应身份（形状 + 滤镜/QE 内容）
    std::string response_id;            // response:v1:<sha32>
    std::string mode;                   // parametric_narrowband | measured_curve
    std::string profile;                // parametric: top_hat_approx
    int profile_version = 0;
    double center_nm = 0.0;
    double fwhm_nm = 0.0;
    double band_lo_nm = 0.0;
    double band_hi_nm = 0.0;
    std::string filter_key;             // measured: 曲线库键（参数化路径为空）
    std::string filter_content_sha256;  // 滤镜**有序样本内容** sha256
    std::string filter_file_sha256;     // 滤镜**文件字节** sha256（provenance）
    std::string filter_curve_source;    // 实际装载的滤镜曲线文件
    std::string qe_key;                 // QE 曲线库键
    std::string qe_content_sha256;      // QE **有序样本内容** sha256
    std::string qe_file_sha256;         // QE **文件字节** sha256（provenance）
    std::string qe_curve_source;        // 实际装载的 QE 曲线文件
    // 合成模型身份（标定 provenance）
    std::string synthesis_id;           // synthesis:v1:<sha32>
    std::string integration;            // segmented_gauss_legendre4_v1 | simpson_xp_grid_v1
    std::string coverage;               // full | xp_truncated | qe_truncated | no_overlap
    std::string knowledge;              // parametric_approx | measured
    std::string source;                 // explicit_frame_override | block_inherit
    int xp_wl_start = 0;
    int xp_wl_step = 0;
    int xp_wl_count = 0;
};

// ── 组级摘要（旧产物兼容 / 摘要用; **不是**逐帧真相）─────────────────────────
struct GroupResponseIdentity {
    bool present = false;               // 组级块是否存在
    std::string response_id;
    std::string synthesis_id;
    std::string filter_key, filter_content_sha256, filter_file_sha256;
    std::string qe_key, qe_content_sha256, qe_file_sha256;
};

// ── 逐帧解析结果（来源可机检）───────────────────────────────────────────────
struct FrameResponseResolution {
    bool available = false;
    // per_frame      = 逐帧权威记录（正常路径）
    // group_legacy   = 旧产物（整份文件无任何逐帧响应记录）的一次组级回退
    // unavailable    = 具名不可用（**未**用别的帧的值代入）
    std::string source;
    FrameResponseIdentity identity;
    std::string reason;                 // 非 per_frame 时的具名说明
};

// ── 拟合入口产出 → 逐帧响应身份（**唯一映射**; 生产与单测共用）──────────────
// 光度节点与单测都用本函数构造逐帧身份 ⇒ 落盘格式与解析规则只有一处定义。
// 曲线**文件字节**摘要不在此计算（调用方按曲线文件路径另算, 只作 provenance）。
inline FrameResponseIdentity frame_response_identity_of(
    const acsd::photometry::ResponseIdentity& rid,
    const std::string& filter_curve_source,
    const std::string& qe_curve_source) {
    FrameResponseIdentity id;
    id.response_id = rid.response_id;
    id.synthesis_id = rid.synthesis_id;
    id.mode = rid.mode;
    id.profile = rid.profile;
    id.profile_version = rid.profile_version;
    id.center_nm = rid.center_nm;
    id.fwhm_nm = rid.fwhm_nm;
    id.band_lo_nm = rid.band_lo_nm;
    id.band_hi_nm = rid.band_hi_nm;
    id.integration = rid.integration;
    id.coverage = rid.coverage;
    id.knowledge = rid.knowledge;
    id.source = rid.source;
    id.qe_key = rid.qe_key;
    id.qe_content_sha256 = rid.qe_content_sha256;
    id.qe_curve_source = qe_curve_source;
    id.filter_key = rid.filter_key;
    id.filter_content_sha256 = rid.filter_content_sha256;
    id.filter_curve_source = filter_curve_source;
    id.xp_wl_start = rid.xp_wl_start;
    id.xp_wl_step = rid.xp_wl_step;
    id.xp_wl_count = rid.xp_wl_count;
    return id;
}

// ── 序列化（光度节点 → p1_phot.json）────────────────────────────────────────
// 逐帧块; 字段名 = 合同（photscale_fit[frame].response）。
inline photometry_frame_identity_json_t frame_response_identity_json(
    const FrameResponseIdentity& id) {
    photometry_frame_identity_json_t j;
    j["mode"] = id.mode;
    j["profile"] = id.profile;
    j["profile_version"] = id.profile_version;
    j["center_nm"] = id.center_nm;
    j["fwhm_nm"] = id.fwhm_nm;
    j["band_lo_nm"] = id.band_lo_nm;
    j["band_hi_nm"] = id.band_hi_nm;
    j["integration"] = id.integration;
    j["coverage"] = id.coverage;
    j["knowledge"] = id.knowledge;
    j["response_id"] = id.response_id;
    j["synthesis_id"] = id.synthesis_id;
    j["source"] = id.source;
    j["qe_key"] = id.qe_key;
    j["qe_content_sha256"] = id.qe_content_sha256;
    j["qe_file_sha256"] = id.qe_file_sha256;
    j["qe_curve_source"] = id.qe_curve_source;
    j["filter_key"] = id.filter_key;
    j["filter_content_sha256"] = id.filter_content_sha256;
    j["filter_file_sha256"] = id.filter_file_sha256;
    j["filter_curve_source"] = id.filter_curve_source;
    j["xp_wl_start"] = id.xp_wl_start;
    j["xp_wl_step"] = id.xp_wl_step;
    j["xp_wl_count"] = id.xp_wl_count;
    return j;
}

// ── 解析（p1_phot.json → 逐帧身份）──────────────────────────────────────────
// 解析一个 `photscale_fit[frame].response` 块; 返回 false = 块缺 response_id
// （⇒ 不是可用的逐帧权威身份）。
inline bool parse_frame_response_identity(const photometry_frame_identity_json_t& block,
                                          FrameResponseIdentity* out) {
    if (out == nullptr) return false;
    *out = FrameResponseIdentity();
    if (!block.is_object()) return false;
    out->response_id = block.value("response_id", std::string());
    if (out->response_id.empty()) return false;   // 无 id ⇒ 无权威身份
    out->synthesis_id = block.value("synthesis_id", std::string());
    out->mode = block.value("mode", std::string());
    out->profile = block.value("profile", std::string());
    out->profile_version = block.value("profile_version", 0);
    out->center_nm = block.value("center_nm", 0.0);
    out->fwhm_nm = block.value("fwhm_nm", 0.0);
    out->band_lo_nm = block.value("band_lo_nm", 0.0);
    out->band_hi_nm = block.value("band_hi_nm", 0.0);
    out->integration = block.value("integration", std::string());
    out->coverage = block.value("coverage", std::string());
    out->knowledge = block.value("knowledge", std::string());
    out->source = block.value("source", std::string());
    out->qe_key = block.value("qe_key", std::string());
    out->qe_content_sha256 = block.value("qe_content_sha256", std::string());
    out->qe_file_sha256 = block.value("qe_file_sha256", std::string());
    out->qe_curve_source = block.value("qe_curve_source", std::string());
    out->filter_key = block.value("filter_key", std::string());
    out->filter_content_sha256 = block.value("filter_content_sha256", std::string());
    out->filter_file_sha256 = block.value("filter_file_sha256", std::string());
    out->filter_curve_source = block.value("filter_curve_source", std::string());
    out->xp_wl_start = block.value("xp_wl_start", 0);
    out->xp_wl_step = block.value("xp_wl_step", 0);
    out->xp_wl_count = block.value("xp_wl_count", 0);
    return true;
}

// ── 收集整份 p1_phot.json 的逐帧响应身份表 ──────────────────────────────────
// 只收**有 response_id** 的帧（无 id 的帧 = 无权威身份, 留给解析规则处理）。
inline std::map<std::string, FrameResponseIdentity> collect_frame_response_identities(
    const photometry_frame_identity_json_t& p1_phot) {
    std::map<std::string, FrameResponseIdentity> out;
    if (!p1_phot.is_object()) return out;
    if (!p1_phot.contains("photscale_fit") || !p1_phot["photscale_fit"].is_object())
        return out;
    const auto& fit = p1_phot["photscale_fit"];
    for (auto it = fit.begin(); it != fit.end(); ++it) {
        if (!it.value().is_object()) continue;
        if (!it.value().contains("response")) continue;
        FrameResponseIdentity id;
        if (!parse_frame_response_identity(it.value()["response"], &id)) continue;
        out[it.key()] = std::move(id);
    }
    return out;
}

// ── 解析组级摘要（passband_identity）────────────────────────────────────────
// 组级块里的**有序内容**摘要是逐帧块同源字段; 文件字节摘要由调用方按曲线
// 文件路径另算（头内不做 I/O）。
inline GroupResponseIdentity parse_group_response_identity(
    const photometry_frame_identity_json_t& p1_phot) {
    GroupResponseIdentity g;
    if (!p1_phot.is_object()) return g;
    if (!p1_phot.contains("passband_identity") ||
        !p1_phot["passband_identity"].is_object())
        return g;
    const auto& pbi = p1_phot["passband_identity"];
    g.present = true;
    g.filter_key = pbi.value("resolved_library_key", std::string());
    if (pbi.contains("qe") && pbi["qe"].is_object()) {
        g.qe_key = pbi["qe"].value("resolved_library_key", std::string());
        g.qe_content_sha256 = pbi["qe"].value("content_sha256", std::string());
    }
    if (pbi.contains("response") && pbi["response"].is_object()) {
        const auto& r = pbi["response"];
        g.response_id = r.value("response_id", std::string());
        g.synthesis_id = r.value("synthesis_id", std::string());
        g.filter_content_sha256 = r.value("filter_content_sha256", std::string());
        g.qe_content_sha256 = g.qe_content_sha256.empty()
                                  ? r.value("qe_content_sha256", std::string())
                                  : g.qe_content_sha256;
    }
    return g;
}

// ── 逐帧身份解析规则（**唯一实现**; 生产与单测共用）────────────────────────
//  ① 逐帧权威记录存在（response_id 非空）⇒ available, source=per_frame。
//  ② 整份产物**没有任何**逐帧响应记录（旧产物）且组级有 id ⇒ available,
//     source=group_legacy（具名回退, 不静默）。
//  ③ 其余 ⇒ unavailable（具名原因; **不**用别的帧的值代入）。
inline FrameResponseResolution resolve_frame_response_identity(
    const std::map<std::string, FrameResponseIdentity>& per_frame,
    const GroupResponseIdentity& group,
    const std::string& frame_key) {
    FrameResponseResolution res;
    const auto it = per_frame.find(frame_key);
    if (it != per_frame.end() && !it->second.response_id.empty()) {
        res.available = true;
        res.source = "per_frame";
        res.identity = it->second;
        return res;
    }
    if (per_frame.empty() && group.present && !group.response_id.empty()) {
        res.available = true;
        res.source = "group_legacy";
        res.reason = "legacy_artifact_without_per_frame_response_identity"
                     " (group passband_identity used; per-frame truth unavailable)";
        res.identity.response_id = group.response_id;
        res.identity.synthesis_id = group.synthesis_id;
        res.identity.filter_key = group.filter_key;
        res.identity.filter_content_sha256 = group.filter_content_sha256;
        res.identity.filter_file_sha256 = group.filter_file_sha256;
        res.identity.qe_key = group.qe_key;
        res.identity.qe_content_sha256 = group.qe_content_sha256;
        res.identity.qe_file_sha256 = group.qe_file_sha256;
        return res;
    }
    res.available = false;
    res.source = "unavailable";
    res.reason = "response_identity_unavailable (no per-frame record in"
                 " p1_phot.json#photscale_fit for this frame; not substituted"
                 " from another frame)";
    return res;
}

}  // namespace scheduler
}  // namespace acsd

#endif  // ACSD_SCHEDULER_PHOTOMETRY_FRAME_IDENTITY_H
