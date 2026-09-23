#ifndef ASTROCS_PHOTOMETRY_FILTER_CURVE_JSON_H
#define ASTROCS_PHOTOMETRY_FILTER_CURVE_JSON_H

// ============================================================================
// filter_curve_json.h - 滤镜/QE 响应曲线 JSON 的定位与数组抽取
//                       （**全仓唯一实现**；两个调用方共用同一份代码）
// ----------------------------------------------------------------------------
// 调用方（唯一两份，均只做委托，不再自持解析）:
//   1. lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp  load_curve
//      —— 生产路径: scheduler/module_adapters.cpp → fit_frame_photometry
//   2. lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
//      load_filter_curve / load_qe_curve —— legacy 编排入口 run_stage_photometric
//
// 规范依据（逐条）:
//   - docs/standards/CODE_STANDARD.md §MUST「禁止重复 production science
//     implementation（单一实现 + oracle）」——本头即该条的落点：曲线解析只能有
//     一份实现，两份独立实现是同类缺陷复发的根源。
//   - docs/standards/CODE_STANDARD.md §MUST「禁止 silent config fallback 改变
//     科学语义」——曲线名解析不到时必须报错，不得静默取到别的曲线。
//   - eng/packaging/config/filters.json#lookup.resolution_rule 逐字:
//     「resolve(name) = filters[name] if name in keys(filters) else
//       ERROR(unknown_filter)。比较为字节精确：不折叠大小写、不折叠空白、
//       不做 Unicode 归一、不解析别名」+ docs/contracts/CONFIG_CONTRACT.md §4
//     （同一合同的文档化说明）。本头按该规则**只认曲线库内的对象键**。
//   - ASTROCS_DESIGN.md §9「aio 是文件级唯一 I/O 边界：任何文件读写必须经 aio」
//     —— 读取经 aio_file::read_all（aio 内唯一实现, header-only），本头不自持
//     fopen/ifstream 通道。
//
// 缺陷背景（P1-PHOT-CURVE-RESOLVE / PHOTOCURVE-ORCH-01）:
//   修复前两个调用方各自持有一份**文本搜索**实现：content.find("\"<name>\"") 取
//   该串的**第一次文本出现**，再从该偏移往后找第一个 "wavelength_nm"。当路径指向
//   **转录版** eng/packaging/config/filters.json（顶层键序
//   [filters_schema, library_id, task, authority, transcription, provenance,
//    lookup, filters]，曲线定义在 filters 段）时，名字先在 provenance.per_filter
//   段命中 ⇒ 偏移落在 filters 段**之前** ⇒ 取到 filters 段的**第一个**滤镜
//   "Antlia V Pro Series B"（53 点 / 420–524 nm，蓝端）而不是声明的 "Baader R"
//   （73 点 / 572–716 nm）⇒ F_syn 用错误通带合成（M42 真实数据实测: 49 帧中位
//   2.5σ 由 0.04505 mag 膨胀到 0.42720 mag）。
// 修法: 只接受**同时**满足 (a) 键后紧跟 ':' 与 '{'（是对象键而非任意文本）、
//   (b) 该对象内**含 "wavelength_nm"** 的候选；取第一个满足者。provenance/lookup
//   段的对象不含 wavelength_nm ⇒ 被跳过。名字只在非曲线段出现（如 provenance）
//   ⇒ 返回 kCurveNameOutsideLibrary（**显式报错**，不是静默取第一条曲线）。
//   **不改任何科学公式、常数与容差。**
// ============================================================================

#include "aio_file_io.h"   // aio_file::read_all（aio 唯一 I/O 实现, header-only）

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace astrocs {
namespace photometry {
namespace curve_json {

// 装载结果（调用方据此给出**具名**诊断, 不得把失败折叠成"取到某条曲线"）
enum class LoadStatus {
    kOk = 0,                    // 成功
    kFileUnreadable = 1,        // 文件不存在/不可读/读不完整
    kCurveNotFound = 2,         // 全文无该名字
    kCurveNameOutsideLibrary = 3,  // 名字只作为**非曲线**对象键出现（如 provenance）
    kArraysMissing = 4,         // 曲线对象内缺 wavelength_nm / value 数组
    kLengthMismatch = 5,        // 两数组长度不一致
    // 通带身份核对失败（曲线对象**自述的身份**与声明名 / provenance 声明不符）。
    // 见 identity_mismatch_detail：该状态**必须**带具名细节，调用方不得折叠成
    // "取到某条曲线"。
    kCurveIdentityMismatch = 6,
};

inline const char* status_name(LoadStatus s) {
    switch (s) {
        case LoadStatus::kOk: return "ok";
        case LoadStatus::kFileUnreadable: return "file_unreadable";
        case LoadStatus::kCurveNotFound: return "curve_not_found";
        case LoadStatus::kCurveNameOutsideLibrary: return "curve_name_outside_library";
        case LoadStatus::kArraysMissing: return "arrays_missing";
        case LoadStatus::kLengthMismatch: return "length_mismatch";
        case LoadStatus::kCurveIdentityMismatch: return "curve_identity_mismatch";
    }
    return "unknown";
}

// ── 通带身份核对的具名细节（load_curve 失败时由出参带回）────────────────────
// 本结构只承载**诊断**：身份判据本身的定义在 check_curve_identity()，落盘/报错
// 由调用方决定；本头不写盘、不打印。
struct IdentityMismatch {
    std::string requested;      // 请求的曲线名（= filters.json 的对象键 / 库键）
    std::string object_name;    // 曲线对象 name 字段的自述值（缺字段时为空）
    bool object_name_present = false;
    std::string reason;         // 机器可读原因码（见 check_curve_identity 的 reason）
    std::string detail;         // 人可读细节（含具体数值）
};

// 曲线对象身份核对结果（供**装配检查**与**判据**共用；本函数是唯一实现）。
struct CurveIdentity {
    bool ok = false;              // 全部核对项通过
    IdentityMismatch mismatch;    // ok=false 时的具名细节
};

// 从曲线对象文本里取字符串字段（如 "name": "Baader R"）。未找到 ⇒ false。
inline bool object_string_field(const std::string& obj, const std::string& field,
                                std::string* out) {
    const std::string key = "\"" + field + "\"";
    std::size_t p = obj.find(key);
    if (p == std::string::npos) return false;
    p += key.size();
    while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' ||
                              obj[p] == '\r')) ++p;
    if (p >= obj.size() || obj[p] != ':') return false;
    ++p;
    while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' ||
                              obj[p] == '\r')) ++p;
    if (p >= obj.size() || obj[p] != '"') return false;
    const std::size_t b = p + 1;
    std::size_t e = b;
    while (e < obj.size()) {
        if (obj[e] == '\\') { e += 2; continue; }   // 跳过转义对
        if (obj[e] == '"') break;
        ++e;
    }
    if (e >= obj.size()) return false;
    if (out) *out = obj.substr(b, e - b);
    return true;
}

// 从曲线对象文本里取数值字段（如 "n_points": 73）。未找到/不可解析 ⇒ false。
inline bool object_number_field(const std::string& obj, const std::string& field,
                                double* out) {
    const std::string key = "\"" + field + "\"";
    std::size_t p = obj.find(key);
    if (p == std::string::npos) return false;
    p += key.size();
    while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' ||
                              obj[p] == '\r')) ++p;
    if (p >= obj.size() || obj[p] != ':') return false;
    ++p;
    while (p < obj.size() && (obj[p] == ' ' || obj[p] == '\t' || obj[p] == '\n' ||
                              obj[p] == '\r')) ++p;
    const std::size_t b = p;
    while (p < obj.size() && (std::isdigit(static_cast<unsigned char>(obj[p])) ||
                              obj[p] == '-' || obj[p] == '+' || obj[p] == '.' ||
                              obj[p] == 'e' || obj[p] == 'E')) ++p;
    if (p == b) return false;
    try {
        const double v = std::stod(obj.substr(b, p - b));
        if (out) *out = v;
        return true;
    } catch (...) {
        return false;
    }
}

// 定位「键后紧跟 ':' 与 '{'」的**任意**对象（不做曲线判据）。用于取
// provenance.per_filter[<name>] 这类非曲线对象。
inline bool find_object_by_key(const std::string& content, const std::string& key_name,
                               std::string* out_obj) {
    if (out_obj) out_obj->clear();
    auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    };
    const std::string key = "\"" + key_name + "\"";
    for (std::size_t p = content.find(key); p != std::string::npos;
         p = content.find(key, p + 1)) {
        std::size_t q = p + key.size();
        while (q < content.size() && is_space(content[q])) ++q;
        if (q >= content.size() || content[q] != ':') continue;
        ++q;
        while (q < content.size() && is_space(content[q])) ++q;
        if (q >= content.size() || content[q] != '{') continue;
        std::size_t depth = 0, end = std::string::npos;
        for (std::size_t i = q; i < content.size(); ++i) {
            if (content[i] == '{') ++depth;
            else if (content[i] == '}') { if (--depth == 0) { end = i; break; } }
        }
        if (end == std::string::npos) continue;
        if (out_obj) *out_obj = content.substr(q, end - q + 1);
        return true;
    }
    return false;
}

// 相对比较（provenance 的 curve_stats 由转录脚本按同一数组算出 ⇒ 正常应逐位相等；
// 容差只吸收 JSON 十进制往返的末位差异，不是"允许曲线不同"的余地）。
inline bool num_equal(double a, double b) {
    if (!std::isfinite(a) || !std::isfinite(b)) return false;
    const double scale = std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
    return std::fabs(a - b) <= 1e-9 * scale;
}

// ── 通带身份核对（**装配检查**，不改科学口径）──────────────────────────────
// 判据（逐条）：
//   (1) 曲线对象**自述名** == 请求名（库键）。对象缺 name 字段 ⇒ 判红（转录字段
//       集含 name，缺字段即不是本库的曲线记录）。
//   (2) 数组非空且两数组等长；对象内 n_points（若有）== 数组长度。
//   (3) 该名字在 provenance.per_filter 中有条目时：其 curve_stats 的
//       n_points / wl_min / wl_max / val_min / val_max 必须与**实际曲线数组**
//       一致（provenance 是本库对"这个名字对应哪条曲线"的显式声明；不一致 ⇒
//       声明与数据矛盾，fail-closed）。
// 判据 (1) 正是「按文本位置取错通带」的判别式：误取到的对象自述名是别的滤镜，
// 与请求名不等 ⇒ 具名判红，而不是静默用错通带合成 F_syn。
inline CurveIdentity check_curve_identity(const std::string& content,
                                          const std::string& requested_name,
                                          const std::string& curve_obj,
                                          const std::vector<double>& wl,
                                          const std::vector<double>& trans) {
    CurveIdentity id;
    id.mismatch.requested = requested_name;

    if (wl.empty() || trans.empty() || wl.size() != trans.size()) {
        id.mismatch.reason = "arrays_empty_or_length_mismatch";
        id.mismatch.detail = "n_wl=" + std::to_string(wl.size()) +
                             " n_value=" + std::to_string(trans.size());
        return id;
    }
    double n_points_field = 0.0;
    const bool has_n_points = object_number_field(curve_obj, "n_points", &n_points_field);
    if (has_n_points && n_points_field != static_cast<double>(wl.size())) {
        id.mismatch.reason = "n_points_field_mismatch";
        id.mismatch.detail = "n_points=" + std::to_string(static_cast<long long>(n_points_field)) +
                             " array_len=" + std::to_string(wl.size());
        return id;
    }
    std::string obj_name;
    const bool has_name = object_string_field(curve_obj, "name", &obj_name);
    id.mismatch.object_name_present = has_name;
    id.mismatch.object_name = has_name ? obj_name : std::string();
    if (!has_name) {
        id.mismatch.reason = "curve_object_name_field_missing";
        id.mismatch.detail = "曲线对象无 name 字段（转录字段集 = name/channel/"
                             "wavelength_nm/value/n_points）";
        return id;
    }
    if (obj_name != requested_name) {
        id.mismatch.reason = "curve_object_name_differs_from_requested";
        id.mismatch.detail = "曲线对象自述 name='" + obj_name + "' 与请求名 '" +
                             requested_name + "' 不等（按文本位置取错通带的判别式）";
        return id;
    }
    // (3) provenance 声明对账（provenance 段不存在 ⇒ 该文件不携带身份声明，跳过）。
    std::string prov_root;
    if (find_object_by_key(content, "per_filter", &prov_root)) {
        std::string entry;
        if (!find_object_by_key(prov_root, requested_name, &entry)) {
            id.mismatch.reason = "provenance_entry_missing";
            id.mismatch.detail = "provenance.per_filter 无 '" + requested_name +
                                 "' 条目（库内曲线必须在 provenance 中如实登记）";
            return id;
        }
        std::string stats;
        if (find_object_by_key(entry, "curve_stats", &stats)) {
            double s_npts = 0.0, s_lo = 0.0, s_hi = 0.0, s_vlo = 0.0, s_vhi = 0.0;
            double s_wlsum = 0.0, s_vsum = 0.0, s_vsumsq = 0.0;
            const bool ok_n = object_number_field(stats, "n_points", &s_npts);
            const bool ok_lo = object_number_field(stats, "wl_min", &s_lo);
            const bool ok_hi = object_number_field(stats, "wl_max", &s_hi);
            const bool ok_vlo = object_number_field(stats, "val_min", &s_vlo);
            const bool ok_vhi = object_number_field(stats, "val_max", &s_vhi);
            // 派生指纹（逐元素和）: min/max 只是包络, 两支不同曲线可以共用同一包络
            // （例如把 value 数组整段换成另一支滤镜的值域相同的曲线）⇒ 身份判据必须
            // 有逐元素量。指纹字段**必填**（缺 ⇒ fail-closed, 不是"跳过核对"）。
            const bool ok_wlsum = object_number_field(stats, "wl_sum", &s_wlsum);
            const bool ok_vsum = object_number_field(stats, "val_sum", &s_vsum);
            const bool ok_vsumsq = object_number_field(stats, "val_sumsq", &s_vsumsq);
            if (!ok_n || !ok_lo || !ok_hi || !ok_vlo || !ok_vhi) {
                id.mismatch.reason = "provenance_curve_stats_unparsable";
                id.mismatch.detail = "provenance.curve_stats 字段缺失或不可解析"
                                     "（n_points/wl_min/wl_max/val_min/val_max）";
                return id;
            }
            if (!ok_wlsum || !ok_vsum || !ok_vsumsq) {
                id.mismatch.reason = "provenance_fingerprint_missing";
                id.mismatch.detail = "provenance.curve_stats 缺派生指纹字段"
                                     "（wl_sum/val_sum/val_sumsq）—— 身份判据要求"
                                     "逐元素量, 包络（min/max）不足以定身份";
                return id;
            }
            const double lo = *std::min_element(wl.begin(), wl.end());
            const double hi = *std::max_element(wl.begin(), wl.end());
            const double vlo = *std::min_element(trans.begin(), trans.end());
            const double vhi = *std::max_element(trans.begin(), trans.end());
            char buf[256];
            if (s_npts != static_cast<double>(wl.size())) {
                std::snprintf(buf, sizeof(buf),
                              "provenance.n_points=%.0f vs 实际曲线点数 %zu", s_npts, wl.size());
                id.mismatch.reason = "provenance_curve_stats_mismatch";
                id.mismatch.detail = buf;
                return id;
            }
            if (!num_equal(s_lo, lo) || !num_equal(s_hi, hi)) {
                std::snprintf(buf, sizeof(buf),
                              "provenance 波长域 [%.4f,%.4f] vs 实际 [%.4f,%.4f] nm",
                              s_lo, s_hi, lo, hi);
                id.mismatch.reason = "provenance_curve_stats_mismatch";
                id.mismatch.detail = buf;
                return id;
            }
            if (!num_equal(s_vlo, vlo) || !num_equal(s_vhi, vhi)) {
                std::snprintf(buf, sizeof(buf),
                              "provenance 透过率域 [%.6f,%.6f] vs 实际 [%.6f,%.6f]",
                              s_vlo, s_vhi, vlo, vhi);
                id.mismatch.reason = "provenance_curve_stats_mismatch";
                id.mismatch.detail = buf;
                return id;
            }
            // 逐元素指纹对账（波长轴 + 透过率 + 透过率平方和）。
            double wl_sum = 0.0, v_sum = 0.0, v_sumsq = 0.0;
            for (std::size_t k = 0; k < wl.size(); ++k) {
                wl_sum += wl[k];
                v_sum += trans[k];
                v_sumsq += trans[k] * trans[k];
            }
            if (!num_equal(s_wlsum, wl_sum)) {
                std::snprintf(buf, sizeof(buf),
                              "provenance wl_sum=%.6f vs 实际 %.6f（波长轴与声明不符）",
                              s_wlsum, wl_sum);
                id.mismatch.reason = "provenance_curve_stats_mismatch";
                id.mismatch.detail = buf;
                return id;
            }
            if (!num_equal(s_vsum, v_sum)) {
                std::snprintf(buf, sizeof(buf),
                              "provenance val_sum=%.9f vs 实际 %.9f（透过率曲线与声明不符）",
                              s_vsum, v_sum);
                id.mismatch.reason = "provenance_curve_stats_mismatch";
                id.mismatch.detail = buf;
                return id;
            }
            if (!num_equal(s_vsumsq, v_sumsq)) {
                std::snprintf(buf, sizeof(buf),
                              "provenance val_sumsq=%.9f vs 实际 %.9f（透过率曲线与声明不符）",
                              s_vsumsq, v_sumsq);
                id.mismatch.reason = "provenance_curve_stats_mismatch";
                id.mismatch.detail = buf;
                return id;
            }
        } else {
            id.mismatch.reason = "provenance_curve_stats_missing";
            id.mismatch.detail = "provenance.per_filter['" + requested_name +
                                 "'] 无 curve_stats 对象";
            return id;
        }
    }
    id.ok = true;
    return id;
}

// FITS FILTER 关键字 → 滤镜库键（**单一实现**）。
// 修复前 orchestrator.cpp:1342 与 frame_photometry_fit.cpp:45 各持一份逐字相同的
// 表；两份表一旦漂移，同一 FILTER 会在两条路径上解析成不同曲线 ⇒ 合并到本处。
inline std::string map_filter_name(const std::string& fits_filter) {
    // 大小写不敏感比较的辅助
    auto ieq = [](const std::string& a, const char* b) {
        return std::equal(a.begin(), a.end(), b, b + std::strlen(b),
            [](char c1, char c2) { return std::tolower(c1) == std::tolower(c2); });
    };
    if (ieq(fits_filter, "Red") || ieq(fits_filter, "R")) return "Baader R";
    if (ieq(fits_filter, "Green") || ieq(fits_filter, "G")) return "Baader G";
    if (ieq(fits_filter, "Blue") || ieq(fits_filter, "B")) return "Baader B";
    if (ieq(fits_filter, "Lum") || ieq(fits_filter, "L") || ieq(fits_filter, "Luminance"))
        return "Baader UV/IR Cut / L CMOS Optimized";
    // 窄带滤光片映射 (H-alpha / OIII 大小写变体)
    // T4: Baader RGBHaOIII (7nm HA, 8.5nm OIII); T2/T3: Astrodon (暂用 Baader 曲线近似)
    if (ieq(fits_filter, "H-alpha") || ieq(fits_filter, "Ha") || ieq(fits_filter, "HA"))
        return "Baader 7nm H-alpha";
    if (ieq(fits_filter, "OIII") || ieq(fits_filter, "Oiii"))
        return "Baader 8.5nm OIII";
    // 未匹配时原样返回 (可能本身就是 filters.json 中的名称)
    return fits_filter;
}

// 在**已读入的 JSON 文本**中定位曲线对象。返回 false 表示未找到曲线对象；
// *out_saw_key_object = 是否至少见过一个「键后紧跟 ':' 与 '{'」的候选（即该名字
// 确实是某个对象段的键，只是那个对象不含 wavelength_nm ⇒ 名字在库外/非曲线段）。
inline bool find_curve_object(const std::string& content, const std::string& curve_name,
                              std::string* out_obj, bool* out_saw_key_object = nullptr) {
    if (out_obj) out_obj->clear();
    if (out_saw_key_object) *out_saw_key_object = false;
    auto is_space = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    };
    const std::string key = "\"" + curve_name + "\"";
    for (std::size_t p = content.find(key); p != std::string::npos;
         p = content.find(key, p + 1)) {
        std::size_t q = p + key.size();
        while (q < content.size() && is_space(content[q])) ++q;
        if (q >= content.size() || content[q] != ':') continue;   // 非对象键 (别名表/字符串值)
        ++q;
        while (q < content.size() && is_space(content[q])) ++q;
        if (q >= content.size() || content[q] != '{') continue;   // 非对象值
        if (out_saw_key_object) *out_saw_key_object = true;
        // 括号配平取该对象的完整文本
        std::size_t depth = 0, end = std::string::npos;
        for (std::size_t i = q; i < content.size(); ++i) {
            if (content[i] == '{') ++depth;
            else if (content[i] == '}') { if (--depth == 0) { end = i; break; } }
        }
        if (end == std::string::npos) continue;
        std::string cand = content.substr(q, end - q + 1);
        if (cand.find("\"wavelength_nm\"") == std::string::npos) continue;  // 曲线对象判据
        if (out_obj) *out_obj = std::move(cand);
        return true;
    }
    return false;
}

// 抽取曲线对象内的 wavelength_nm / value 数组（解析口径与修复前逐位一致:
// 去逗号后按空白切分浮点）。
inline bool extract_curve_arrays(const std::string& obj, std::vector<double>* out_wl,
                                 std::vector<double>* out_trans) {
    std::size_t pos = 0;
    auto extract_array = [&obj, &pos](const std::string& arr_key,
                                      std::vector<double>* out) -> bool {
        std::size_t kpos = obj.find(arr_key, pos);
        if (kpos == std::string::npos) return false;
        std::size_t b0 = obj.find('[', kpos);
        if (b0 == std::string::npos) return false;
        std::size_t b1 = obj.find(']', b0);
        if (b1 == std::string::npos) return false;
        std::string arr = obj.substr(b0 + 1, b1 - b0 - 1);
        std::replace(arr.begin(), arr.end(), ',', ' ');
        std::istringstream iss(arr);
        if (out) out->clear();
        double v = 0.0;
        while (iss >> v) { if (out) out->push_back(v); }
        return out != nullptr && !out->empty();
    };
    if (!extract_array("\"wavelength_nm\"", out_wl) ||
        !extract_array("\"value\"", out_trans)) {
        return false;
    }
    return out_wl->size() == out_trans->size();
}

// 完整装载: aio 读文件 → 结构化定位 → 抽数组 → **通带身份核对**。
// *out_mismatch（可空）= 身份核对失败时的具名细节；调用方据此给出可定位判词。
// 失败时**不**留下半份曲线：out_wl/out_trans 被清空（禁止"取到一条曲线"的静默
// 降级，CODE_STANDARD §MUST「禁止 silent config fallback 改变科学语义」）。
inline LoadStatus load_curve(const std::string& json_path, const std::string& curve_name,
                             std::vector<double>* out_wl, std::vector<double>* out_trans,
                             IdentityMismatch* out_mismatch = nullptr) {
    if (out_mismatch) *out_mismatch = IdentityMismatch();
    std::string content;
    if (!aio_file::read_all(json_path.c_str(), &content)) {
        return LoadStatus::kFileUnreadable;
    }
    std::string obj;
    bool saw_key_object = false;
    if (!find_curve_object(content, curve_name, &obj, &saw_key_object)) {
        return saw_key_object ? LoadStatus::kCurveNameOutsideLibrary
                              : LoadStatus::kCurveNotFound;
    }
    if (!extract_curve_arrays(obj, out_wl, out_trans)) {
        return LoadStatus::kArraysMissing;
    }
    // 通带身份门（装配期 fail-closed）: 曲线对象自述的身份必须与请求名及
    // provenance 的显式声明一致；不一致 ⇒ 具名拒绝, 绝不返回曲线。
    const CurveIdentity id = check_curve_identity(content, curve_name, obj,
                                                  out_wl ? *out_wl : std::vector<double>(),
                                                  out_trans ? *out_trans : std::vector<double>());
    if (!id.ok) {
        if (out_mismatch) *out_mismatch = id.mismatch;
        if (out_wl) out_wl->clear();
        if (out_trans) out_trans->clear();
        return LoadStatus::kCurveIdentityMismatch;
    }
    return LoadStatus::kOk;
}

}  // namespace curve_json
}  // namespace photometry
}  // namespace astrocs

#endif  // ASTROCS_PHOTOMETRY_FILTER_CURVE_JSON_H
