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
#include <cstddef>
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
};

inline const char* status_name(LoadStatus s) {
    switch (s) {
        case LoadStatus::kOk: return "ok";
        case LoadStatus::kFileUnreadable: return "file_unreadable";
        case LoadStatus::kCurveNotFound: return "curve_not_found";
        case LoadStatus::kCurveNameOutsideLibrary: return "curve_name_outside_library";
        case LoadStatus::kArraysMissing: return "arrays_missing";
        case LoadStatus::kLengthMismatch: return "length_mismatch";
    }
    return "unknown";
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

// 完整装载: aio 读文件 → 结构化定位 → 抽数组。
inline LoadStatus load_curve(const std::string& json_path, const std::string& curve_name,
                             std::vector<double>* out_wl, std::vector<double>* out_trans) {
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
    return LoadStatus::kOk;
}

}  // namespace curve_json
}  // namespace photometry
}  // namespace astrocs

#endif  // ASTROCS_PHOTOMETRY_FILTER_CURVE_JSON_H
