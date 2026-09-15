#!/usr/bin/env python3
"""SCHEMA-INTEGRATE-001 / W6 —— 生产数据字典（contracts/data/v6_data_dictionary_v1.json）。

科学表（单位/权重模式/禁止项/provenance 最小集/条款锚）在本脚本中**独立手写**，
冻结表（clause 注册表/状态计数/签字项）从 W4 冻结表机械读取；两者由
tests/contracts/v6/v6_oracle.py 独立对拍 —— 生产字典与冻结合同的任何漂移都会判红。
"""
import json, pathlib, sys

REPO = pathlib.Path(__file__).resolve().parents[4]
FREEZE = REPO / "docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json"
OUT = REPO / "contracts/data/v6_data_dictionary_v1.json"

SCHEMA_FILES = [
    "astrocs.v6.units.v1.schema.json",
    "astrocs.v6.signal.v1.schema.json",
    "astrocs.v6.covariance.v1.schema.json",
    "astrocs.v6.psf.v1.schema.json",
    "astrocs.v6.effective-psf.v1.schema.json",
    "astrocs.v6.point-information.v1.schema.json",
    "astrocs.v6.weight-mode.v1.schema.json",
    "astrocs.v6.psfsw.v1.schema.json",
    "astrocs.v6.provenance.v1.schema.json",
    "astrocs.v6.phase3.v1.schema.json",
]

# ── 独立手写：冻结单位表镜像（8 条，逐字对齐 FZ-UNIT-* / FZ-P3-BUNIT-QUADRATIC）──
FROZEN_UNITS_TABLE = [
    {"symbol": "signal_sb", "unit": "ADU/px^2", "meaning": "Phase1 Drizzle/HiPS 面亮度 signal",
     "variance_unit": "ADU^2/px^4", "ivar_unit": "px^4/ADU^2", "clause_id": "FZ-UNIT-SIGNAL-SB"},
    {"symbol": "pixel_variance_in", "unit": "ADU^2", "meaning": "输入源像素逐像素方差 v_j",
     "variance_unit": None, "ivar_unit": None, "clause_id": "FZ-UNIT-VAR-IN"},
    {"symbol": "W_info", "unit": "ADU^-2", "meaning": "点源信息权重 = 1/Var(F_hat)",
     "variance_unit": None, "ivar_unit": None, "clause_id": "FZ-UNIT-WINFO"},
    {"symbol": "Q", "unit": "ADU^-1", "meaning": "点源线性充分统计量",
     "variance_unit": None, "ivar_unit": None, "clause_id": "FZ-UNIT-Q"},
    {"symbol": "flux", "unit": "ADU", "meaning": "F_hat 点源通量估计",
     "variance_unit": "ADU^2", "ivar_unit": "ADU^-2", "clause_id": "FZ-UNIT-FLUX"},
    {"symbol": "psfsw_robust_weight", "unit": "1", "meaning": "无量纲组内相对复合权重",
     "variance_unit": None, "ivar_unit": None, "clause_id": "FZ-UNIT-PSFSW"},
    {"symbol": "phase2_mosaic_signal", "unit": "BUNIT(声明)", "meaning": "Phase2 马赛克 signal; 面亮度产品则 ADU/px^2",
     "variance_unit": "BUNIT^2", "ivar_unit": "1/BUNIT^2", "clause_id": "FZ-UNIT-SIGNAL-SB"},
    {"symbol": "phase3_var_out", "unit": "BUNIT^2", "meaning": "Phase3 输出方差 = 主 HDU BUNIT 平方",
     "variance_unit": None, "ivar_unit": "1/BUNIT^2", "clause_id": "FZ-P3-BUNIT-QUADRATIC"},
]

# ── 独立手写：DATA-DESIGN 设计表（10 条，含 sb_variance_out/sb_ivar_out 分离命名）──
DESIGN_UNITS_TABLE = FROZEN_UNITS_TABLE[:1] + [
    {"symbol": "sb_variance_out", "unit": "ADU^2/px^4", "meaning": "Phase1 输出面亮度方差 variance_p",
     "variance_unit": None, "ivar_unit": None, "clause_id": "FZ-UNIT-VAR-SB"},
    {"symbol": "sb_ivar_out", "unit": "px^4/ADU^2", "meaning": "Phase1 输出 ivar",
     "variance_unit": None, "ivar_unit": None, "clause_id": "FZ-UNIT-IVAR-SB"},
] + FROZEN_UNITS_TABLE[1:]

# ── 独立手写：weight_mode 三面互斥 ──
WEIGHT_MODES = {
    "production": ["point_information", "surface_gls", "psfsw_robust"],
    "documented_baseline": ["equal", "pixel_ivar"],
    "deferred": ["psf_snr_power"],
    "legacy_superseded": {"0": "support_x_snr2", "1": "equal", "2": "pixel_ivar"},
    "legacy_integer_allowed": False,
    "production_forbidden_values": ["psf_snr_power", "auto", "support_x_snr2", 0],
    "mode_details": [
        {"mode": "point_information", "status": "PRODUCTION_FROZEN", "weight_object": "W_info", "units": "ADU^-2",
         "authoritative_formula": "Q_k=a_k P_k^T C_k^-1 d_k; W_info,k=a_k^2 P_k^T C_k^-1 P_k; F_hat=Q/W; Var=1/W",
         "covariance_source": "combination_coefficients", "effective_psf_required": True, "group_normalized": False,
         "clause_id": "FZ-FORMULA-WINFO"},
        {"mode": "surface_gls", "status": "PRODUCTION_FROZEN", "weight_object": "A^T C^-1 A", "units": "1/(surface_brightness^2)",
         "authoritative_formula": "x_hat=(A^T C^-1 A)^-1 A^T C^-1 d; Cov=(A^T C^-1 A)^-1",
         "covariance_source": "combination_coefficients", "effective_psf_required": True, "group_normalized": False,
         "clause_id": "FZ-FORMULA-GLS"},
        {"mode": "psfsw_robust", "status": "PRODUCTION_FROZEN", "weight_object": "psfsw_robust_weight", "units": "1",
         "authoritative_formula": "Wt_k=C_norm*S^alpha*Conc^beta/(N^gamma*B^delta); W_psfsw,k=Wt_k/median_j(Wt_j)",
         "covariance_source": "combination_coefficients", "effective_psf_required": True, "group_normalized": True,
         "clause_id": "FZ-FORMULA-PSFSW-COMPOSITE"},
        {"mode": "equal", "status": "DOCUMENTED_BASELINE", "weight_object": "unit_weight", "units": "1",
         "authoritative_formula": "I_out=mean_k d_k", "covariance_source": "combination_coefficients",
         "effective_psf_required": True, "group_normalized": False, "clause_id": "FZ-MODE-BASELINE"},
        {"mode": "pixel_ivar", "status": "DOCUMENTED_BASELINE", "weight_object": "pixel_ivar", "units": "1/BUNIT^2",
         "authoritative_formula": "I_out=Sum_k w_k d_k/Sum_k w_k, w_k=1/v_k", "covariance_source": "combination_coefficients",
         "effective_psf_required": True, "group_normalized": False, "clause_id": "FZ-MODE-BASELINE"},
        {"mode": "psf_snr_power", "status": "DEFERRED_NOT_PRODUCTION", "weight_object": "ratio_of_powers", "units": "1",
         "authoritative_formula": "DEFERRED (未冻结)", "covariance_source": "combination_coefficients",
         "effective_psf_required": True, "group_normalized": True, "clause_id": "FZ-MODE-DEFERRED",
         "controller_ref": "C-004.1"},
    ],
}

# ── 独立手写：禁止作权重来源的诊断别名（18）──
FORBIDDEN_WEIGHT_SOURCE_TOKENS = [
    "median_source_snr", "median_snr", "source_snr_median", "med_source_snr",
    "support", "support_area", "coverage", "coverage_area",
    "fwhm", "psf_fwhm", "median_fwhm", "source_fwhm",
    "residual", "psf_residual", "psf_fit_residual", "fit_residual",
    "psfsw_robust_weight", "psfsw",
]
PSFSW_FORBIDDEN_KEYS = ["ivar", "inverse_variance", "variance", "var", "sigma", "sigma2",
                        "fisher", "fisher_information", "information", "w_info", "w_psf"]
PSFSW_EXTENDED_GUARD_KEYS = ["snr", "snr2", "support", "coverage"]

DECLARED_WEIGHT_SOURCES = {
    "point_information": ["psf", "photometric_response", "noise_covariance"],
    "surface_gls": ["design_matrix", "noise_covariance", "upm_scale"],
    "psfsw_robust": ["psfsw.signal", "psfsw.concentration", "psfsw.noise", "psfsw.background"],
    "equal": ["unit_weight"],
    "pixel_ivar": ["pixel_ivar"],
}

PROVENANCE_MINIMAL_SET = [
    {"path": "product.type_id", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "product.schema_version", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "software_sha", "clause_id": "FZ-PROV-MINIMAL-SET", "constraint": "40 hex"},
    {"path": "run_id", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "input_product_hashes", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "config_hash", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "units.bunit", "clause_id": "FZ-BUNIT-SEMANTICS"},
    {"path": "units.pixel_semantics", "clause_id": "FZ-BUNIT-SEMANTICS"},
    {"path": "units.pixel_area_power", "clause_id": "FZ-BUNIT-SEMANTICS"},
    {"path": "units.target_pixel_area", "clause_id": "FZ-BUNIT-SEMANTICS"},
    {"path": "coordinate.frame", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "pixel_semantics", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "sampling", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "algorithm_ids", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "module", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "provider", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "approximations", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "degradations", "clause_id": "FZ-DEGRADE-SCALAR"},
    {"path": "normalization_version", "clause_id": "FZ-COND-FLUX-CONSERV"},
    {"path": "weight_mode_version", "clause_id": "FZ-FIELD-WEIGHTMODE"},
    {"path": "correlation_summary", "clause_id": "FZ-PROV-SHARED-SYSTEMATIC"},
    {"path": "flux_conservation_factor", "clause_id": "FZ-COND-FLUX-CONSERV"},
    {"path": "k_corr", "clause_id": "FZ-PROV-KCORR"},
    {"path": "generated_utc", "clause_id": "FZ-PROV-MINIMAL-SET"},
    {"path": "output_hash", "clause_id": "FZ-PROV-MINIMAL-SET"},
]

CONCENTRATION_UNIT_AUTHORITY = {
    "authoritative_unit": "component_flux_unit/px^2",
    "a_nea_unit": "px^2",
    "component_flux_unit": "组内常量、显式声明；S_k/N_k/B_k 共享该单位，Conc_k = component_flux_unit/px^2",
    "anchor": "ALG-P2-PSFSW-001 单位一致性规则; FZ-FIELD-PSFSW-4COMP; FZ-COND-WHITENOISE",
    "registered_text_error": {
        "file": "docs/algorithms/v6/phase1/ALG_P1_001_PHASE1_ALGORITHM_SPEC.md",
        "section": "§4.2 四分量表",
        "line": 203,
        "as_written": "ADU/px",
        "disposition": "W12 待修文本错误（DOC-CONVERGE-001 + 负责人签字后修正 FROZEN 正文）；生产 schema 与数据字典合法域**不含** 'ADU/px'",
        "not_silently_adopted": True,
    },
    "legacy_proposal_example": "contracts/proposals/v6/data/examples/psfsw.example.json 原写 'ADU/px'；生产正例 contracts/data/examples/v6/psfsw.example.json 已按唯一权威修正为 'ADU/px^2'",
}

# ── 独立手写：fail-closed 语义清单（门 → 处置）──
FAIL_CLOSED = [
    {"gate": "G-BUNIT-SEMANTICS", "clause_id": "FZ-BUNIT-SEMANTICS", "condition": "BUNIT=ADU 且 provenance 无 pixel_semantics=surface_brightness + pixel_area_power=-2", "disposition": "unavailable/REJECT"},
    {"gate": "G-PSFSW-UNIT", "clause_id": "FZ-UNIT-PSFSW", "condition": "psfsw weight.units 含 flux^-2/ivar 或 != '1'", "disposition": "REJECT"},
    {"gate": "G-PSFSW-NORMALIZATION", "clause_id": "FZ-FIELD-PSFSW-UNIT", "condition": "group_normalized=false / scope=global / median_target != 1", "disposition": "REJECT"},
    {"gate": "G-PSFSW-4COMP", "clause_id": "FZ-FIELD-PSFSW-4COMP", "condition": "四分量塌陷/缺分量/measurement_id 重复/p05>p50>p95", "disposition": "REJECT"},
    {"gate": "G-PSFSW-4COMP-UNITS", "clause_id": "FZ-FIELD-PSFSW-4COMP", "condition": "concentration.units != component_flux_unit/px^2（或 legacy 'ADU/px'）", "disposition": "REJECT"},
    {"gate": "G-PSFSW-FAILCLOSED", "clause_id": "FZ-GATE-PSFSW-FAILCLOSED", "condition": "valid=false 而 weight_value 非 null，或 reason 不在 5 项白名单，或回退 median source SNR", "disposition": "REJECT"},
    {"gate": "G-PSFSW-COV", "clause_id": "FZ-GATE-PSFSW-COV", "condition": "variance_from=psfsw_robust_weight / Var=1/W_psfsw / psfsw 禁止键", "disposition": "REJECT"},
    {"gate": "G-EPSF-PRESENT", "clause_id": "FZ-GATE-PSFSW-EPSF", "condition": "三生产模式缺 effective PSF / 只给 FWHM 标量", "disposition": "REJECT"},
    {"gate": "G-WEIGHT-SOURCES", "clause_id": "FZ-GATE-MEDIAN-SNR", "condition": "weight.sources 命中诊断别名集", "disposition": "REJECT"},
    {"gate": "G-WEIGHTMODE-ENUM", "clause_id": "FZ-MODE-PRODUCTION", "condition": "未知模式 / legacy 0 / auto / support_x_snr2 / psf_snr_power 进生产", "disposition": "REJECT"},
    {"gate": "G-COV-VARIANCE-FROM", "clause_id": "FZ-FORMULA-COV-PROP", "condition": "从权重标量/诊断量反推 variance", "disposition": "REJECT"},
    {"gate": "G-COV-CORRELATION-KERNEL", "clause_id": "FZ-PROV-SHARED-SYSTEMATIC", "condition": "只出对角 variance 而无相关核/可重建算子摘要", "disposition": "REJECT"},
    {"gate": "G-PARENT-VAR-DEFICIT", "clause_id": "FZ-GATE-PARENT-VAR", "condition": "HiPS 父级对角归约声明为精确", "disposition": "REJECT/unavailable"},
    {"gate": "G-KCORR-DOMAIN", "clause_id": "FZ-PROV-KCORR", "condition": "k_corr=1 忽略相关 / 缺适用域 / 跨域外推", "disposition": "REJECT"},
    {"gate": "G-KCORR-CALIBRATION", "clause_id": "FZ-PROV-KCORR", "condition": "缺标定脚本/固定种子", "disposition": "REJECT"},
    {"gate": "G-PROV-MINIMAL-SET", "clause_id": "FZ-PROV-MINIMAL-SET", "condition": "provenance 缺最小集键", "disposition": "REJECT"},
    {"gate": "G-UNAVAILABLE-REASON", "clause_id": "FZ-PROV-MINIMAL-SET", "condition": "unavailable 无 reason/scope 或占位键", "disposition": "REJECT"},
    {"gate": "G-DEGRADE-SCALAR", "clause_id": "FZ-DEGRADE-SCALAR", "condition": "标量降级缺 p05/p50/p95/双门", "disposition": "REJECT"},
    {"gate": "G-P3-KERNEL-REGISTRY", "clause_id": "FZ-P3-KERNEL-REGISTRY", "condition": "未注册核进生产 / registered 无 Oracle", "disposition": "REJECT"},
    {"gate": "G-P3-QW-RECOMPUTE", "clause_id": "FZ-P3-QW-RECOMPUTE", "condition": "重采样输入 Q/W 代替输出帧重算", "disposition": "REJECT"},
    {"gate": "G-P3-VISUALIZATION", "clause_id": "FZ-P3-FAILCLOSED", "condition": "visualization 且 measurement_capable=true", "disposition": "REJECT"},
]

OPEN_REGISTRY = {
    "controller_refs": ["C-004.1", "C-004.2", "C-004.3", "C-004.4", "C-004.5", "C-004.6", "C-006", "C-007"],
    "design_open_items": [
        "DI-01", "DI-02", "DI-03", "DI-04", "DI-05", "DI-06", "DI-07",
        "OI-01", "OI-02", "OI-03", "OI-04", "OI-05",
        "OPEN-P2S-01", "OPEN-P2S-02", "OPEN-P2S-03", "P3-OPEN-EPSILON-CORR",
        "PF-01", "PF-02", "PF-03", "PF-04", "PF-05", "PF-06", "PF-07",
        "AR-032-GAP", "AR-034-GAP", "AR-035-GAP", "AR-036-SIGNOFF"],
    "w6_closed": ["DI-01", "DI-07"],
    "w6_carried": ["DI-06"],
    "note": "DI-01（词表归一）与 DI-07（weight_units '1' vs dimensionless_relative 双射）由本任务闭合；DI-06（plane 枚举与 runtime validator 同提交）因 runtime 不在 write_scope 保持 OPEN。",
}


def build():
    freeze = json.loads(FREEZE.read_text(encoding="utf-8"))
    schema_index = []
    for fn in SCHEMA_FILES:
        p = REPO / "contracts" / "schemas" / "v6" / fn
        d = json.loads(p.read_text(encoding="utf-8"))
        schema_index.append({
            "schema_id": d["$id"],
            "path": "contracts/schemas/v6/" + fn,
            "version": 1,
            "clause_ids": d["x-astrocs-production"]["clause_ids"],
            "source_proposal": d["x-astrocs-production"]["source_proposal"],
        })
    examples = sorted(p.name for p in (REPO / "contracts" / "data" / "examples" / "v6").glob("*.json"))
    clauses = freeze["clauses"]
    by_status = {}
    for c in clauses:
        by_status.setdefault(c["status"], []).append(c["id"])
    doc = {
        "dictionary_schema": "astrocs.v6.data-dictionary/v1",
        "schema_version": 1,
        "task": "SCHEMA-INTEGRATE-001",
        "wave": 6,
        "status": "PRODUCTION_INTEGRATED",
        "baseline_head": "ac04289dea9d3ccb3dad8310dade53e75162447f",
        "freeze_contract": "docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json",
        "freeze_baseline_head": freeze["baseline_head"],
        "semantic_authority": freeze["semantic_source"],
        "controller_authority": "工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/CONTROLLER_LOG.md",
        "schema_index": schema_index,
        "examples_index": ["contracts/data/examples/v6/" + e for e in examples],
        "frozen_units_table": FROZEN_UNITS_TABLE,
        "design_units_table": DESIGN_UNITS_TABLE,
        "quadratic_law": {
            "statement": "variance = signal^2; ivar = 1/variance; Phase3 variance BUNIT = (main HDU signal BUNIT)^2",
            "freeze_id": "FZ-P3-BUNIT-QUADRATIC"},
        "bunit_semantics": {
            "rule": "BUNIT 必须量纲可判: (a) 显式含 px 幂次(canonical ADU/px^2 与 ADU^2/px^4); 或 (b) BUNIT=ADU 时 provenance 必须声明 pixel_semantics=surface_brightness + pixel_area_power=-2 + 目标像素面积",
            "freeze_id": "FZ-BUNIT-SEMANTICS",
            "pixel_area_power_defaults": {"signal_sb": -2, "sb_variance_out": -4, "sb_ivar_out": 4,
                                          "flux": 0, "Q": 0, "W_info": 0, "psfsw_robust_weight": 0},
            "judgeable_units": ["ADU/px^2", "ADU^2/px^4", "px^4/ADU^2"]},
        "weight_modes": WEIGHT_MODES,
        "weight_vocabulary_ref": "contracts/data/v6_weight_vocabulary_v1.json",
        "migration_map_ref": "contracts/data/v6_migration_map_v1.json",
        "forbidden": {
            "weight_source_tokens": FORBIDDEN_WEIGHT_SOURCE_TOKENS,
            "psfsw_forbidden_keys": PSFSW_FORBIDDEN_KEYS,
            "psfsw_extended_guard_keys": PSFSW_EXTENDED_GUARD_KEYS,
            "production_weight_mode_forbidden_values": WEIGHT_MODES["production_forbidden_values"],
        },
        "declared_weight_sources": DECLARED_WEIGHT_SOURCES,
        "provenance_minimal_set": PROVENANCE_MINIMAL_SET,
        "concentration_unit_authority": CONCENTRATION_UNIT_AUTHORITY,
        "fail_closed": FAIL_CLOSED,
        "open_registry": OPEN_REGISTRY,
        "clause_registry": {
            "total": len(clauses),
            "counts_by_status": {k: len(v) for k, v in sorted(by_status.items())},
            "ids_by_status": {k: sorted(v) for k, v in sorted(by_status.items())},
            "signoff_items": freeze["signoff_items"],
            "controller_only_items": freeze["controller_only_items"],
            "superseded_sections": [s["id"] for s in freeze["superseded_sections"]],
            "policy": "PENDING_OWNER_SIGNOFF 条款保持 pending 并 fail-closed，任何生产 artifact 不得将其写成 FROZEN。",
        },
        "boundary": {
            "no_commit": True, "no_push": True,
            "write_scope": ["contracts/schemas/", "contracts/data/", "docs/contracts/", "tests/contracts/v6/"],
            "not_reintroduced": ["P33-COEF §12.2 snr_coefficient 落位段", "P27 生产路径不消费的 IpvParams 字段节"],
            "frozen_not_modified": ["docs/science/**", "docs/owner/**", "docs/design/**", "docs/references/**"],
        },
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(doc, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print("DICT_PASS", OUT)
    return 0


if __name__ == "__main__":
    sys.exit(build())
