#!/usr/bin/env python3
"""SCHEMA-INTEGRATE-001 / Wave 6 —— 从 v6 proposal 机械迁移生产 schema。

本生成器只做**确定性结构迁移**，不改变任何冻结语义（单位/公式/枚举/容差）：
  1. 原样保留 $schema / $id / 全部 properties / $defs / x-astrocs 锚；
  2. title 去掉 "(DATA-DESIGN-001 提案)" 并标注生产集成；
  3. description 前缀生产集成头（来源 proposal + 冻结表锚）；
  4. 注入 x-astrocs-production 块（task / wave / 来源 / 冻结条款 id / 状态）；
  5. 词表归一：删除 SCI-PSFW/SCI-P2 双词表的"别名"桥接字段，单一权威词表落
     eng/contracts/data/v6_weight_vocabulary_v1.json，双向映射落
     eng/contracts/data/v6_migration_map_v1.json；schema 只认 canonical。
  6. psfsw concentration 单位按唯一权威 = component_flux_unit/px^2 收紧
     （ALG-P2-PSFSW-001 §…；Phase1 ALG §4.2 的 "ADU/px" 登记为 W12 待修文本错误，
     不在生产 schema 的合法域内）。
  7. 结构可判门（if/then/anyOf/not/propertyNames）：
       - BUNIT 量纲可判（provenance.units.bunit=ADU ⇒ pixel_semantics=surface_brightness
         且 pixel_area_power=-2；bunit 含 /px^2 ⇒ pixel_area_power=-2）；
       - signal pixel_semantics ↔ pixel_area_power 自洽；
       - covariance 对角表示必须带相关核或可重建算子摘要；
       - weight_mode ↔ mode_class 自洽；psfsw_robust ⇒ kind/units/group_normalized canonical；
       - provenance k_corr.value != 1（k_corr=1 忽略相关 → REJECT）；
       - psfsw 产物禁止键（ivar/fisher/w_info/...）propertyNames 守卫。

用法（仓库根）: python3 eng/tests/contracts/v6/tools/gen_production_schemas.py
退出码: 0 = 全部写入；非 0 = 输入缺失/结构不符。
"""
import json, os, sys, pathlib

REPO = pathlib.Path(__file__).resolve().parents[5]
SRC = REPO / "eng" / "contracts" / "proposals" / "v6" / "data"
DST = REPO / "eng" / "contracts" / "schemas" / "v6"
FREEZE_REL = "docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json"

CLAUSES = {
    "astrocs.v6.units.v1.schema.json": [
        "FZ-UNIT-SIGNAL-SB", "FZ-UNIT-Q", "FZ-UNIT-FLUX", "FZ-UNIT-WINFO",
        "FZ-UNIT-PSFSW", "FZ-UNIT-VAR-IN", "FZ-UNIT-VAR-SB", "FZ-UNIT-IVAR-SB",
        "FZ-P3-BUNIT-QUADRATIC", "FZ-BUNIT-SEMANTICS"],
    "astrocs.v6.signal.v1.schema.json": [
        "FZ-FORMULA-DRIZZLE-SB", "FZ-GATE-CONST-SB", "FZ-COND-FLUX-CONSERV",
        "FZ-DEGRADE-SCALAR", "FZ-BUNIT-SEMANTICS"],
    "astrocs.v6.covariance.v1.schema.json": [
        "FZ-FORMULA-COV-PROP", "FZ-FORMULA-GLS", "FZ-GATE-PIXIVAR-APPROX",
        "FZ-GATE-PARENT-VAR", "FZ-PROV-SHARED-SYSTEMATIC", "FZ-GATE-PSFSW-COV"],
    "astrocs.v6.psf.v1.schema.json": [
        "FZ-COND-WHITENOISE", "FZ-GATE-MEDIAN-SNR", "FZ-GATE-SUPPORT-COVERAGE",
        "FZ-DEGRADE-SCALAR"],
    "astrocs.v6.effective-psf.v1.schema.json": [
        "FZ-GATE-PSFSW-EPSF", "FZ-P3-FAILCLOSED"],
    "astrocs.v6.point-information.v1.schema.json": [
        "FZ-FORMULA-WINFO", "FZ-FORMULA-Q", "FZ-FORMULA-FHAT", "FZ-UNIT-WINFO",
        "FZ-COND-WHITENOISE", "FZ-DEGRADE-SCALAR"],
    "astrocs.v6.weight-mode.v1.schema.json": [
        "FZ-MODE-PRODUCTION", "FZ-MODE-BASELINE", "FZ-MODE-DEFERRED",
        "FZ-FIELD-WEIGHTMODE", "FZ-GATE-MEDIAN-SNR", "FZ-GATE-SUPPORT-COVERAGE"],
    "astrocs.v6.psfsw.v1.schema.json": [
        "FZ-FIELD-PSFSW-4COMP", "FZ-FIELD-PSFSW-UNIT", "FZ-FORMULA-PSFSW-COMPOSITE",
        "FZ-GATE-PSFSW-FAILCLOSED", "FZ-GATE-PSFSW-COV", "FZ-GATE-PSFSW-EPSF"],
    "astrocs.v6.provenance.v1.schema.json": [
        "FZ-PROV-MINIMAL-SET", "FZ-PROV-SHARED-SYSTEMATIC", "FZ-PROV-KCORR",
        "FZ-DEGRADE-SCALAR", "FZ-BUNIT-SEMANTICS", "FZ-COND-FLUX-CONSERV"],
    "astrocs.v6.phase3.v1.schema.json": [
        "FZ-P3-MODES", "FZ-P3-FAILCLOSED", "FZ-P3-QW-RECOMPUTE",
        "FZ-P3-KERNEL-REGISTRY", "FZ-P3-BUNIT-QUADRATIC", "FZ-FORMULA-COV-PROP"],
}

HEAD = ("[生产 schema | SCHEMA-INTEGRATE-001/W6] 由 eng/contracts/proposals/v6/data/{f} 机械迁移；"
        "语义权威 = " + FREEZE_REL + "（条款 id 见 x-astrocs-production.clause_ids）。"
        "词表归一：weight.kind / weight.units / group_normalized / normalization.scope 为唯一 canonical 名，"
        "legacy 别名与双向映射见 eng/contracts/data/v6_weight_vocabulary_v1.json 与 v6_migration_map_v1.json。"
        "PENDING_OWNER_SIGNOFF 条款生效前 fail-closed，不得放宽。原描述：")

FORBIDDEN_PSFSW_KEYS = ["ivar", "inverse_variance", "variance", "var", "sigma",
                        "sigma2", "fisher", "fisher_information", "information",
                        "w_info", "w_psf"]


def prod_note(fname, src_desc):
    return HEAD.format(f=fname) + src_desc


PENDING_STRUCTURAL = {
    "astrocs.v6.psfsw.v1.schema.json": ["PSFSW-T-NMIN"],
    "astrocs.v6.provenance.v1.schema.json": ["FZ-PROV-KCORR-VALUE"],
}


def production_block(fname):
    return {
        "task": "SCHEMA-INTEGRATE-001",
        "wave": 6,
        "status": "PRODUCTION_INTEGRATED",
        "source_proposal": "eng/contracts/proposals/v6/data/" + fname,
        "freeze_contract": FREEZE_REL,
        "clause_ids": CLAUSES[fname],
        "pending_owner_signoff_structural_refs": PENDING_STRUCTURAL.get(fname, []),
        "signoff_policy": "PENDING_OWNER_SIGNOFF 条款保持待签并 fail-closed；不得写成已冻结",
    }


def load(fname):
    with open(SRC / fname, encoding="utf-8") as f:
        return json.load(f)


def write(fname, doc):
    DST.mkdir(parents=True, exist_ok=True)
    out = DST / fname
    with open(out, "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, indent=2)
        f.write("\n")
    return out


def base(fname):
    d = load(fname)
    d["title"] = d["title"].replace("（DATA-DESIGN-001 提案）", "（生产，W6 集成）")
    d["description"] = prod_note(fname, d.get("description", ""))
    d["x-astrocs-production"] = production_block(fname)
    return d


def migrate_units(d):
    return d  # 单位表值落 eng/contracts/data/v6_data_dictionary_v1.json；schema 只约束结构


def migrate_signal(d):
    d["allOf"] = [
        {"if": {"properties": {"pixel_semantics": {"const": "surface_brightness"}}},
         "then": {"properties": {"pixel_area_power": {"const": -2}}}},
        {"if": {"properties": {"pixel_semantics": {"const": "integrated_flux"}}},
         "then": {"properties": {"pixel_area_power": {"const": 0}}}},
    ]
    return d


def migrate_covariance(d):
    d["allOf"] = [
        {"if": {"properties": {"representation": {"const": "diagonal_variance"}}},
         "then": {"anyOf": [{"required": ["correlation_kernel"]},
                            {"required": ["operator_descriptor"]}]}},
    ]
    return d


def migrate_psf(d):
    return d


def migrate_effective_psf(d):
    return d


def migrate_point_information(d):
    return d


def migrate_weight_mode(d):
    w = d["properties"]["weight"]
    for k in ("kind_alias_sci_psfw", "units_alias_sci_psfw", "normalization_scope_alias"):
        w["properties"].pop(k, None)
    d.pop("x-astrocs-token-mapping-for-W6", None)
    d["x-astrocs-legacy-vocabulary"] = {
        "note": "W6 归一前的两套既有词表已折叠为 canonical 字段；只允许迁移层接受，禁止生产层自创第三套。",
        "mapping_ref": "eng/contracts/data/v6_weight_vocabulary_v1.json",
        "migration_ref": "eng/contracts/data/v6_migration_map_v1.json",
    }
    d["allOf"] = [
        {"if": {"properties": {"weight_mode": {"enum": ["point_information", "surface_gls", "psfsw_robust"]}}},
         "then": {"properties": {"mode_class": {"const": "production"}}}},
        {"if": {"properties": {"weight_mode": {"enum": ["equal", "pixel_ivar"]}}},
         "then": {"properties": {"mode_class": {"const": "documented_baseline"}}}},
        {"if": {"properties": {"weight_mode": {"const": "psfsw_robust"}}},
         "then": {"properties": {"weight": {"properties": {
             "kind": {"const": "psfsw_robust_weight"},
             "units": {"const": "1"},
             "group_normalized": {"const": True}}}}}},
    ]
    return d


CONC_PATTERN = "^[A-Za-z][A-Za-z0-9_()^\\-]*/px\\^2$"


def migrate_psfsw(d):
    props = d["properties"]
    props["component_flux_unit"] = {
        "type": "string", "minLength": 1, "maxLength": 32,
        "x-astrocs": {
            "anchor": "ALG-P2-PSFSW-001 四分量单位一致性规则; FZ-FIELD-PSFSW-4COMP",
            "domain": "Phase2 psfsw_robust",
            "fail_closed": "S_k/N_k/B_k 必须共享组内常量 component_flux_unit；未声明或组内不一致 → REJECT",
            "gate": "G-PSFSW-4COMP-UNITS"}}
    d["required"] = list(d["required"])
    if "component_flux_unit" not in d["required"]:
        d["required"].append("component_flux_unit")
    wprops = props["weight"]["properties"]
    for k in ("kind_alias_sci_psfw", "units_alias_sci_psfw", "normalization_scope_alias"):
        wprops.pop(k, None)
    comps = props["components"]["properties"]
    comps["concentration"] = {
        "allOf": [{"$ref": "#/$defs/component"},
                  {"properties": {"units": {"pattern": CONC_PATTERN}}}],
        "x-astrocs": {
            "anchor": "ALG-P2-PSFSW-001 单位一致性规则（Conc_k = component_flux_unit/px^2）; FZ-FIELD-PSFSW-4COMP; A_NEA=px^2",
            "domain": "全部",
            "fail_closed": "concentration.units != component_flux_unit + '/px^2'（含 legacy 文本错误 'ADU/px'）→ REJECT；禁 FWHM/拟合残差代替",
            "gate": "G-PSFSW-4COMP-UNITS"},
    }
    for name in ("signal", "noise", "background"):
        comps[name]["x-astrocs"]["fail_closed"] = (
            "分量单位必须等于 component_flux_unit（组内常量）；不等或含 flux^-2/ivar → REJECT")
    d["properties"]["common_star_set"]["properties"]["n_common"].setdefault("x-astrocs", {})["signoff"] = (
        "PENDING_OWNER_SIGNOFF: PSFSW-T-NMIN / SO-07；生效前 fail-closed（<3 即 unavailable(insufficient_valid_stars)），不得写成已冻结")
    d["propertyNames"] = {"not": {"enum": FORBIDDEN_PSFSW_KEYS}}
    w = props["weight"]
    w["propertyNames"] = {"not": {"enum": FORBIDDEN_PSFSW_KEYS}}
    d["x-astrocs-concentration-unit-authority"] = {
        "authoritative": "component_flux_unit/px^2",
        "a_nea_unit": "px^2",
        "anchor": "ALG-P2-PSFSW-001 单位一致性规则",
        "registered_text_error": {
            "file": "docs/algorithms/v6/phase1/ALG_P1_001_PHASE1_ALGORITHM_SPEC.md",
            "section": "§4.2 四分量表",
            "as_written": "ADU/px",
            "disposition": "W12 待修文本错误；生产 schema 合法域不含 'ADU/px'（DOC-CONVERGE-001 修正，须负责人签字后方可改 FROZEN 正文）",
        },
    }
    return d


def migrate_provenance(d):
    uprops = d["properties"]["units"]["properties"]
    uprops["pixel_area_power"]["allOf"] = [{"not": {"const": None}}]
    d["properties"]["k_corr"]["properties"]["value"] = {
        "type": "number", "exclusiveMinimum": 0, "not": {"const": 1},
        "x-astrocs": {
            "anchor": "FZ-PROV-KCORR; ALG-P2-SURF-UPM §6",
            "domain": "Phase2 UPM control_variance",
            "fail_closed": "k_corr=1 忽略相关 → REJECT；跨域外推 → REJECT",
            "signoff": "PENDING_OWNER_SIGNOFF: FZ-PROV-KCORR-VALUE / SO-07；生效前 fail-closed",
            "gate": "G-KCORR-DOMAIN"}}
    d["allOf"] = [
        {"if": {"required": ["units"], "properties": {"units": {"properties": {"bunit": {"const": "ADU"}}}}},
         "then": {"properties": {"units": {"properties": {
             "pixel_semantics": {"const": "surface_brightness"},
             "pixel_area_power": {"const": -2}}}}}},
        {"if": {"properties": {"units": {"properties": {"bunit": {"pattern": "/px\\^2$"}}}}},
         "then": {"properties": {"units": {"properties": {"pixel_area_power": {"const": -2}}}}}},
    ]
    return d


def migrate_phase3(d):
    return d


MIGRATORS = {
    "astrocs.v6.units.v1.schema.json": migrate_units,
    "astrocs.v6.signal.v1.schema.json": migrate_signal,
    "astrocs.v6.covariance.v1.schema.json": migrate_covariance,
    "astrocs.v6.psf.v1.schema.json": migrate_psf,
    "astrocs.v6.effective-psf.v1.schema.json": migrate_effective_psf,
    "astrocs.v6.point-information.v1.schema.json": migrate_point_information,
    "astrocs.v6.weight-mode.v1.schema.json": migrate_weight_mode,
    "astrocs.v6.psfsw.v1.schema.json": migrate_psfsw,
    "astrocs.v6.provenance.v1.schema.json": migrate_provenance,
    "astrocs.v6.phase3.v1.schema.json": migrate_phase3,
}


def main():
    missing = [f for f in MIGRATORS if not (SRC / f).is_file()]
    if missing:
        print("GEN_FAIL missing proposals:", missing)
        return 2
    for fname, fn in MIGRATORS.items():
        d = fn(base(fname))
        write(fname, d)
        print("wrote", DST / fname)
    print("GEN_PASS", len(MIGRATORS), "schemas")
    return 0


if __name__ == "__main__":
    sys.exit(main())
