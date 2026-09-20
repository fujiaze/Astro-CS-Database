// lib/algorithms/projection/p3_projection_registry.h — Phase3 投影「产品声明注册表」
// （ASTROCS_DESIGN.md §5.3 的唯一产品声明权威；FIX-205）
//
// 规范依据:
//   * ASTROCS_DESIGN.md §5.3: 首批冻结 8 投影 TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA;
//     「已实现并可作为产品声明的以实际注册表为准; 未实现的必须显式报「不支持」,
//     禁止声称支持」(当前登记: 仅 TAN 已实现); 每种投影必须声明适用域
//     (含 TAN 的 |dec|≤85°、FOV≤20°、手性 det(CD)<0、CRPIX 用 FITS 1-based
//     像素中心、往返误差 <1e-6 px), 违反 ⇒ 拒绝。
//   * docs/algorithms/PHASE3_PROJ_IMPL.md §15.1/§15.5: v6 内核 registry
//     (p3_proj_v6.h/.cpp, kProjectionRegistryVersion=3) 已实现 4/8
//     (TAN/SIN/CAR/AIT), 但**会话/产品面收窄为仅 TAN**; 内核行不是产品声明。
//
// 三层集合（本文件是唯一权威; 机器判据见 p3_proj_registry_selfcheck 与共址测试
// lib/algorithms/projection/tests/p3wcs/p3_projection_registry_test.cpp）:
//   ① 冻结集 F = DESIGN §5.3 八投影（p3_proj_frozen_table）;
//   ② 实现集 I = 生产路径真正有内核的码（p3_proj_is_implemented）;
//   ③ 声明集 D = 可作为产品声明的码（p3_proj_is_declared）。
//   判据: D == I == 实际可运行集 R（R = 生产路径黑盒实跑 p3_proj_probe 通过的码）。
//   任一不等即判红: 声明未实现 = 假声称（禁止）; 实现未声明 = 隐藏能力（禁止）。
//
// fail-closed: 未冻结码 / 冻结未实现码 / 冻结仅内核码一律显式拒绝并给出
// 已支持清单; **不得**静默回落 TAN, **不得**声称支持（DESIGN §5.3）。
//
// 实现形态 = header-only（inline）: 生产内核 p3_wcs.cpp 被多个 target 以「同 TU
// 直编」方式复用（如 tests/unit/p3_projection_test），头内实现使这些既有 target
// 零构建改动即取得同一份注册表, 不产生第二权威, 也不新增链接面。
#ifndef ASTROCS_P3_PROJECTION_REGISTRY_H
#define ASTROCS_P3_PROJECTION_REGISTRY_H

#include <cmath>
#include <cstring>
#include <string>

#include "p3_wcs.h"

namespace astrocs::phase3 {

// 冻结码的产品声明状态（三值, 无第四态）。
enum class P3ProjProductStatus : int {
    kProductDeclarable = 0,  // 生产路径可跑通且适用域已声明 ⇒ 可作产品声明
    kKernelOnly = 1,         // 内核已实现（v6 registry 有行）但未接入生产路径
    kNotImplemented = 2      // 无内核、无生产接线
};

// 冻结集合逐码登记行（code 为静态存储的冻结码字面量）。
struct P3ProjFrozenEntry {
    const char* code;             // 冻结码（大小写敏感）
    P3ProjProductStatus status;   // 产品声明状态
    const char* reason;           // 状态依据（不可声明原因必须写明）
};

namespace p3_proj_registry_detail {

inline bool in_list(const char* const* list, int n, const char* code) {
    if (!code) return false;
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(list[i], code) == 0) return true;
    }
    return false;
}

}  // namespace p3_proj_registry_detail

// ---- 冻结集 F（DESIGN §5.3 八投影, 冻结序）----
inline const P3ProjFrozenEntry* p3_proj_frozen_table(int* count) {
    // reason 串同时用于文档与拒绝消息; CLI 错误文本上限 200 字节
    // (parser.cpp sanitize), 故消息把「请求码 + 不支持 + 已支持清单」前置。
    static constexpr P3ProjFrozenEntry kTable[8] = {
        {"TAN", P3ProjProductStatus::kProductDeclarable,
         "production kernel p3_wcs.cpp + applicability domain declared"},
        {"SIN", P3ProjProductStatus::kKernelOnly,
         "not product-declarable (v6 kernel not wired to the production path)"},
        {"CAR", P3ProjProductStatus::kKernelOnly,
         "not product-declarable (v6 kernel not wired to the production path)"},
        {"AIT", P3ProjProductStatus::kKernelOnly,
         "not product-declarable (v6 kernel not wired to the production path)"},
        {"STG", P3ProjProductStatus::kNotImplemented,
         "not product-declarable (no kernel, no production wiring)"},
        {"MOL", P3ProjProductStatus::kNotImplemented,
         "not product-declarable (no kernel, no production wiring)"},
        {"CEA", P3ProjProductStatus::kNotImplemented,
         "not product-declarable (no kernel, no production wiring)"},
        {"ZEA", P3ProjProductStatus::kNotImplemented,
         "not product-declarable (no kernel, no production wiring)"},
    };
    if (count) *count = 8;
    return kTable;
}

inline bool p3_proj_is_frozen_code(const char* code) {
    int n = 0;
    const P3ProjFrozenEntry* t = p3_proj_frozen_table(&n);
    if (!code) return false;
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(t[i].code, code) == 0) return true;
    }
    return false;
}

inline const char* p3_proj_frozen_list() {
    return "TAN/SIN/CAR/AIT/STG/MOL/CEA/ZEA";
}

// ---- 声明集 D（唯一权威; 产品可声明码）----
inline const char* const* p3_proj_declared_codes(int* count) {
    // 当前仅 TAN（DESIGN §5.3「当前登记: 仅 TAN 已实现」）。
    static constexpr const char* kDeclared[1] = {"TAN"};
    if (count) *count = 1;
    return kDeclared;
}

inline bool p3_proj_is_declared(const char* code) {
    int n = 0;
    const char* const* c = p3_proj_declared_codes(&n);
    return p3_proj_registry_detail::in_list(c, n, code);
}

inline const char* p3_proj_declared_list() {
    return "TAN";
}

// ---- 实现集 I（生产路径真正有内核的码）----
inline const char* const* p3_proj_implemented_codes(int* count) {
    // 新增投影必须同时进实现集与声明集, 否则 p3_proj_registry_selfcheck 判红。
    static constexpr const char* kImplemented[1] = {"TAN"};
    if (count) *count = 1;
    return kImplemented;
}

inline bool p3_proj_is_implemented(const char* code) {
    int n = 0;
    const char* const* c = p3_proj_implemented_codes(&n);
    return p3_proj_registry_detail::in_list(c, n, code);
}

// 冻结码字面量（静态存储, 可安全存指针）; 非冻结码 → nullptr。
inline const char* p3_proj_canonical_code(const char* code) {
    int n = 0;
    const P3ProjFrozenEntry* t = p3_proj_frozen_table(&n);
    if (!code) return nullptr;
    for (int i = 0; i < n; ++i) {
        if (std::strcmp(t[i].code, code) == 0) return t[i].code;
    }
    return nullptr;
}

// ---- 产品声明门（请求面唯一语义源）----
// OK: 码可声明; UNSUPPORTED: 显式不支持（why = 请求码 + 原因 + 已支持清单）;
// nullptr/缺省 → TAN（与 p3_wcs_validate_request 冻结缺省一致）。
inline P3WcsStatus p3_proj_declare(const char* code, std::string* why) {
    const std::string c = code ? std::string(code) : std::string("TAN");
    if (p3_proj_is_declared(c.c_str())) {
        if (why) why->clear();
        return P3_WCS_OK;
    }
    if (why) {
        // 顺序冻结: 请求码 → 「unsupported」 → 已支持清单 → 原因。
        // 原因放最后, 使 CLI 侧 200 字节截断（parser.cpp sanitize）不会吃掉
        // 「已支持清单」这一验收门要求的信息。
        std::string reason;
        if (c.empty()) {
            reason = "empty projection code";
        } else if (p3_proj_is_frozen_code(c.c_str())) {
            int n = 0;
            const P3ProjFrozenEntry* t = p3_proj_frozen_table(&n);
            for (int i = 0; i < n; ++i) {
                if (std::strcmp(t[i].code, c.c_str()) == 0) reason = t[i].reason;
            }
        } else {
            reason = "unknown projection code (not in the frozen 8-projection set)";
        }
        *why = "projection '" + c + "' unsupported; supported projections: " +
               p3_proj_declared_list() + " (ASTROCS_DESIGN 5.3); reason: " + reason;
    }
    return P3_WCS_UNSUPPORTED;
}

// ---- 实际可运行探针（黑盒实跑生产路径, 不查声明表）----
// 顺序: p3_wcs_validate_request → p3_wcs_make → 往返 <1e-6 px → CTYPE 含该码。
// OK = 该码在生产路径真的可跑; 其余状态 = 实跑失败（detail 填原因）。
inline P3WcsStatus p3_proj_probe(const char* code, std::string* detail) {
    if (!code || !*code) {
        if (detail) *detail = "probe: empty projection code";
        return P3_WCS_PARAM;
    }
    // 探针不查声明表/实现表 —— 只看生产路径的真实行为（判据独立性）。
    std::string why;
    const P3WcsStatus vst = p3_wcs_validate_request(code, nullptr, nullptr, &why);
    if (vst != P3_WCS_OK) {
        if (detail) *detail = "probe: production request gate rejected: " + why;
        return vst;
    }
    // 探针几何: 0.5"/px × 64×64（FOV 对角 ~0.0126° ≪ 20° 适用域上界）。
    P3WcsDescriptor d{};
    const P3WcsStatus mst =
        p3_wcs_make(150.0, 2.0, 0.0001389, 64, 64, "east_left", 0.0, &d, code);
    if (mst != P3_WCS_OK) {
        if (detail)
            *detail = "probe: production make rejected (status " +
                      std::to_string(static_cast<int>(mst)) + ")";
        return mst;
    }
    double max_err_px = 0.0;
    const P3WcsStatus rst = p3_wcs_roundtrip_max_error_px(&d, &max_err_px);
    if (rst != P3_WCS_OK) {
        if (detail) *detail = "probe: roundtrip check failed";
        return rst;
    }
    if (!(max_err_px < 1e-6)) {   // SCI-P3-001 §7 冻结容差
        if (detail) *detail = "probe: roundtrip error exceeds 1e-6 px";
        return P3_WCS_PARAM;
    }
    const std::string kw = p3_wcs_fits_keywords(&d);
    const std::string ctype1 = std::string("RA---") + code;
    if (kw.find(ctype1) == std::string::npos) {
        if (detail)
            *detail = "probe: no CTYPE '" + ctype1 +
                      "' emitted (projection not declarable in FITS output)";
        return P3_WCS_UNSUPPORTED;
    }
    if (detail) {
        *detail =
            "probe ok: roundtrip max error " + std::to_string(max_err_px) + " px";
    }
    return P3_WCS_OK;
}

// ---- 注册表自检（启动自检与测试共用）----
// 检查: 冻结表 8 行/冻结序/码唯一/状态合法/reason 非空; 声明集 ⊆ 实现集 ⊆
// 冻结集; 声明集 == 状态为 kProductDeclarable 的行; kKernelOnly/kNotImplemented
// 不得出现在声明集。全过 0, 否则首个失败项序号（1 起）。
inline int p3_proj_registry_selfcheck() {
    static constexpr const char* kFrozenOrder[8] = {"TAN", "SIN", "CAR", "AIT",
                                                    "STG", "MOL", "CEA", "ZEA"};
    int n = 0;
    const P3ProjFrozenEntry* tab = p3_proj_frozen_table(&n);
    if (n != 8 || tab == nullptr) return 1;
    for (int i = 0; i < n; ++i) {
        if (tab[i].code == nullptr || tab[i].code[0] == '\0') return 2 + i;
        if (std::strcmp(tab[i].code, kFrozenOrder[i]) != 0) return 20 + i;
        if (tab[i].reason == nullptr || tab[i].reason[0] == '\0') return 40 + i;
        const int s = static_cast<int>(tab[i].status);
        if (s < 0 || s > 2) return 60 + i;
        for (int j = 0; j < i; ++j) {
            if (std::strcmp(tab[i].code, tab[j].code) == 0) return 80 + i;
        }
    }
    int nd = 0;
    int ni = 0;
    const char* const* decl = p3_proj_declared_codes(&nd);
    const char* const* impl = p3_proj_implemented_codes(&ni);
    if (nd <= 0 || decl == nullptr || ni <= 0 || impl == nullptr) return 100;
    for (int i = 0; i < nd; ++i) {
        if (!p3_proj_is_frozen_code(decl[i])) return 101 + i;   // D ⊆ F
        if (!p3_proj_is_implemented(decl[i])) return 110 + i;   // D ⊆ I
    }
    for (int i = 0; i < ni; ++i) {
        if (!p3_proj_is_frozen_code(impl[i])) return 120 + i;   // I ⊆ F
    }
    // 状态为 kProductDeclarable 的行必须恰是声明集; 其余状态不得进声明集。
    for (int i = 0; i < n; ++i) {
        const bool declared = p3_proj_is_declared(tab[i].code);
        const bool declarable =
            tab[i].status == P3ProjProductStatus::kProductDeclarable;
        if (declared != declarable) return 130 + i;
    }
    return 0;
}

}  // namespace astrocs::phase3

#endif  // ASTROCS_P3_PROJECTION_REGISTRY_H
