// lib/algorithms/resample/p3_input_unit.cpp — Stage3 signal 输入单位身份裁决（纯逻辑）
//
// 分发规则（逐条可诊断；见头文件）:
//   ① properties 文本格式（无 '=' 行 / 空键 / NUL / 非单位键精确重复）
//   ② 单位声明面: BUNIT 逻辑键（大小写不敏感）+ ACSD_SIGNAL_UNIT 辅助面
//        缺声明 → MISSING；空串 → EMPTY；同键重复同值 → DUPLICATE；重复异值/两面对立 → CONFLICT/…
//   ③ 像素语义声明 ACSD_PIXEL_SEMANTICS / ACSD_PIXEL_AREA_POWER 的解析与内部一致性
//   ④ 冻结单位表归类（resolve_bunit）；裸 ADU 无 provenance → UNDECIDABLE；
//      表外语汇 → UNPARSABLE（要求上游科学模型裁决，本轮不自行映射）
//   ⑤ 身份 = signal 面亮度（adu_power=1, px_power=-2）→ 通过，canonical "ADU/sr"；
//      可解析但非面亮度 → NOT-SURFACE-BRIGHTNESS（科学语义，exit 4）
//   ⑥ 显式面亮度身份与已声明像素语义/面积幂次矛盾 → UNIT-SEMANTICS-CONFLICT（exit 4）
#include "p3_input_unit.h"

#include <cctype>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "p3_rsmp.h"

namespace acsd::phase3 {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

std::string ascii_upper(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (const char c : s)
        o += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return o;
}

std::string strip_ws(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (const char c : s)
        if (c != ' ' && c != '\t' && c != '\r') o += c;
    return o;
}

// legacy 像元面积写法（"px"/"pixel" 因子）在冻结面亮度口径中已被明确否定：
// docs/science/algorithms/export/PHASE3_RESAMPLE.md（"§31.1a 同时否定 'ADU/px^2'"）
// 与 PHASE3_RSMP_IMPL.md 同判。读侧解析器保留该别名只为兼容旧产品回读，
// **不构成** Stage3 输入身份可接受性的依据 ⇒ 生产入口 fail-closed。
bool uses_legacy_pixel_area_alias(const std::string& raw) {
    const std::string u = ascii_upper(strip_ws(raw));
    return u.find("PIXEL") != std::string::npos || u.find("PX") != std::string::npos;
}

P3InputUnitVerdict reject_input(P3InputUnitVerdict v, const std::string& code,
                                const std::string& reason) {
    v.accepted = false;
    v.input_error = true;
    v.code = code;
    v.reason = reason;
    return v;
}

P3InputUnitVerdict reject_science(P3InputUnitVerdict v, const std::string& code,
                                  const std::string& reason) {
    v.accepted = false;
    v.input_error = false;
    v.code = code;
    v.reason = reason;
    return v;
}

std::string spellings(const std::vector<std::pair<std::string, std::string>>& d) {
    std::string s;
    for (size_t i = 0; i < d.size(); ++i) {
        if (i) s += ", ";
        s += d[i].first + "='" + d[i].second + "'";
    }
    return s;
}

}  // namespace

P3InputUnitVerdict p3_input_unit_verdict_from_text(const std::string& text,
                                                   const std::string& properties_path) {
    using namespace acsd::p3rsmp;
    P3InputUnitVerdict v;
    v.properties_path = properties_path;

    // 单位声明面（保序，便于诊断重复/冲突）
    std::vector<std::pair<std::string, std::string>> bunit_decls, alias_decls;
    std::map<std::string, std::string> seen_exact;   // 非单位键精确重复检测
    std::string ps_raw, pap_raw;
    bool ps_seen = false, pap_seen = false;

    std::istringstream f(text);
    std::string line;
    int lineno = 0;
    while (std::getline(f, line)) {
        ++lineno;
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') continue;
        const size_t eq = t.find('=');
        if (eq == std::string::npos)
            return reject_input(v, "P3-INPUT-PROPERTIES-MALFORMED",
                                "malformed line " + std::to_string(lineno) +
                                    " (no '='): " + properties_path);
        const std::string key = trim(t.substr(0, eq));
        const std::string val = trim(t.substr(eq + 1));
        if (key.empty())
            return reject_input(v, "P3-INPUT-PROPERTIES-MALFORMED",
                                "empty key at line " + std::to_string(lineno) + ": " +
                                    properties_path);
        if (val.find('\0') != std::string::npos)
            return reject_input(v, "P3-INPUT-PROPERTIES-MALFORMED",
                                "value contains NUL at line " + std::to_string(lineno) +
                                    ": " + properties_path);
        const std::string up = ascii_upper(key);
        if (up == "BUNIT") {
            bunit_decls.push_back({key, val});
            continue;
        }
        if (up == "ACSD_SIGNAL_UNIT") {
            alias_decls.push_back({key, val});
            continue;
        }
        if (up == "ACSD_PIXEL_SEMANTICS") {
            if (ps_seen)
                return reject_input(v, "P3-INPUT-PROPERTIES-DUPLICATE-KEY",
                                    "duplicate key " + key + ": " + properties_path);
            ps_seen = true;
            ps_raw = val;
            continue;
        }
        if (up == "ACSD_PIXEL_AREA_POWER") {
            if (pap_seen)
                return reject_input(v, "P3-INPUT-PROPERTIES-DUPLICATE-KEY",
                                    "duplicate key " + key + ": " + properties_path);
            pap_seen = true;
            pap_raw = val;
            continue;
        }
        if (seen_exact.count(key))
            return reject_input(v, "P3-INPUT-PROPERTIES-DUPLICATE-KEY",
                                "duplicate properties key '" + key + "' (line " +
                                    std::to_string(lineno) + "): " + properties_path);
        seen_exact[key] = val;
    }

    // ── ② 单位声明面: 重复/大小写别名/冲突 ─────────────────────────────────
    if (bunit_decls.size() > 1) {
        bool same = true;
        for (const auto& d : bunit_decls)
            if (strip_ws(d.second) != strip_ws(bunit_decls[0].second)) same = false;
        if (same)
            return reject_input(v, "P3-INPUT-BUNIT-DUPLICATE",
                                "unit key declared " + std::to_string(bunit_decls.size()) +
                                    " times (" + spellings(bunit_decls) +
                                    "); unique-declaration contract (禁重复声明): " +
                                    properties_path);
        return reject_input(v, "P3-INPUT-BUNIT-CONFLICT",
                            "conflicting unit declarations (" + spellings(bunit_decls) +
                                "); unit identity must be unique: " + properties_path);
    }
    if (alias_decls.size() > 1) {
        bool same = true;
        for (const auto& d : alias_decls)
            if (strip_ws(d.second) != strip_ws(alias_decls[0].second)) same = false;
        if (same)
            return reject_input(v, "P3-INPUT-UNIT-ALIAS-DUPLICATE",
                                "unit alias key declared " +
                                    std::to_string(alias_decls.size()) + " times (" +
                                    spellings(alias_decls) + "): " + properties_path);
        return reject_input(v, "P3-INPUT-UNIT-ALIAS-CONFLICT",
                            "conflicting unit alias declarations (" + spellings(alias_decls) +
                                "): " + properties_path);
    }

    // ── 单位声明缺失 / 空串 ────────────────────────────────────────────────
    if (bunit_decls.empty()) {
        if (!alias_decls.empty())
            return reject_input(v, "P3-INPUT-BUNIT-MISSING",
                                "signal/properties has no FITS BUNIT key; only '" +
                                    alias_decls[0].first + "'='" + alias_decls[0].second +
                                    "' declares the unit. 外部 FITS 消费者读 BUNIT，"
                                    "canonical 单位键缺失 ⇒ 身份不可证实 (FZ-BUNIT-SEMANTICS;"
                                    " 禁按 ADU 猜测): " + properties_path);
        return reject_input(v, "P3-INPUT-BUNIT-MISSING",
                            "signal/properties declares no BUNIT; unit undecidable"
                            " (FZ-BUNIT-SEMANTICS; 禁按 ADU 猜测): " + properties_path);
    }
    v.bunit_raw = bunit_decls[0].second;
    v.unit_declarations = static_cast<int>(bunit_decls.size() + alias_decls.size());
    if (strip_ws(v.bunit_raw).empty())
        return reject_input(v, "P3-INPUT-BUNIT-EMPTY",
                            "BUNIT is declared but empty; unit undecidable"
                            " (FZ-BUNIT-SEMANTICS; 禁按 ADU 猜测): " + properties_path);
    if (!alias_decls.empty() &&
        strip_ws(alias_decls[0].second) != strip_ws(v.bunit_raw))
        return reject_input(v, "P3-INPUT-UNIT-DECLARATION-CONFLICT",
                            "two unit declaration surfaces disagree: BUNIT='" + v.bunit_raw +
                                "' vs " + alias_decls[0].first + "='" +
                                alias_decls[0].second + "': " + properties_path);

    // ── ③ 像素语义声明解析 ────────────────────────────────────────────────
    v.pixel_semantics = ps_raw;
    v.pixel_area_power = pap_raw;
    BunitProvenance prov;
    if (ps_seen && !ps_raw.empty()) {
        if (ps_raw == "surface_brightness")
            prov.pixel_semantics = PixelSemantics::SurfaceBrightness;
        else if (ps_raw == "integrated_flux")
            prov.pixel_semantics = PixelSemantics::IntegratedFlux;
        else
            return reject_science(v, "P3-INPUT-PIXEL-SEMANTICS-UNSUPPORTED",
                                  "ACSD_PIXEL_SEMANTICS '" + ps_raw +
                                      "' is not a declared pixel semantics"
                                      " (surface_brightness|integrated_flux): " +
                                      properties_path);
    }
    bool power_ok = false;
    if (pap_seen && !pap_raw.empty()) {
        try {
            size_t used = 0;
            const int x = std::stoi(pap_raw, &used);
            if (used != pap_raw.size()) throw std::invalid_argument("trailing chars");
            prov.pixel_area_power_present = true;
            prov.pixel_area_power = x;
            power_ok = true;
        } catch (...) {
            return reject_input(v, "P3-INPUT-PIXEL-AREA-POWER-UNPARSABLE",
                                "ACSD_PIXEL_AREA_POWER '" + pap_raw +
                                    "' is not an integer: " + properties_path);
        }
    }
    // ── ④ 声明面内部一致性（单位与像素语义/面积幂次矛盾）────────────────────
    if (prov.pixel_semantics == PixelSemantics::SurfaceBrightness && power_ok &&
        prov.pixel_area_power != -2)
        return reject_science(v, "P3-INPUT-UNIT-SEMANTICS-CONFLICT",
                              "ACSD_PIXEL_SEMANTICS=surface_brightness requires"
                              " ACSD_PIXEL_AREA_POWER=-2 (got " + pap_raw +
                                  "); contradictory input identity (禁自行猜测): " +
                                  properties_path);
    if (prov.pixel_semantics == PixelSemantics::IntegratedFlux && power_ok &&
        prov.pixel_area_power != 0)
        return reject_science(v, "P3-INPUT-UNIT-SEMANTICS-CONFLICT",
                              "ACSD_PIXEL_SEMANTICS=integrated_flux requires"
                              " ACSD_PIXEL_AREA_POWER=0 (got " + pap_raw +
                                  "); contradictory input identity: " + properties_path);

    // ── ⑤ 冻结单位表归类（唯一实现 resolve_bunit）────────────────────────────
    const BunitResolution res = resolve_bunit(v.bunit_raw, prov);
    if (!res.resolvable) {
        if (ascii_upper(strip_ws(v.bunit_raw)) == "ADU")
            return reject_input(v, "P3-INPUT-BUNIT-UNDECIDABLE",
                                "input BUNIT 'ADU' without ACSD_PIXEL_SEMANTICS="
                                "surface_brightness + ACSD_PIXEL_AREA_POWER=-2 is not"
                                " dimensionally decidable (FZ-BUNIT-SEMANTICS; 禁按 ADU"
                                " 猜测); 面亮度输入须逐字写 'ADU/sr' 或补像素语义声明: " +
                                    properties_path);
        return reject_input(v, "P3-INPUT-BUNIT-UNPARSABLE",
                            "BUNIT '" + v.bunit_raw +
                                "' is outside the frozen unit vocabulary"
                                " (p3rsmp::resolve_bunit: " + res.reason +
                                "); 合法的非 ADU 承载单位需上游科学模型裁决（未闭合项），"
                                "本轮不自行映射为 'ADU/sr': " + properties_path);
    }
    if (res.resolved.adu_power != 1 || res.resolved.px_power != -2) {
        if (prov.pixel_semantics == PixelSemantics::IntegratedFlux)
            return reject_science(v, "P3-INPUT-NOT-SURFACE-BRIGHTNESS",
                                  "BUNIT '" + v.bunit_raw +
                                      "' + ACSD_PIXEL_SEMANTICS=integrated_flux resolves to '"
                                      + res.resolved.canonical() +
                                      "' (integrated flux); export accepts surface-brightness"
                                      " inputs only (docs/ACSD_DESIGN §6.3): " + properties_path);
        return reject_science(v, "P3-INPUT-NOT-SURFACE-BRIGHTNESS",
                              "export accepts surface-brightness inputs only"
                              " (docs/ACSD_DESIGN §6.3): BUNIT '" + v.bunit_raw +
                                  "' resolves to '" + res.resolved.canonical() +
                                  "' (expected 'ADU/sr'): " + properties_path);
    }
    // 维度等价但使用 legacy 像元面积写法（"px"/"pixel"）：冻结口径明确否定
    // （docs/science/algorithms/export/PHASE3_RESAMPLE.md / PHASE3_RSMP_IMPL.md），
    // 生产入口不据此放行（读侧兼容 ≠ 写侧可接受）。
    if (uses_legacy_pixel_area_alias(v.bunit_raw))
        return reject_science(v, "P3-INPUT-NOT-SURFACE-BRIGHTNESS",
                              "BUNIT '" + v.bunit_raw +
                                  "' uses the legacy pixel-area spelling; the frozen"
                                  " surface-brightness contract rejects 'ADU/px^2' and"
                                  " 'ADU/pixel^2' (write the canonical 'ADU/sr'): " +
                                  properties_path);

    // ── ⑥ 显式面亮度身份 ↔ 已声明像素语义/面积幂次 矛盾 ────────────────────
    if (prov.pixel_semantics == PixelSemantics::IntegratedFlux)
        return reject_science(v, "P3-INPUT-UNIT-SEMANTICS-CONFLICT",
                              "BUNIT '" + v.bunit_raw +
                                  "' is an explicit surface-brightness unit but"
                                  " ACSD_PIXEL_SEMANTICS=integrated_flux contradicts it"
                                  " (禁自行猜测): " + properties_path);
    if (power_ok && prov.pixel_area_power != -2)
        return reject_science(v, "P3-INPUT-UNIT-SEMANTICS-CONFLICT",
                              "BUNIT '" + v.bunit_raw +
                                  "' is an explicit surface-brightness unit but"
                                  " ACSD_PIXEL_AREA_POWER=" + pap_raw +
                                  " contradicts it: " + properties_path);

    // ── 通过: 身份 = signal 面亮度; 产品串取冻结 canonical ───────────────────
    v.accepted = true;
    v.input_error = false;
    v.bunit_canonical = kP3SignalBunitCanonical;
    return v;
}

}  // namespace acsd::phase3
