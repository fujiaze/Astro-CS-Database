#ifndef ACSD_PHOTOMETRY_FILTER_CURVE_JSON_H
#define ACSD_PHOTOMETRY_FILTER_CURVE_JSON_H

// ============================================================================
// filter_curve_json.h - 响应曲线库的**运行期扫描与解析**（全仓唯一实现）
// ----------------------------------------------------------------------------
// 曲线库 = **目录 + 运行期扫描**：代码内只固定一个相对路径
//   config/response_curves/            （相对**可执行文件所在目录**）
// 不设环境变量覆盖、不设 CLI 覆盖、不设优先级链。运行期扫描该目录得到可用曲线
// 集合，再按库键**字节精确**解析（无归一、无别名）。
//
// 调用方（唯一两份，均只做委托，不自持解析）:
//   1. lib/algorithms/photometry/cpp/src/frame_photometry_fit.cpp
//      —— 生产路径: scheduler/module_adapters.cpp → fit_frame_photometry
//   2. lib/infrastructure/pipeline/orchestrator/cpp/src/orchestrator.cpp
//      —— legacy 编排入口（其曲线选择已退役，见该处退役注释块）
//
// 规范依据（逐条）:
//   - docs/engineering/contracts/CONFIG.md「响应曲线库（代码内单一路径 + 运行时扫描）」
//     一节（**唯一口径来源**）：单一路径 / 扫描规则（非递归、`*.json`、按文件名
//     字节序稳定排序、不跟随指向目录外的符号链接、文件数与文件大小硬上限）/
//     名字唯一性（重名 ⇒ `curve_name_duplicate`）/ 匹配语义（字节精确、无别名）/
//     未知滤镜 ⇒ `unknown_filter`（含候选与扫描总数）/ 目录不可达或空 ⇒ fail-closed /
//     曲线身份核对四项（`name` 逐字节、`n_points` == 数组长度、`curve_stats` 的
//     min/max、以及逐元素指纹 `wl_sum`/`val_sum`/`val_sumsq`）。
//   - docs/engineering/standards/CODE.md「MUST」一节「禁止重复 production science
//     implementation（单一实现 + oracle）」——曲线解析只能有一份实现。
//   - docs/engineering/standards/CODE.md「MUST」一节「禁止 silent config fallback 改变
//     科学语义」——解析不到必须报错，不得静默取到别的曲线、不得当作「无滤镜」。
//   - docs/ACSD_DESIGN.md §10「统一 I/O 是文件级唯一边界」
//     —— 读取经 aio_file::read_all（aio 内唯一实现, header-only）。
//
// 已退役（AGENTS §7：退役代码删除，或留统一注释块）:
//   - `map_filter_name`（FITS 头值 → 库键的别名表）：违反 `aliases:{}` 与「字节精确、
//     无归一、无别名」⇒ 已删除。
//   - `load_curve(file, name)` / `find_curve_object` / `list_curve_keys`：旧「聚合文件
//     路径 + 全文定位」读取面（`photometry.fit.filters_json` / `qe_json` 语义）。
//     目录扫描落地后该面没有第二个调用方，整段删除；曲线只从扫描集来。
// ============================================================================

#include "aio_file_io.h"   // aio_file::read_all（aio 唯一 I/O 实现, header-only）
#include "crypto/sha256.h" // acsd::crypto::sha256_hex（曲线内容身份, 单一实现）

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace acsd {
namespace photometry {
namespace curve_json {

// ── 扫描结果（调用方据此给出**具名**诊断, 不得把失败折叠成"取到某条曲线"）────
enum class ScanStatus {
    kOk = 0,
    kExeDirUnavailable = 1,     // 无法确定可执行文件目录
    kDirUnreachable = 2,        // 目录不存在 / 不可读
    kDirNotDirectory = 3,       // 存在但不是目录
    kDirEmpty = 4,              // 目录内无 *.json（扫描集为空 ⇒ fail-closed）
    kTooManyFiles = 5,          // 文件数超硬上限
    kFileTooLarge = 6,          // 单文件超硬上限
    kFileUnreadable = 7,        // 文件不可读
    kParseError = 8,            // 字段缺失 / 数组不可解析
    kNameDuplicate = 9,         // 重名（curve_name_duplicate）
    kIdentityMismatch = 10,     // 身份核对失败（curve_identity_mismatch）
    kCurveNotFound = 11,        // 请求名不在扫描集（unknown_filter）
    kChannelClassMismatch = 12, // 名字命中但类别不符（滤镜名命中 QE 曲线 / 反之）
};

inline const char* scan_status_name(ScanStatus s) {
    switch (s) {
        case ScanStatus::kOk: return "ok";
        case ScanStatus::kExeDirUnavailable: return "executable_dir_unavailable";
        case ScanStatus::kDirUnreachable: return "curve_dir_unreachable";
        case ScanStatus::kDirNotDirectory: return "curve_dir_not_a_directory";
        case ScanStatus::kDirEmpty: return "curve_dir_empty";
        case ScanStatus::kTooManyFiles: return "curve_dir_too_many_files";
        case ScanStatus::kFileTooLarge: return "curve_file_too_large";
        case ScanStatus::kFileUnreadable: return "curve_file_unreadable";
        case ScanStatus::kParseError: return "curve_file_parse_error";
        case ScanStatus::kNameDuplicate: return "curve_name_duplicate";
        case ScanStatus::kIdentityMismatch: return "curve_identity_mismatch";
        case ScanStatus::kCurveNotFound: return "unknown_filter";
        case ScanStatus::kChannelClassMismatch: return "curve_channel_class_mismatch";
    }
    return "unknown";
}

// 硬上限（防误放；合同「单文件大小与文件数设硬上限」）
inline constexpr std::size_t kMaxCurveFiles = 4096;
inline constexpr std::size_t kMaxCurveFileBytes = 8u * 1024u * 1024u;
inline constexpr std::size_t kMaxCandidateNames = 12;   // 报错列候选的条数

// ── 曲线身份核对的具名细节 ──────────────────────────────────────────────────
// 本结构只承载**诊断**：身份判据本身的定义在 check_curve_identity()，落盘/报错
// 由调用方决定；本头不写盘、不打印。
struct IdentityMismatch {
    std::string requested;      // 请求的曲线名（= 库键）
    std::string object_name;    // 曲线对象 name 字段的自述值（缺字段时为空）
    bool object_name_present = false;
    std::string reason;         // 机器可读原因码
    std::string detail;         // 人可读细节（含具体数值）
};

// 曲线身份核对结果（供装配检查与判据共用；本函数是唯一实现）。
struct CurveIdentity {
    bool ok = false;
    IdentityMismatch mismatch;
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
// provenance / curve_stats 这类非曲线对象。
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

// 相对比较（curve_stats 由转录脚本按同一数组算出 ⇒ 正常应逐位相等；容差只吸收
// JSON 十进制往返的末位差异，不是"允许曲线不同"的余地）。
inline bool num_equal(double a, double b) {
    if (!std::isfinite(a) || !std::isfinite(b)) return false;
    const double scale = std::max(1.0, std::max(std::fabs(a), std::fabs(b)));
    return std::fabs(a - b) <= 1e-9 * scale;
}

// 抽取曲线对象内的 wavelength_nm / value 数组（去逗号后按空白切分浮点）。
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

// ── 曲线身份核对（装配期 fail-closed；判据见合同「曲线身份核对」条）──────────
// ③④ 段：把 `curve_stats` 对象与实际数组逐项对账。stats 缺字段（含逐元素指纹）
// ⇒ fail-closed（缺字段 ≠ 身份已核对）。
inline bool check_curve_stats(const std::string& stats_obj,
                              const std::vector<double>& wl,
                              const std::vector<double>& trans,
                              CurveIdentity* id) {
    double s_npts = 0.0, s_lo = 0.0, s_hi = 0.0, s_vlo = 0.0, s_vhi = 0.0;
    double s_wlsum = 0.0, s_vsum = 0.0, s_vsumsq = 0.0;
    const bool ok_n = object_number_field(stats_obj, "n_points", &s_npts);
    const bool ok_lo = object_number_field(stats_obj, "wl_min", &s_lo);
    const bool ok_hi = object_number_field(stats_obj, "wl_max", &s_hi);
    const bool ok_vlo = object_number_field(stats_obj, "val_min", &s_vlo);
    const bool ok_vhi = object_number_field(stats_obj, "val_max", &s_vhi);
    const bool ok_wlsum = object_number_field(stats_obj, "wl_sum", &s_wlsum);
    const bool ok_vsum = object_number_field(stats_obj, "val_sum", &s_vsum);
    const bool ok_vsumsq = object_number_field(stats_obj, "val_sumsq", &s_vsumsq);
    if (!ok_n || !ok_lo || !ok_hi || !ok_vlo || !ok_vhi) {
        id->mismatch.reason = "curve_stats_unparsable";
        id->mismatch.detail = "provenance.curve_stats 字段缺失或不可解析"
                              "（n_points/wl_min/wl_max/val_min/val_max）";
        return false;
    }
    // min/max 只是包络，两支不同曲线可以共用同一包络 ⇒ 逐元素指纹是必需项。
    if (!ok_wlsum || !ok_vsum || !ok_vsumsq) {
        id->mismatch.reason = "curve_stats_fingerprint_missing";
        id->mismatch.detail = "provenance.curve_stats 缺派生指纹字段"
                              "（wl_sum/val_sum/val_sumsq）—— 身份判据要求逐元素量，"
                              "包络（min/max）不足以定身份";
        return false;
    }
    const double lo = *std::min_element(wl.begin(), wl.end());
    const double hi = *std::max_element(wl.begin(), wl.end());
    const double vlo = *std::min_element(trans.begin(), trans.end());
    const double vhi = *std::max_element(trans.begin(), trans.end());
    char buf[256];
    if (s_npts != static_cast<double>(wl.size())) {
        std::snprintf(buf, sizeof(buf),
                      "curve_stats.n_points=%.0f vs 实际曲线点数 %zu", s_npts, wl.size());
        id->mismatch.reason = "curve_stats_mismatch";
        id->mismatch.detail = buf;
        return false;
    }
    if (!num_equal(s_lo, lo) || !num_equal(s_hi, hi)) {
        std::snprintf(buf, sizeof(buf),
                      "curve_stats 波长域 [%.4f,%.4f] vs 实际 [%.4f,%.4f] nm",
                      s_lo, s_hi, lo, hi);
        id->mismatch.reason = "curve_stats_mismatch";
        id->mismatch.detail = buf;
        return false;
    }
    if (!num_equal(s_vlo, vlo) || !num_equal(s_vhi, vhi)) {
        std::snprintf(buf, sizeof(buf),
                      "curve_stats 透过率域 [%.6f,%.6f] vs 实际 [%.6f,%.6f]",
                      s_vlo, s_vhi, vlo, vhi);
        id->mismatch.reason = "curve_stats_mismatch";
        id->mismatch.detail = buf;
        return false;
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
                      "curve_stats wl_sum=%.6f vs 实际 %.6f（波长轴与声明不符）",
                      s_wlsum, wl_sum);
        id->mismatch.reason = "curve_stats_mismatch";
        id->mismatch.detail = buf;
        return false;
    }
    if (!num_equal(s_vsum, v_sum)) {
        std::snprintf(buf, sizeof(buf),
                      "curve_stats val_sum=%.9f vs 实际 %.9f（透过率曲线与声明不符）",
                      s_vsum, v_sum);
        id->mismatch.reason = "curve_stats_mismatch";
        id->mismatch.detail = buf;
        return false;
    }
    if (!num_equal(s_vsumsq, v_sumsq)) {
        std::snprintf(buf, sizeof(buf),
                      "curve_stats val_sumsq=%.9f vs 实际 %.9f（透过率曲线与声明不符）",
                      s_vsumsq, v_sumsq);
        id->mismatch.reason = "curve_stats_mismatch";
        id->mismatch.detail = buf;
        return false;
    }
    return true;
}

// 单条曲线文件（一文件一曲线）的四项身份核对：
//   ① 对象 `name` 与请求名（= 库键）逐字节相等；
//   ② `n_points` == `wavelength_nm` 数组长度（且两数组等长）；
//   ③④ `provenance.curve_stats` 的 min/max 与逐元素指纹 == 实际数组。
inline CurveIdentity check_curve_identity(const std::string& file_content,
                                          const std::string& requested_name,
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
    if (object_number_field(file_content, "n_points", &n_points_field) &&
        n_points_field != static_cast<double>(wl.size())) {
        id.mismatch.reason = "n_points_field_mismatch";
        id.mismatch.detail =
            "n_points=" + std::to_string(static_cast<long long>(n_points_field)) +
            " array_len=" + std::to_string(wl.size());
        return id;
    }
    std::string obj_name;
    const bool has_name = object_string_field(file_content, "name", &obj_name);
    id.mismatch.object_name_present = has_name;
    id.mismatch.object_name = has_name ? obj_name : std::string();
    if (!has_name) {
        id.mismatch.reason = "curve_object_name_field_missing";
        id.mismatch.detail = "曲线对象无 name 字段（字段集 = name/channel/"
                             "wavelength_nm/value/n_points/provenance）";
        return id;
    }
    if (obj_name != requested_name) {
        id.mismatch.reason = "curve_object_name_differs_from_requested";
        id.mismatch.detail = "曲线对象自述 name='" + obj_name + "' 与库键 '" +
                             requested_name + "' 不等（按文本位置取错曲线的判别式）";
        return id;
    }
    std::string prov;
    if (!find_object_by_key(file_content, "provenance", &prov)) {
        id.mismatch.reason = "provenance_missing";
        id.mismatch.detail = "曲线文件无 provenance 段（每条曲线必须自带出处与 curve_stats）";
        return id;
    }
    std::string stats;
    if (!find_object_by_key(prov, "curve_stats", &stats)) {
        id.mismatch.reason = "provenance_curve_stats_missing";
        id.mismatch.detail = "provenance 无 curve_stats 对象";
        return id;
    }
    if (!check_curve_stats(stats, wl, trans, &id)) return id;
    id.ok = true;
    return id;
}

// ── 曲线**有序样本内容**的身份摘要（#42 / #104）──────────────────────────
// 口径: 库键、n_points、min/max、wl_sum/val_sum/val_sumsq **都不足以**唯一
// 识别一条曲线（两条不同曲线可共用同一包络与同一和；把波长位置对调后这三项
// 完全不变）⇒ 响应身份必须绑定**有序样本内容**。
// 规范化字节串 = "acsd-curve-v1\n" + n + "\n" + 逐点 "<wl_bits>:<val_bits>;"
// 其中 <bits> 是 IEEE-754 double 的**位模式**（16 位小写 hex ⇒ 与平台字节序
// 无关, 且不做任何十进制往返）。
inline std::string curve_content_sha256(const std::vector<double>& wl,
                                        const std::vector<double>& val) {
    std::string blob = "acsd-curve-v1\n";
    blob += std::to_string(wl.size());
    blob += "\n";
    char buf[64];
    for (std::size_t i = 0; i < wl.size(); ++i) {
        unsigned long long wb = 0ull, vb = 0ull;
        const double w = wl[i];
        const double v = (i < val.size()) ? val[i] : 0.0;
        std::memcpy(&wb, &w, sizeof(double));
        std::memcpy(&vb, &v, sizeof(double));
        std::snprintf(buf, sizeof(buf), "%016llx:%016llx;", wb, vb);
        blob += buf;
    }
    return acsd::crypto::sha256_hex(blob.data(), blob.size());
}

// ── 可执行文件目录（唯一机制；无覆盖链）────────────────────────────────────
// 与仓内既有约定同款（lib/infrastructure/cli/commands.cpp 的 cli_install_dir）：
// Windows 用 GetModuleFileNameA(nullptr)，POSIX 用 /proc/self/exe 的 canonical 路径。
inline std::string executable_dir() {
#if defined(_WIN32)
    char buf[MAX_PATH];
    const DWORD n = ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        std::error_code ec;
        const auto p = std::filesystem::path(std::string(buf, n)).parent_path();
        if (!p.empty()) return p.string();
    }
    return std::string(".");
#else
    std::error_code ec;
    const auto cp = std::filesystem::canonical("/proc/self/exe", ec);
    if (!ec) {
        const auto p = cp.parent_path();
        if (!p.empty()) return p.string();
    }
    return std::string(".");
#endif
}

// 曲线目录 = <可执行文件目录>/config/response_curves（**唯一相对路径**）
inline std::string response_curve_dir() {
    const std::string d = executable_dir();
    return (d == ".") ? std::string("config/response_curves")
                      : d + "/config/response_curves";
}

// ── 扫描集 ──────────────────────────────────────────────────────────────────
struct CurveEntry {
    std::string key;        // 曲线名（= 文件内 `name`，字节精确）
    std::string channel;    // 通带类别；QE 曲线 = "Q"
    std::string file;       // 来源文件（绝对路径，供 provenance 记录）
    std::vector<double> wl;
    std::vector<double> value;
    bool is_qe() const { return channel == "Q"; }
};

struct CurveLibrary {
    std::string dir;                 // 扫描目录（绝对路径）
    std::vector<CurveEntry> entries; // 按文件名字节序稳定排序

    // 字节精确查找（无归一、无别名）。未命中 ⇒ nullptr。
    const CurveEntry* find(const std::string& key) const {
        for (const auto& e : entries) {
            if (e.key == key) return &e;
        }
        return nullptr;
    }
    // 类别匹配查找：want_qe=true 只接受 `channel == "Q"`，false 只接受非 QE。
    const CurveEntry* find_class(const std::string& key, bool want_qe) const {
        const CurveEntry* e = find(key);
        if (e == nullptr || e->is_qe() != want_qe) return nullptr;
        return e;
    }
    std::vector<std::string> keys(bool want_qe) const {
        std::vector<std::string> out;
        for (const auto& e : entries) {
            if (e.is_qe() == want_qe) out.push_back(e.key);
        }
        return out;
    }
    std::size_t count(bool want_qe) const {
        std::size_t n = 0;
        for (const auto& e : entries) {
            if (e.is_qe() == want_qe) ++n;
        }
        return n;
    }
};

// 候选串（供「不在扫描集 ⇒ 列候选」的具名报错；最多 max_show 条，超出以 "..." 收尾）
inline std::string format_key_candidates(const std::vector<std::string>& keys,
                                         std::size_t max_show = kMaxCandidateNames) {
    std::string out;
    for (std::size_t i = 0; i < keys.size() && i < max_show; ++i) {
        if (i) out += ", ";
        out += keys[i];
    }
    if (keys.size() > max_show) out += ", ...";
    return out;
}

// **唯一解析实现**：扫描 <exe_dir>/config/response_curves/ 并逐条做身份核对。
// 失败 ⇒ 返回具名状态（*err_detail 带文件/名字细节），**不回退**、不返回半份集合。
inline ScanStatus scan_curve_library(CurveLibrary* out, std::string* err_detail) {
    if (err_detail) err_detail->clear();
    if (out == nullptr) return ScanStatus::kExeDirUnavailable;
    out->entries.clear();
    const std::string dir = response_curve_dir();
    out->dir = dir;
    if (dir.empty()) {
        if (err_detail) *err_detail = "无法确定可执行文件目录";
        return ScanStatus::kExeDirUnavailable;
    }
    std::error_code ec;
    const auto dir_path = std::filesystem::path(dir);
    const auto st = std::filesystem::status(dir_path, ec);
    if (ec || !std::filesystem::exists(st)) {
        if (err_detail) *err_detail = "曲线目录不可达: " + dir;
        return ScanStatus::kDirUnreachable;
    }
    if (!std::filesystem::is_directory(st)) {
        if (err_detail) *err_detail = "曲线路径不是目录: " + dir;
        return ScanStatus::kDirNotDirectory;
    }
    // 非递归、`*.json`、按文件名**字节序稳定排序**。
    std::vector<std::filesystem::path> files;
    std::error_code ec_it;
    for (std::filesystem::directory_iterator it(dir_path, ec_it), end; it != end;
         it.increment(ec_it)) {
        if (ec_it) {
            if (err_detail) *err_detail = "目录遍历失败: " + dir;
            return ScanStatus::kDirUnreachable;
        }
        const auto& p = it->path();
        if (p.extension() != ".json") continue;
        std::error_code ec_sym;
        if (std::filesystem::is_symlink(std::filesystem::symlink_status(p, ec_sym))) {
            // 合同：不跟随指向目录外的符号链接。
            std::error_code ec_c;
            const auto canon = std::filesystem::weakly_canonical(p, ec_c);
            const auto dir_canon = std::filesystem::weakly_canonical(dir_path, ec_c);
            if (ec_c || canon.string().rfind(dir_canon.string() + "/", 0) != 0) continue;
        }
        files.push_back(p);
        if (files.size() > kMaxCurveFiles) {
            if (err_detail) *err_detail = "曲线文件数超硬上限 " +
                                          std::to_string(kMaxCurveFiles) + ": " + dir;
            return ScanStatus::kTooManyFiles;
        }
    }
    if (files.empty()) {
        if (err_detail) *err_detail = "曲线目录内无 *.json: " + dir;
        return ScanStatus::kDirEmpty;
    }
    std::sort(files.begin(), files.end(),
              [](const std::filesystem::path& a, const std::filesystem::path& b) {
                  return a.filename().string() < b.filename().string();   // 字节序
              });
    for (const auto& p : files) {
        std::error_code ec_sz;
        const auto sz = std::filesystem::file_size(p, ec_sz);
        if (!ec_sz && sz > kMaxCurveFileBytes) {
            if (err_detail) *err_detail = "曲线文件超硬上限 " +
                                          std::to_string(kMaxCurveFileBytes) + " 字节: " +
                                          p.string();
            return ScanStatus::kFileTooLarge;
        }
        std::string content;
        if (!aio_file::read_all(p.string().c_str(), &content)) {
            if (err_detail) *err_detail = "曲线文件不可读: " + p.string();
            return ScanStatus::kFileUnreadable;
        }
        CurveEntry e;
        e.file = p.string();
        if (!object_string_field(content, "name", &e.key) || e.key.empty()) {
            if (err_detail) *err_detail = "曲线文件缺 name 字段或为空: " + p.string();
            return ScanStatus::kParseError;
        }
        if (!object_string_field(content, "channel", &e.channel)) {
            if (err_detail) *err_detail = "曲线文件缺 channel 字段: " + p.string();
            return ScanStatus::kParseError;
        }
        if (!extract_curve_arrays(content, &e.wl, &e.value)) {
            if (err_detail) *err_detail =
                "曲线文件缺 wavelength_nm/value 数组或两数组长度不等: " + p.string();
            return ScanStatus::kParseError;
        }
        const CurveIdentity id = check_curve_identity(content, e.key, e.wl, e.value);
        if (!id.ok) {
            if (err_detail) {
                *err_detail = "曲线身份核对失败 (" + id.mismatch.reason + "): " +
                              p.string() + " [" + id.mismatch.detail + "]";
            }
            return ScanStatus::kIdentityMismatch;
        }
        // 名字唯一性（字节精确）：重名 ⇒ 报按文件名序的第一个冲突。
        for (const auto& prev : out->entries) {
            if (prev.key == e.key) {
                if (err_detail) {
                    *err_detail = "曲线名重名 (curve_name_duplicate): '" + e.key +
                                  "' 同时出现在 " + prev.file + " 与 " + e.file;
                }
                return ScanStatus::kNameDuplicate;
            }
        }
        out->entries.push_back(std::move(e));
    }
    return ScanStatus::kOk;
}

// 扫描失败的统一判词（两个调用方共用同一措辞）
inline std::string scan_failure_message(ScanStatus st, const CurveLibrary& lib,
                                        const std::string& detail) {
    std::string msg = std::string("response curve library scan failed: ") +
                      scan_status_name(st) + " (dir='" + lib.dir + "')";
    if (!detail.empty()) msg += " [" + detail + "]";
    return msg;
}

// 未命中扫描集的统一判词（`unknown_filter` + 候选 + 扫描总数）
inline std::string unknown_curve_message(const CurveLibrary& lib,
                                         const std::string& requested, bool want_qe) {
    const std::vector<std::string> keys = lib.keys(want_qe);
    return std::string("unknown_filter: '") + requested +
           "' not in the scanned curve library (dir='" + lib.dir + "'; " +
           (want_qe ? "QE" : "passband") + " curves=" + std::to_string(keys.size()) +
           "); candidates: " + format_key_candidates(keys);
}

}  // namespace curve_json
}  // namespace photometry
}  // namespace acsd

#endif  // ACSD_PHOTOMETRY_FILTER_CURVE_JSON_H
