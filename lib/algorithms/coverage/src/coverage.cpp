// lib/algorithms/coverage/src/coverage.cpp — Phase2 W3 coverage union 实现（真实 AIO 接入）
//
// 语义（ 34A532A2...B2EB308 + wiki Phase2_Architecture）：
// - 输入为多个 Phase1 单帧 HiPS（signal/support/snr）；
// - 通过唯一 AIO（astro_image_io.dll aio_hips_reader）读取每帧
// properties 与叶级 tile 列表（Moc.fits）；
// - 兼容校验：hips_version / hips_frame / tile_width / obs_filter；
// - target_order = min(所有输入 max leaf order)（禁止低 order 插值伪装分辨率）；
// - Ω = MOC_1 ∪ ... ∪ MOC_N（NESTED，允许不连通分量）。
#include "astro/phase2/coverage.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// 唯一 AIO HiPS reader（Phase1 冻结模块，只读使用）
static std::map<std::string, std::string> parse_props(const char* text) {
    std::map<std::string, std::string> kv;
    if (!text) return kv;
    const char* p = text;
    while (*p) {
        const char* eol = std::strchr(p, '\n');
        const std::string line(p, eol ? (size_t)(eol - p) : std::strlen(p));
        if (!line.empty() && line[0] != '#') {
            const size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                auto trim = [](std::string& s) {
                    while (!s.empty() && (s.back() == ' ' || s.back() == '\r'))
                        s.pop_back();
                    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
                };
                trim(k); trim(v);
                kv[k] = v;
            }
        }
        if (!eol) break;
        p = eol + 1;
    }
    return kv;
}

extern "C" {


#include "aio_hips_reader.h"
}

extern "C" {

namespace {


// 读取单帧 HiPS：校验兼容性并收集叶级 tile ipix（NESTED order=hips_order）
int inspect_frame(const char* path, P2HipsInputInfo* info,
                  std::vector<std::uint64_t>* tiles, char* err, size_t err_size) {
    AioHipsDataset* d = aio_hips_open(path, AIO_HIPS_RD_SIGNAL);
    if (!d) {
        std::snprintf(err, err_size, "aio_hips_open failed: %s",
                      aio_hips_reader_last_error());
        return 1;
    }
    char buf[8192];
    if (aio_hips_get_properties(d, buf, (int)sizeof(buf)) != 0) {
        std::snprintf(err, err_size, "aio_hips_get_properties failed");
        aio_hips_close(d);
        return 1;
    }
    const auto kv = parse_props(buf);
    auto geti = [&](const std::string& k, int def) -> int {
        auto it = kv.find(k);
        return it == kv.end() ? def : std::atoi(it->second.c_str());
    };
    auto gets = [&](const std::string& k) -> std::string {
        auto it = kv.find(k);
        return it == kv.end() ? std::string() : it->second;
    };

    const int order = geti("hips_order", -1);
    const int tw = geti("hips_tile_width", 0);
    const std::string frame = gets("hips_frame");
    const std::string version = gets("hips_version");
    // B2-A8: 兼容前提 = 同一 filter/passband（coverage.h:7）。"键缺失"（外层
    // HiPS，从未声明观测 passband）与"键存在"是两种事实：前者无法与任何
    // 基准帧建立 filter 组，必须 fail-closed，而不是像旧实现那样因空串被
    // 静默并入 union（DISP-COV-003）。
    const bool has_filter = kv.find("obs_filter") != kv.end();
    const std::string filter = gets("obs_filter");
    if (!has_filter) {
        std::snprintf(err, err_size,
                      "missing obs_filter property (filter/passband identity "
                      "is required for compatibility): %s", path);
        aio_hips_close(d);
        return 1;
    }
    if (order < 0) {
        std::snprintf(err, err_size, "missing hips_order: %s", path);
        aio_hips_close(d);
        return 1;
    }
    if (tw != 512) {
        std::snprintf(err, err_size, "unsupported tile_width=%d: %s", tw, path);
        aio_hips_close(d);
        return 1;
    }
    if (version.empty()) {
        std::snprintf(err, err_size, "missing hips_version: %s", path);
        aio_hips_close(d);
        return 1;
    }
    if (frame != "equatorial" && frame != "icrs") {
        std::snprintf(err, err_size, "unsupported hips_frame=%s: %s",
                      frame.c_str(), path);
        aio_hips_close(d);
        return 1;
    }
    // B2-A8: 读并断言 hips_ordering。本模块的 union/int 父聚合（t >> 2s）
    // 与下游 NESTED 消费只在 NESTED 语义下成立；RING 输入必须 fail-closed
    // （旧实现全仓 0 命中 hips_ordering，AIO reader 直接按 NESTED 解释，
    // 使 RING 帧被静默错读）。
    const bool has_ordering = kv.find("hips_ordering") != kv.end();
    const std::string ordering = gets("hips_ordering");
    if (has_ordering && ordering != "NESTED") {
        std::snprintf(err, err_size, "unsupported hips_ordering=%s (NESTED "
                      "required): %s", ordering.c_str(), path);
        aio_hips_close(d);
        return 1;
    }

    if (info) {
        std::strncpy(info->hips_path, path, sizeof(info->hips_path) - 1);
        info->hips_path[sizeof(info->hips_path) - 1] = '\0';
        // frame_id：路径 basename（不含扩展名/分隔符）
        std::string base(path);
        const size_t slash = base.find_last_of("/\\");
        if (slash != std::string::npos) base = base.substr(slash + 1);
        std::strncpy(info->frame_id, base.c_str(), sizeof(info->frame_id) - 1);
        info->frame_id[sizeof(info->frame_id) - 1] = '\0';
        info->max_leaf_order = order;
        info->n_tiles = 0;
        std::strncpy(info->filter_passband, filter.c_str(),
                     sizeof(info->filter_passband) - 1);
        info->filter_passband[sizeof(info->filter_passband) - 1] = '\0';
        std::strncpy(info->frame_type, frame.c_str(),
                     sizeof(info->frame_type) - 1);
        info->frame_type[sizeof(info->frame_type) - 1] = '\0';
        // 语义审计面：缺省 key（AIO 写侧恒 NESTED）登记为空串，显式声明登记原值。
        std::strncpy(info->hips_ordering, ordering.c_str(),
                     sizeof(info->hips_ordering) - 1);
        info->hips_ordering[sizeof(info->hips_ordering) - 1] = '\0';
    }
    if (tiles) {
        const int n = aio_hips_tile_count(d);
        tiles->clear();
        tiles->reserve(n > 0 ? (size_t)n : 0);
        for (int i = 0; i < n; ++i) {
            std::uint64_t ipix = 0;
            if (aio_hips_tile_ipix(d, i, &ipix) == 0) tiles->push_back(ipix);
        }
        if (info) info->n_tiles = (int)tiles->size();
    }
    aio_hips_close(d);
    return 0;
}

} // namespace

int p2_coverage_build(const char* const* hips_paths,
                      std::uint64_t n_inputs,
                      P2CoverageResult* out) {
    if (out == nullptr) return 1;
    // 保存调用方缓冲区指针（memset 会清掉它们）
    P2HipsInputInfo* saved_inputs = out->inputs;
    P2MocCell* saved_cells = out->union_cells;
    std::memset(out, 0, sizeof(*out));
    out->inputs = saved_inputs;
    out->union_cells = saved_cells;
    if (hips_paths == nullptr || n_inputs == 0) {
        std::strncpy(out->error, "no inputs", sizeof(out->error) - 1);
        return 1;
    }
    // 第一次调用：逐帧检查并收集（为第二次填充做准备）
    std::vector<std::vector<std::uint64_t>> frame_tiles(n_inputs);
    std::vector<P2HipsInputInfo> infos(n_inputs);
    int target_order = -1;
    std::string filter_ref;
    std::string frame_ref;
    bool filter_set = false;
    for (std::uint64_t i = 0; i < n_inputs; ++i) {
        if (!hips_paths[i] || !*hips_paths[i]) {
            std::snprintf(out->error, sizeof(out->error),
                          "empty path at index %llu", (unsigned long long)i);
            out->status = 1;
            return 1;
        }
        char err[512] = {0};
        if (inspect_frame(hips_paths[i], &infos[i], &frame_tiles[i],
                          err, sizeof(err)) != 0) {
            // snprintf 保证 NUL 终止，避免 strncpy
            // 静默截断告警（CODE_STANDARD：禁止 silent truncation）。
            std::snprintf(out->error, sizeof(out->error), "%s", err);
            out->status = 1;
            return 1;
        }
        // 兼容校验：filter/passband 全等（含显式空声明；inspect_frame 已拒"键
        // 缺失"）。B2-A8: 旧实现只比较非空串，使一个带 filter 的帧与一个空
        // 声明帧被并入同一 union —— 违反 coverage.h:7「同一 filter/passband」前提。
        const std::string f = infos[i].filter_passband;
        const std::string fr = infos[i].frame_type;
        if (!filter_set) {
            filter_ref = f;
            frame_ref = fr;
            filter_set = true;
        } else if (f != filter_ref) {
            std::snprintf(out->error, sizeof(out->error),
                          "filter mismatch: %s vs %s",
                          filter_ref.c_str(), f.c_str());
            out->status = 1;
            return 1;
        }
        // B2-A8: 跨帧坐标系必须相等（旧实现只逐帧校验 frame∈{equatorial,icrs}，
        // 不比较跨帧一致性；equatorial 与 icrs 混用会让 MOC 父聚合跨坐标系）。
        if (fr != frame_ref) {
            std::snprintf(out->error, sizeof(out->error),
                          "hips_frame mismatch: %s vs %s",
                          frame_ref.c_str(), fr.c_str());
            out->status = 1;
            return 1;
        }
        // target_order = min(max leaf order)
        if (target_order < 0 || infos[i].max_leaf_order < target_order)
            target_order = infos[i].max_leaf_order;
    }
    if (target_order < 0) {
        std::strncpy(out->error, "no valid inputs", sizeof(out->error) - 1);
        out->status = 1;
        return 1;
    }

    // MOC union：统一到 target_order（NESTED parent 聚合）
    std::vector<std::uint64_t> union_cells;
    for (std::uint64_t i = 0; i < n_inputs; ++i) {
        const int shift = infos[i].max_leaf_order - target_order;
        for (std::uint64_t ip : frame_tiles[i]) {
            union_cells.push_back(ip >> (2 * shift));
        }
    }
    std::sort(union_cells.begin(), union_cells.end());
    union_cells.erase(std::unique(union_cells.begin(), union_cells.end()),
                      union_cells.end());

    out->n_inputs = n_inputs;
    out->target_order = target_order;
    out->n_union_cells = union_cells.size();
    if (out->union_cells != nullptr) {
        for (std::uint64_t i = 0; i < out->n_union_cells; ++i) {
            out->union_cells[i].order = (std::uint64_t)target_order;
            out->union_cells[i].ipix = union_cells[(size_t)i];
        }
    }
    if (out->inputs != nullptr) {
        for (std::uint64_t i = 0; i < n_inputs; ++i)
            out->inputs[i] = infos[(size_t)i];
    }
    out->status = 0;
    return 0;
}

int p2_coverage_free(P2CoverageResult* out) {
    if (out == nullptr) return 0;
    std::memset(out, 0, sizeof(*out));
    return 0;
}

// ===========================================================================
// V6 目标态：coverage/support 区分、确定性边界与权重角色门
// 冻结锚见 coverage.h V6 节；语义源 = docs/contracts/DATA_SEMANTICS.md §31
// astrocs.v6.contract-freeze.v1.json。禁止零填、禁止权重冒充。
// ===========================================================================

static void set_p2_err(char* err, std::size_t err_size, const char* msg) {
    if (err == nullptr || err_size == 0) return;
    std::snprintf(err, err_size, "%s", msg ? msg : "");
}

int p2_coverage_support_classify(const P2CoverageSupportInput* in,
                                 P2CellState* out_state,
                                 std::uint64_t* out_n_supported,
                                 std::uint64_t* out_n_covered_unsupported,
                                 std::uint64_t* out_n_uncovered,
                                 std::uint64_t* out_n_unavailable,
                                 char* err, std::size_t err_size) {
    if (in == nullptr || out_state == nullptr || in->n_cells == 0 ||
        in->coverage == nullptr || in->support_frames == nullptr ||
        in->support_known == nullptr) {
        set_p2_err(err, err_size,
                   "coverage_support_classify: invalid input (null or n_cells=0)");
        return 1;
    }
    std::uint64_t n_sup = 0, n_cov_unsup = 0, n_unc = 0, n_unavail = 0;
    for (std::uint64_t i = 0; i < in->n_cells; ++i) {
        const std::size_t k = (std::size_t)i;
        const bool cov_known = (in->coverage_known == nullptr) ? true
                                                               : (in->coverage_known[k] != 0);
        const bool sup_known = (in->support_known[k] != 0);
        P2CellState st;
        if (!cov_known || !sup_known) {
            // 缺失/NaN：显式 UNAVAILABLE，禁止零填为 UNCOVERED/UNSUPPORTED。
            st = P2_CELL_UNAVAILABLE;
            ++n_unavail;
        } else if (in->coverage[k] == 0) {
            st = P2_CELL_UNCOVERED;
            ++n_unc;
        } else if (in->support_frames[k] < in->min_support_frames) {
            st = P2_CELL_COVERED_UNSUPPORTED;
            ++n_cov_unsup;
        } else {
            st = P2_CELL_SUPPORTED;
            ++n_sup;
        }
        out_state[k] = st;
    }
    if (out_n_supported) *out_n_supported = n_sup;
    if (out_n_covered_unsupported) *out_n_covered_unsupported = n_cov_unsup;
    if (out_n_uncovered) *out_n_uncovered = n_unc;
    if (out_n_unavailable) *out_n_unavailable = n_unavail;
    return 0;
}

std::uint64_t p2_tile_boundary_owner(std::uint64_t leaf_ipix, int leaf_shift) {
    if (leaf_shift < 0 || leaf_shift > 31) {
        return UINT64_MAX;  // 非法位移哨兵；调用方必须 fail-closed
    }
    return leaf_ipix >> (2 * leaf_shift);
}

int p2_deterministic_reduction_order(const std::uint64_t* ipix_in,
                                     std::uint64_t n_in,
                                     std::uint64_t* out_sorted,
                                     std::uint64_t capacity,
                                     std::uint64_t* out_n,
                                     char* err, std::size_t err_size) {
    if (ipix_in == nullptr && n_in > 0) {
        set_p2_err(err, err_size,
                   "deterministic_reduction_order: null input with n_in>0");
        return 1;
    }
    std::vector<std::uint64_t> v(ipix_in, ipix_in + n_in);
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    if (out_n) *out_n = (std::uint64_t)v.size();
    if (out_sorted == nullptr) return 0;  // 仅查询容量
    if (capacity < (std::uint64_t)v.size()) {
        set_p2_err(err, err_size,
                   "deterministic_reduction_order: capacity insufficient");
        return 1;
    }
    for (std::size_t i = 0; i < v.size(); ++i) out_sorted[i] = v[i];
    return 0;
}

namespace {

// ── RETIRED-OBJECT-REJECT (PSFSW-RETIRE-01；ENGINEERING_SPEC §2 保留则注释) ──
// WHAT:       冻结 forbidden.weight_source_tokens 里的 "psfsw_robust_weight" / "psfsw"
//             —— **保留不动**：这是退役对象的**显式拒绝面**（旧产品把该对象写进
//             weight.sources / variance_from ⇒ rc=1 + token 名）。删除即变成静默接受。
// WHY:        负责人裁决（原话）：「只要纯净信号/噪声的信噪比。要求跨帧可用，不基于
//             参考帧。而是绝对标定。」⇒ 受 PixInsight PSFSW 启发的稳健复合帧权重
//             psfsw_robust_weight **不是现行对象**：ASTROCS_DESIGN.md §3.1
//             「权重只能来自纯净信号与噪声之比……任何使偏差随帧而变的量（含 PSF 拟合
//             质量代理）都不得进入科学叠加权重」；docs/design/UNIFIED_MODEL.md:58；
//             docs/science/PSF_SIGNAL_WEIGHT.md §1/§4；
//             统一对象退役登记 eng/contracts/data/unified_object_compatibility_map_v1.json:133-143。
// STATUS:     生产可达的合同门：p2_weight_source_token_reject
//             由 lib/algorithms/integration/v6/src/phase2_integrate.cpp 调用，
//             并在 coverage target 内编译。
// EXIT:       无（拒绝面必须保留）。
// AUTHORITY:  ASTROCS_DESIGN.md §3.1；docs/science/PSF_SIGNAL_WEIGHT.md §1/§4；
//             docs/design/UNIFIED_MODEL.md:58；ENGINEERING_SPEC.md §2/§3；
//             eng/contracts/data/unified_object_compatibility_map_v1.json:133-143（14→13 退役登记）。
// ──────────────────────────────────────────────────────────────────────
// 冻结 forbidden.weight_source_tokens（大小写不敏感全等匹配）。
// 语义源 = eng/contracts/data/v6_clause_registry_v1.json；
// 其中 psfsw_robust_weight/psfsw 两项按退役对象拒绝面保留（见上）。
const char* const kForbiddenWeightSourceTokens[] = {
    "median_source_snr", "median_snr", "source_snr_median", "med_source_snr",
    "support", "support_area", "coverage", "coverage_area",
    "fwhm", "psf_fwhm", "median_fwhm", "source_fwhm",
    "residual", "psf_residual", "psf_fit_residual", "fit_residual",
    "psfsw_robust_weight", "psfsw",
};

bool ascii_ieq(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb - 'A' + 'a');
        if (ca != cb) return false;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

}  // namespace

int p2_weight_source_token_reject(const char* const* tokens, std::uint64_t n,
                                  char* err, std::size_t err_size) {
    if (tokens == nullptr && n > 0) {
        set_p2_err(err, err_size, "weight_source_token_reject: null tokens");
        return 2;
    }
    const std::size_t n_forbidden =
        sizeof(kForbiddenWeightSourceTokens) / sizeof(kForbiddenWeightSourceTokens[0]);
    for (std::uint64_t i = 0; i < n; ++i) {
        const char* t = tokens[i];
        if (t == nullptr || *t == '\0') continue;
        for (std::size_t j = 0; j < n_forbidden; ++j) {
            if (ascii_ieq(t, kForbiddenWeightSourceTokens[j])) {
                if (err != nullptr && err_size > 0) {
                    // 退役对象与"诊断量冒充权重"分开报出（可诊断 + 迁移提示）：
                    // psfsw_robust_weight 连对象都不存在（不是"质量代理"）。
                    if (ascii_ieq(kForbiddenWeightSourceTokens[j], "psfsw_robust_weight")) {
                        std::snprintf(err, err_size,
                                      "forbidden weight source token '%s' "
                                      "(FZ-MODE-RETIRED: not a current object - "
                                      "ASTROCS_DESIGN.md 3.1; UNIFIED_MODEL.md:58; "
                                      "migration: Phase2 reconstructs the dense SNR field "
                                      "and derives inverse-variance weights "
                                      "w = SNR^2/F_ref^2; there is no selectable weight "
                                      "mode)",
                                      kForbiddenWeightSourceTokens[j]);
                    } else {
                        std::snprintf(err, err_size,
                                      "forbidden weight source token '%s' "
                                      "(FZ-GATE-SUPPORT-COVERAGE / FZ-GATE-MEDIAN-SNR / "
                                      "FZ-FIELD-WEIGHTMODE: diagnostics are not weights)",
                                      kForbiddenWeightSourceTokens[j]);
                    }
                }
                return 1;
            }
        }
    }
    return 0;
}

} // extern "C"
