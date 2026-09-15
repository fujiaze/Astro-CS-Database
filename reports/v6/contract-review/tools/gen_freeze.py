#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CONTRACT-FREEZE-001 生成器：从 freeze_source.json + adjudications.json 生成机器冻结表与人读合同文档。
写域：docs/science/v6/frozen/、docs/contracts/v6/frozen/、docs/algorithms/v6/frozen/、reports/v6/contract-review/。"""
import json, os, sys, subprocess

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))

def load(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
        return json.load(f)

ADJ = load("reports/v6/science-adjudication/adjudications.json")
SRC = load("reports/v6/contract-review/tools/freeze_source.json")
CAT = load("contracts/proposals/v6/data/astrocs.v6.data-design-catalog.v1.json")
HEAD = subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()

ft = ADJ["freeze_table"]
required = set(ADJ["required_freeze_ids"])
missing = [e["id"] for e in ft if e["id"] not in SRC["fz_contract"]]
if missing:
    sys.exit("FATAL: freeze_source.fz_contract missing: " + ",".join(missing))

clauses = []
for e in ft:
    c = SRC["fz_contract"][e["id"]]
    so = SRC["signoff_map"].get(e["id"])
    clauses.append({
        "id": e["id"], "kind": e["kind"], "layer": c["layer"],
        "subject": e["subject"], "value": e["value"], "scope": e["scope"],
        "anchor": e["anchor"], "gate": e["gate"],
        "fail_closed": c["fail_closed"], "negative_mutation": c["negative_mutation"],
        "status": "PENDING_OWNER_SIGNOFF" if so else "FROZEN",
        "owner_signoff": so["so"] if so else None,
        "signoff_reason": so["reason"] if so else None,
        "required_freeze_id": e["id"] in required,
    })
for t in SRC["numeric_thresholds"]:
    clauses.append({
        "id": t["id"], "kind": "threshold", "layer": "algorithm",
        "subject": t["subject"], "value": t["value"], "value_display": t["value_display"],
        "unit": t["unit"], "scope": t["scope"], "gate": t["gate"],
        "fail_closed": t["fail_closed"], "negative_mutation": t["negative_mutation"],
        "status": t["status"], "owner_signoff": t["owner_signoff"],
        "source_binding": t.get("source_binding"), "anchor": t["anchor"],
        "required_freeze_id": False,
    })

machine = {
    "freeze_schema": "astrocs.v6.contract-freeze/v1",
    "freeze_id": "CONTRACT-FREEZE-001",
    "task": "工程控制/AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915/tasks/CONTRACT-FREEZE-001.md",
    "wave": 4, "baseline_head": HEAD,
    "semantic_source": "reports/v6/science-adjudication/adjudications.json",
    "status_vocabulary": {
        "FROZEN": "本任务(W4)冻结的 V6 目标态合同条款，无外部签字依赖，可实施",
        "PENDING_OWNER_SIGNOFF": "条款文本与数值唯一确定，但涉及 FROZEN 非 v6 SCI 修订/数值确认，须负责人按 SO-xx 签字后方可作为正式修订生效；生效前实现不得放宽、相应面 fail-closed",
        "OPEN": "未落定，登记 owner（DI/OI/pending_freeze/epsilon_corr）",
    },
    "units_table": ADJ["units_table"],
    "weight_modes": {
        "production": ADJ["production_weight_modes"],
        "documented_baseline": ADJ["documented_baseline_modes"],
        "deferred": ADJ["deferred_modes"],
        "legacy_superseded": {"0": "support_x_snr2", "1": "equal", "2": "pixel_ivar"},
        "legacy_integer_allowed": False,
        "mode_details": ADJ["modes"],
    },
    "forbidden": {
        "weight_source_tokens": ADJ["forbidden_weight_source_tokens"],
        "psfsw_forbidden_keys": CAT["forbidden"]["psfsw_forbidden_keys"],
        "psfsw_extended_guard_keys": CAT["forbidden"]["psfsw_extended_guard_keys"],
        "production_weight_mode_forbidden_values": ["psf_snr_power", "auto", "support_x_snr2", 0],
    },
    "declared_weight_sources": {
        "point_information": ["psf", "photometric_response", "noise_covariance"],
        "surface_gls": ["design_matrix", "noise_covariance", "upm_scale"],
        "psfsw_robust": ["psfsw.signal", "psfsw.concentration", "psfsw.noise", "psfsw.background"],
        "equal": ["unit_weight"],
        "pixel_ivar": ["pixel_ivar"],
    },
    "clauses": clauses,
    "required_freeze_ids": list(ADJ["required_freeze_ids"]),
    "all_sci_adj_freeze_ids": [e["id"] for e in ft],
    "signoff_items": [dict(s, status="PENDING_OWNER_SIGNOFF") for s in ADJ["signoff_items"]],
    "controller_only_items": ADJ["controller_only_items"],
    "open_items": SRC["open_items"],
    "superseded_sections": SRC["superseded_sections"],
    "counts": {
        "clauses_total": len(clauses),
        "frozen": sum(1 for c in clauses if c["status"] == "FROZEN"),
        "pending_owner_signoff": sum(1 for c in clauses if c["status"] == "PENDING_OWNER_SIGNOFF"),
        "open": sum(1 for c in clauses if c["status"] == "OPEN"),
        "sci_adj_freeze_ids": len(ft),
        "required_freeze_ids": len(ADJ["required_freeze_ids"]),
        "numeric_thresholds": sum(1 for c in clauses if c["kind"] == "threshold"),
        "open_items_registered": len(SRC["open_items"]),
        "signoff_items": len(ADJ["signoff_items"]),
        "superseded_sections": len(SRC["superseded_sections"]),
    },
}

def w(path, text):
    p = os.path.join(ROOT, path)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    with open(p, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote", path)

mpath = "docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json"
w(mpath, json.dumps(machine, ensure_ascii=False, indent=1) + "\n")

def esc(s):
    return str(s).replace("|", "\\|").replace("\n", " ")

HDR = "> 由 `reports/v6/contract-review/tools/gen_freeze.py` 机械渲染，与 `docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json` 同源；语义源 = `reports/v6/science-adjudication/adjudications.json` + W3 各规格；基线 HEAD = `" + HEAD + "`。"

def clause_rows(sel):
    out = ["| 条款 id | 主题 | 冻结值 | 适用域 | 来源锚 | 验证门 | fail-closed 语义 | 负向 mutation | 状态 | 签字 |",
           "|---|---|---|---|---|---|---|---|---|---|"]
    for c in sel:
        out.append("| " + " | ".join([esc(c["id"]), esc(c["subject"]), esc(c.get("value_display", c.get("value", ""))),
            esc(c.get("scope", "")), esc(c.get("anchor", "")), esc(c.get("gate", "")),
            esc(c.get("fail_closed", "")), esc(c.get("negative_mutation", "")),
            c["status"], c.get("owner_signoff") or "-"]) + " |")
    return out

def units_md():
    out = ["| 符号 | 单位 | 含义 | 方差单位 | ivar 单位 |", "|---|---|---|---|---|"]
    for u in machine["units_table"]:
        out.append("| " + " | ".join([esc(u["symbol"]), esc(u["unit"]), esc(u["meaning"]), esc(u.get("variance_unit")), esc(u.get("ivar_unit"))]) + " |")
    return out

def modes_md():
    out = ["| mode | 状态 | 权重对象 | 单位 | 权威式 | covariance 来源 | effective PSF | 组内归一 | 禁止声明 |", "|---|---|---|---|---|---|---|---|---|"]
    for m in machine["weight_modes"]["mode_details"]:
        out.append("| " + " | ".join([esc(m["mode"]), esc(m["status"]), esc(m["weight_object"]), esc(m["units"]),
            "`" + esc(m["authoritative_formula"]) + "`", esc(m["covariance_source"]),
            esc(m["effective_psf_required"]), esc(m["group_normalized"]), esc("; ".join(m["forbidden_claims"]))]) + " |")
    return out

sci = [c for c in clauses if c["layer"] == "science"]
data = [c for c in clauses if c["layer"] == "data"]
thr = [c for c in clauses if c["kind"] == "threshold"]

# docs/science/v6/frozen/00_README.md
L = [HDR, "", "## 0. 权威与状态", "",
 "- 文档 ID：`CONTRACT-FREEZE-001-FROZEN-SCIENCE`；状态 = V6 目标态语义冻结（三档：FROZEN / PENDING_OWNER_SIGNOFF / OPEN）。",
 "- 权威分层：冻结宪章 > PROJECT_SPEC > PHASE{1,2,3} 详细设计 > UNIFIED / PSF_SIGNAL_WEIGHT > 本冻结合同 > ALG/DATA/API > 代码/测试 > 历史文档。",
 "- 语义继承 SCI-ADJ-001（42 条 `FZ-*`）；数值阈值继承 W3 各 ALG 规格（见 `docs/algorithms/v6/frozen/`）。",
 "- 未签字项（SO-01..07）一律标 `PENDING_OWNER_SIGNOFF`，不得写成已冻结；实现不得放宽、相应面 fail-closed。",
 "", "## 1. 冻结单位表（`FZ-UNIT-*`）", ""] + units_md() + [
 "", "二次律：`variance=signal^2`、`ivar=1/variance`、`W_info=signal^-2`、`psfsw_robust_weight=1`；Phase3 variance BUNIT=(signal BUNIT)^2。",
 "", "## 2. 冻结 mode 表", ""] + modes_md() + [
 "", "- 生产 = {point_information, surface_gls, psfsw_robust}；基线 = {equal, pixel_ivar}；延迟 = psf_snr_power（C-004.1 不解冻）。",
 "", "## 3. science 层冻结条款", ""] + clause_rows(sci) + [
 "", "## 4. 签字/取代/开放", "",
 "- 需负责人签字（保持待签）：SO-01..SO-07，见 `docs/science/v6/frozen/02_SIGNOFF_CONTROLLER_SUPERSEDED.md`。",
 "- 被取代非 v6 SCI 段：`reports/v6/contract-review/03_SUPERSEDED_SCI_SECTIONS.md`（只登记，不改写）。",
 "- 开放项：`reports/v6/contract-review/04_OPEN_ITEMS_AND_SIGNOFF.md`。"]
w("docs/science/v6/frozen/00_README.md", "\n".join(L) + "\n")

# docs/science/v6/frozen/01_SEMANTIC_FREEZE.md
L = [HDR, "",
 "本页逐条列示全部 42 条 SCI-ADJ-001 语义冻结条目在 W4 的合同化结果：条款 id、冻结值、来源锚、适用域、fail-closed 语义、验证门、负向 mutation、状态、签字归属。",
 "", "| 条款 id | 主题 | 冻结值 | 适用域 | 来源锚 | 验证门 | fail-closed 语义 | 负向 mutation | 状态 | 签字 |",
 "|---|---|---|---|---|---|---|---|---|---|"]
for c in [x for x in clauses if x["kind"] != "threshold"]:
    L.append("| " + " | ".join([esc(c["id"]), esc(c["subject"]), esc(c.get("value_display", c.get("value", ""))),
        esc(c.get("scope", "")), esc(c.get("anchor", "")), esc(c.get("gate", "")),
        esc(c.get("fail_closed", "")), esc(c.get("negative_mutation", "")), c["status"], c.get("owner_signoff") or "-"]) + " |")
w("docs/science/v6/frozen/01_SEMANTIC_FREEZE.md", "\n".join(L) + "\n")

# docs/science/v6/frozen/02_SIGNOFF_CONTROLLER_SUPERSEDED.md
L = [HDR, "", "## 1. 需负责人签字项（PENDING_OWNER_SIGNOFF，不得写成已冻结）", "",
 "| ID | 事项 | 理由 | owner | 权威 | 状态 |", "|---|---|---|---|---|---|"]
for s in machine["signoff_items"]:
    L.append("| " + " | ".join([esc(s["id"]), esc(s["topic"]), esc(s["reason"]), esc(s["owner"]), esc(s["authority"]), "PENDING_OWNER_SIGNOFF"]) + " |")
L += ["", "## 2. 控制器级事项（只登记不裁决）", "", "| ID | 事项 | owner | 状态 |", "|---|---|---|---|"]
for s in machine["controller_only_items"]:
    L.append("| " + " | ".join([esc(s["id"]), esc(s["topic"]), esc(s["ruling_owner"]), "registered_only"]) + " |")
L += ["", "## 3. 被取代的非 v6 SCI 段（AR-032/SO-06，只登记）", "",
 "| 编号 | 文件 | 段落 | 行 | 取代条款 | 签字 |", "|---|---|---|---|---|---|"]
for s in machine["superseded_sections"]:
    L.append("| " + " | ".join([esc(s["id"]), esc(s["file"]), esc(s["section"]), esc(s["lines"]), esc(", ".join(s["superseded_by"])), esc(s["signoff"])]) + " |")
L += ["", "完整事实与建议措辞见 `reports/v6/contract-review/03_SUPERSEDED_SCI_SECTIONS.md`。本包不得改写非 v6 `docs/science/*.md`。"]
w("docs/science/v6/frozen/02_SIGNOFF_CONTROLLER_SUPERSEDED.md", "\n".join(L) + "\n")

# docs/contracts/v6/frozen/00_README.md
L = [HDR, "", "## 0. 权威与用途", "",
 "- 文档 ID：`CONTRACT-FREEZE-001-FROZEN-DATA`；机器可读冻结表 = `astrocs.v6.contract-freeze.v1.json`（唯一事实源）。",
 "- 覆盖：信号/协方差/PSF/effective PSF/W_info/PSFSW/weight_mode/provenance 的单位、字段、枚举、适用域、fail-closed 与迁移规则。",
 "- 上游：`docs/contracts/v6/data/**`（DATA-DESIGN-001 设计提案）与 `contracts/proposals/v6/data/**`（schema proposal）；本目录为正式冻结层。",
 "- schema 词表归一仍归 SCHEMA-INTEGRATE-001(W6)：本目录只冻结语义与取值域，不写生产 schema。",
 "", "## 1. data 层冻结条款", ""] + clause_rows(data) + [
 "", "## 2. 文件索引", "",
 "| 文件 | 内容 |", "|---|---|",
 "| `astrocs.v6.contract-freeze.v1.json` | 机器冻结表（units/modes/forbidden/clauses/signoff/open/superseded/counts） |",
 "| `01_DATA_CONTRACT_FREEZE.md` | 字段/单位/BUNIT/provenance 冻结明细 |",
 "| `02_WEIGHT_MODE_VOCABULARY.md` | weight_mode 语义、禁止项、双词表映射建议 |"]
w("docs/contracts/v6/frozen/00_README.md", "\n".join(L) + "\n")

# docs/contracts/v6/frozen/01_DATA_CONTRACT_FREEZE.md
L = [HDR, "", "## 1. 冻结单位表与二次律", ""] + units_md() + [
 "", "## 2. BUNIT 语义（`FZ-BUNIT-SEMANTICS`）", "",
 "写盘 BUNIT 必须量纲可判：(a) 显式含 px 幂次（canonical `ADU/px^2` / `ADU^2/px^4`）；或 (b) BUNIT=`ADU` 时 provenance 声明 `pixel_semantics=surface_brightness` 且 `pixel_area_power=-2` 与目标像素面积。",
 "缺 (b) 的裸 ADU = 单位不可判 → unavailable/REJECT。pixel_area_power 缺省：signal_sb=-2、sb_variance_out=-4、sb_ivar_out=+4、flux/Q/W_info/psfsw=0（由二次律唯一导出）。",
 "", "## 3. data 层冻结条款", ""] + clause_rows(data) + [
 "", "## 4. provenance 最小集（`FZ-PROV-MINIMAL-SET`）", "",
 "schema/软件 SHA/run ID/输入+配置哈希/单位+pixel_area_power/frame/像素语义/算法 ID/provider/近似+降级（含 unavailable 原因）/归一版本/相关核或低秩摘要/flux_conservation_factor/k_corr 值+适用域/生成时间/输出哈希。unavailable 必须显式登记原因，禁止占位与静默缺键。"]
w("docs/contracts/v6/frozen/01_DATA_CONTRACT_FREEZE.md", "\n".join(L) + "\n")

# docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md
L = [HDR, "", "## 1. weight_mode 三面互斥（`FZ-MODE-*` / `FZ-FIELD-WEIGHTMODE`）", ""] + modes_md() + [
 "", "- 生产 = " + str(machine["weight_modes"]["production"]) + "；文档基线 = " + str(machine["weight_modes"]["documented_baseline"]) + "；延迟 = " + str(machine["weight_modes"]["deferred"]) + "。",
 "- legacy 整数 {0=support×snr², 1=equal, 2=ivar} 一律被取代，`legacy_integer_allowed=false`；0 不得进任何科学权重面。",
 "- 生产枚举禁止值：" + str(machine["forbidden"]["production_weight_mode_forbidden_values"]) + "。",
 "", "## 2. 禁止作为权重来源的诊断别名（`FZ-GATE-MEDIAN-SNR` / `FZ-GATE-SUPPORT-COVERAGE`）", "",
 "`" + ", ".join(machine["forbidden"]["weight_source_tokens"]) + "`",
 "", "任一 token 出现在 `weight.sources` / `weight_value` / `covariance.variance_from` 即 REJECT。",
 "注意：`psfsw_robust_weight` 与 `psfsw` 也在禁止来源集内——psfsw 权重不得被当作 ivar/W_info/方差来源。",
 "", "## 3. psfsw 产物禁止键（任何层命中即 REJECT，`FZ-GATE-PSFSW-COV`）", "",
 "`" + ", ".join(machine["forbidden"]["psfsw_forbidden_keys"]) + "`",
 "", "扩展守卫（登记）：`" + ", ".join(machine["forbidden"]["psfsw_extended_guard_keys"]) + "`",
 "", "## 4. 双词表映射建议（归 W6 SCHEMA-INTEGRATE-001 归一，不发明第三套）", "",
 "| 语义 | SCI-PSFW 词表 | SCI-P2 词表 |", "|---|---|---|",
 "| 权重对象种类 | `weight_kind=\"relative_dimensionless\"` | `weight.kind=\"psfsw_robust_weight\"` |",
 "| 权重单位串 | `weight_units=\"1\"` | `weight.units=\"dimensionless_relative\"` |",
 "| 归一域 | `normalization.scope=\"group\"` | `group_normalized=true` |",
 "| 组内 median 目标 | `normalization.median_target=1.0` | `group_normalized=true (median=1)` |",
 "", "## 5. 各模式声明的科学原始量（禁止诊断量）", "",
 "| mode | declared weight sources |", "|---|---|"]
for k, v in machine["declared_weight_sources"].items():
    L.append("| " + k + " | " + ", ".join(v) + " |")
w("docs/contracts/v6/frozen/02_WEIGHT_MODE_VOCABULARY.md", "\n".join(L) + "\n")

# docs/algorithms/v6/frozen/00_README.md
L = [HDR, "", "## 0. 权威与用途", "",
 "- 文档 ID：`CONTRACT-FREEZE-001-FROZEN-ALG`；把 SCI-ADJ-001 语义冻结的 W3 数值阈值收敛为可实施合同。",
 "- 上游：`docs/algorithms/v6/{phase1,phase2-point,phase2-psfsw,phase2-surface,phase3}/**`、`docs/validation/v6/**`、`reports/v6/qa-design/**`。",
 "- 每条给唯一数值或明确 OPEN+pending owner；未签字项标 PENDING_OWNER_SIGNOFF，不得写成已冻结。",
 "- 本目录不修改任何既有 FZ-* 数值/容差；继承项（ALG-REJ-001/UPM_SOLVER）标 FROZEN(继承)。",
 "", "## 1. 文件索引", "", "| 文件 | 内容 |", "|---|---|",
 "| `01_NUMERIC_THRESHOLD_FREEZE.md` | 全部数值阈值条款（值/单位/状态/owner/source binding/负向 mutation） |",
 "| `02_GATE_AND_MUTATION_FREEZE.md` | 验证门/负向 mutation 冻结索引与 fail-closed 矩阵 |"]
w("docs/algorithms/v6/frozen/00_README.md", "\n".join(L) + "\n")

# docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md
L = [HDR, "",
 "每条给出唯一值（或 OPEN+owner）、单位、状态、source binding（须能在 W3 规格中定位）、fail-closed 语义与负向 mutation。",
 "", "| 条款 id | 量 | 冻结值 | 单位 | 状态 | owner | 适用域 | 来源锚 | 来源绑定 | fail-closed 语义 | 负向 mutation |",
 "|---|---|---|---|---|---|---|---|---|---|---|"]
for c in thr:
    b = c.get("source_binding")
    bs = (b["file"] + " #" + b["locator"] + " ⊃ " + b["contains"]) if b else "（无数值，OPEN）"
    val = c.get("value_display", c.get("value"))
    if val is None:
        val = "PENDING（无数值）"
    L.append("| " + " | ".join([esc(c["id"]), esc(c["subject"]), esc(val), esc(c.get("unit", "")), c["status"],
        esc(c.get("owner_signoff") or "-"), esc(c.get("scope", "")), esc(c.get("anchor", "")), esc(bs), esc(c.get("fail_closed", "")), esc(c.get("negative_mutation", ""))]) + " |")
L += ["", "## 状态统计", "",
 "| 状态 | 条数 |", "|---|---|",
 "| FROZEN | %d |" % sum(1 for c in thr if c["status"] == "FROZEN"),
 "| PENDING_OWNER_SIGNOFF | %d |" % sum(1 for c in thr if c["status"] == "PENDING_OWNER_SIGNOFF"),
 "| OPEN | %d |" % sum(1 for c in thr if c["status"] == "OPEN"),
 "", "### 需签字数值阈值 owner", ""]
seen = {}
for c in thr:
    if c["status"] == "PENDING_OWNER_SIGNOFF":
        seen.setdefault(c.get("owner_signoff") or "-", []).append(c["id"])
for k in sorted(seen):
    L.append("- `" + k + "`：" + ", ".join(seen[k]))
w("docs/algorithms/v6/frozen/01_NUMERIC_THRESHOLD_FREEZE.md", "\n".join(L) + "\n")

# docs/algorithms/v6/frozen/02_GATE_AND_MUTATION_FREEZE.md
L = [HDR, "", "## 1. fail-closed 与负向 mutation 冻结矩阵（全部条款）", "",
 "| 条款 id | 状态 | 验证门 | fail-closed | 负向 mutation |", "|---|---|---|---|---|"]
for c in clauses:
    L.append("| " + " | ".join([esc(c["id"]), c["status"], esc(c.get("gate", "")), esc(c.get("fail_closed", "")), esc(c.get("negative_mutation", ""))]) + " |")
L += ["", "## 2. 生产面硬禁止（缺省即红）", "",
 "- 生产权重枚举出现 `psf_snr_power` / `auto` / `support_x_snr2` / legacy `0`（`FZ-MODE-DEFERRED` / `FZ-FIELD-WEIGHTMODE`）。",
 "- 帧级 `median(SNR_F)` / `median_source_snr` / support / coverage / FWHM / residual 进权重来源或方差来源（`FZ-GATE-MEDIAN-SNR` / `FZ-GATE-SUPPORT-COVERAGE`，C-004.2）。",
 "- psfsw 产物出现 `ivar/variance/fisher/w_info/sigma` 等键（`FZ-GATE-PSFSW-COV`）。",
 "- 由权重标量反推 variance 或 `C_out≠R C_in R^T`（`FZ-FORMULA-COV-PROP`）。",
 "- Phase3 重采样输入 Q/W、W=ΣW_in、替换上游 W_info（`FZ-P3-QW-RECOMPUTE`）。",
 "", "## 3. 零用例/skip-only 即红", "",
 "任何门不得以零用例、skip-only 或同实现自证判 PASS（宪章 §14.2/§13.1；QA-MATRIX-001 ORACLE_AND_ZERO_CASE_POLICY）；本冻结合同的 Oracle 含正向控制与逐条负向 mutation。"]
w("docs/algorithms/v6/frozen/02_GATE_AND_MUTATION_FREEZE.md", "\n".join(L) + "\n")

# reports/v6/contract-review/01_FREEZE_STATUS_MATRIX.md
L = [HDR, "", "## 1. 三类状态统计（逐条见机器表 `clauses`）", "",
 "| 状态 | 条数 |", "|---|---|",
 "| FROZEN（本任务冻结，可实施） | %d |" % machine["counts"]["frozen"],
 "| PENDING_OWNER_SIGNOFF（值唯一但待负责人签字） | %d |" % machine["counts"]["pending_owner_signoff"],
 "| OPEN（未落定，登记 owner） | %d |" % machine["counts"]["open"],
 "| 合计条款 | %d |" % machine["counts"]["clauses_total"],
 "", "## 2. 待签字条款（PENDING_OWNER_SIGNOFF）", "",
 "| 条款 id | 主题 | 值 | 签字 | 原因 |", "|---|---|---|---|---|"]
for c in clauses:
    if c["status"] == "PENDING_OWNER_SIGNOFF":
        L.append("| " + " | ".join([esc(c["id"]), esc(c["subject"]), esc(c.get("value_display", c.get("value", ""))), esc(c.get("owner_signoff")), esc(c.get("signoff_reason") or "SO-07 数值/数据面/标定确认")]) + " |")
L += ["", "## 3. 开放项（OPEN）", "", "| ID | 主题 | 类别 | owner | 锚 |", "|---|---|---|---|---|"]
for o in machine["open_items"]:
    L.append("| " + " | ".join([esc(o["id"]), esc(o["topic"]), esc(o["cls"]), esc(o["owner"]), esc(o["anchor"])]) + " |")
L += ["", "## 4. SO-01..07（保持待签）", "", "| ID | 事项 | owner | 状态 |", "|---|---|---|---|"]
for s in machine["signoff_items"]:
    L.append("| " + " | ".join([esc(s["id"]), esc(s["topic"]), esc(s["owner"]), "PENDING_OWNER_SIGNOFF"]) + " |")
w("reports/v6/contract-review/01_FREEZE_STATUS_MATRIX.md", "\n".join(L) + "\n")

# reports/v6/contract-review/03_SUPERSEDED_SCI_SECTIONS.md
L = [HDR, "", "> 回应 AR-032 / SO-06：列出与本包冻结冲突的**非 v6** `docs/science/*.md` 段落，**只登记清单与建议措辞，不得改写这些文件**。正式 amendment 须负责人签字后由 DOC-CONVERGE-001(W12) 统一收口。", "",
 "| 编号 | 文件 | 段落 | 行 | 冲突事实 | 冻结取代条款 | 签字 |", "|---|---|---|---|---|---|---|"]
for s in machine["superseded_sections"]:
    L.append("| " + " | ".join([esc(s["id"]), esc(s["file"]), esc(s["section"]), esc(s["lines"]), esc(s["statement"]), esc(", ".join(s["superseded_by"])), esc(s["signoff"])]) + " |")
L += ["", "## 建议措辞（逐条，仅为提案，未改写）", ""]
for s in machine["superseded_sections"]:
    L += ["### " + s["id"] + " — " + s["file"] + " " + s["section"] + "（行 " + s["lines"] + "）", "",
          "- 冲突：" + s["statement"], "- 取代条款：" + ", ".join(s["superseded_by"]),
          "- 建议措辞：" + s["suggested_wording"], "- 签字：" + s["signoff"], ""]
L += ["## 覆盖声明", "",
 "本清单覆盖 AR-032 点名的 `DRIZZLE.md` / `CONTROL_WEIGHT_SNR.md` / `ACR_EQUIVALENCE.md` / `INTEGRATION.md`，并补登 `CALIBRATION.md`（OI-03）、`PHASE3_HIPS_TO_FITS.md`（F3-05/F3-06 口径）与 `UNCERTAINTY_AND_COVARIANCE.md`（F-OBS-05 k_corr 口径，部分取代/需注记）。其余非 v6 `docs/science/*.md`（ASTROMETRY/NOISE_MODEL/PHASE2_UPM/PHOTOMETRY/PSF/REJECTION/SCIENCE_SCOPE/STAR_DETECTION/UNIFIED_SCIENCE_MODEL/PSF_SIGNAL_WEIGHT）本轮未登记冲突；UNIFIED 与 PSF_SIGNAL_WEIGHT 为上位权威（非取代对象）。"]
w("reports/v6/contract-review/03_SUPERSEDED_SCI_SECTIONS.md", "\n".join(L) + "\n")

# reports/v6/contract-review/04_OPEN_ITEMS_AND_SIGNOFF.md
L = [HDR, "", "## 1. 开放项总表（DI / OI / PF / P3 / AR 缺口）", "",
 "| ID | 主题 | 类别 | owner | 状态 |", "|---|---|---|---|---|"]
for o in machine["open_items"]:
    L.append("| " + " | ".join([esc(o["id"]), esc(o["topic"]), esc(o["cls"]), esc(o["owner"]), o["status"]]) + " |")
L += ["", "## 2. pending_freeze 7 条（度量/门冻结，数值未冻）", "",
 "| 门 | 度量 | 设计默认值 | owner | 条款 id |", "|---|---|---|---|---|",
 "| G-INJ-01 | `|bias|/F_ref` | 0.02 | ALG-P2-POINT-001 | QF-G-INJ-01 |",
 "| G-INJ-02 | 三量 max 偏差 | 0.03 | ALG-P2-SURF-001 | QF-G-INJ-02 |",
 "| G-INJ-03 | `|W_info-W_info_ref|/W_info_ref` | 0.05 | ALG-P2-PSFSW-001 | QF-G-INJ-03 |",
 "| G-INJ-07 | `|flux_rec/F_inj-1|` | 0.02 | ALG-P3-001 | QF-G-INJ-07 |",
 "| G-RD-01 | M42 接缝/噪声清单失败数 | 0 | REAL-SCIENCE-001 | QF-G-RD-01 |",
 "| G-RD-02 | 银心清单失败数 | 0 | REAL-SCIENCE-001 | QF-G-RD-02 |",
 "| G-BASE-03 | psfsw 基线效应量 | >0 | ALG-P2-PSFSW-001/预注册 | QF-G-BASE-03 |",
 "", "## 3. 需负责人签字（保持待签，不得写成已冻结）", "", "| ID | 事项 | owner | 状态 |", "|---|---|---|---|"]
for s in machine["signoff_items"]:
    L.append("| " + " | ".join([esc(s["id"]), esc(s["topic"]), esc(s["owner"]), "PENDING_OWNER_SIGNOFF"]) + " |")
L += ["", "## 4. 控制器级事项（只登记不裁决）", "", "| ID | 事项 | owner | 状态 |", "|---|---|---|---|"]
for s in machine["controller_only_items"]:
    L.append("| " + " | ".join([esc(s["id"]), esc(s["topic"]), esc(s["ruling_owner"]), "registered_only"]) + " |")
w("reports/v6/contract-review/04_OPEN_ITEMS_AND_SIGNOFF.md", "\n".join(L) + "\n")

print("SUMMARY", json.dumps(machine["counts"], ensure_ascii=False))
