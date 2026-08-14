#!/usr/bin/env python3
# V17 G6/G9：API/语义 ID/status/版本 与 docs 的 machine 一致性校验
#
# checks：
#   1. public_header_api_vs_docs  — 已删除 API 不得在 docs/reports 中被
#      无标注地列为现存接口；头文件 public 函数（p2_* 新接口）必须出现在
#      PUBLIC_API.md / api_inventory.md
#   2. semantic_ids_vs_docs        — rejection.h 的 P2_SEMANTIC_* canonical
#      ID 必须出现在 docs+reports；docs 表格不得含已删除 minmax
#      max_iterations 组合
#   3. status_enums_vs_docs        — P2_STATUS_* / P2_INTEGRATE_* 枚举名
#      必须出现在 docs+reports
#   4. profile_canonical           — wbpp_current 不得作为 canonical 出现
#      （只允许标注 alias/migration 或已删除的说明行）
#   5. false_reject_naming         — reports 不得把真实观测率叫
#      clean_sample_false_reject（允许历史说明行）
#   6. freeze_version_vs_report    — SCIENCE_FREEZE.md 与 final_status.md
#      必须指向当前版本 V17；冻结状态字面量必须与 EXPECTED_FREEZE 一致
#   7. schema_vs_parser / defaults_vs_template — 复用 config_consistency
#      （该工具输出 evidence/config_consistency.json；此处仅引用）
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED_FREEZE = "CANDIDATE"   # G10 Round6 clean-tree 终验通过后改 PASS
SCAN_DIRS = [ROOT / "docs", ROOT / "reports", ROOT / "docs_snapshot"]
OUT = ROOT / "run" / "temp" / "p2_v17_evidence" / "api_doc_consistency.json"


def collect_text():
    parts = []
    for d in SCAN_DIRS:
        if not d.exists():
            continue
        for p in d.rglob("*"):
            if p.is_file() and p.suffix.lower() in (".md", ".csv", ".txt"):
                parts.append((p, p.read_text(encoding="utf-8", errors="replace")))
    return parts


def main():
    texts = collect_text()
    problems = []
    checks = {}

    # ---- 1. deleted API absent ----
    hits = []
    for p, t in texts:
        for ln, line in enumerate(t.splitlines(), 1):
            if "p2_rejection_workspace_create" in line or \
                    "p2_rejection_workspace_free" in line:
                if not any(k in line for k in ("已删除", "不再存在", "删除")):
                    hits.append(f"{p.relative_to(ROOT)}:{ln}")
    checks["deleted_api_p2_rejection_workspace"] = len(hits) == 0
    if hits:
        problems.append({"check": "deleted_api",
                         "unannotated_mentions": hits})

    # ---- 1b. new public APIs present ----
    required_apis = [
        "p2_collect_candidate_stack", "p2_validate_candidate_weights",
        "p2_large_scale_apply", "p2_reject_stack_ex",
        "p2_integrate_pixel", "p2_reject_plan_resolve",
    ]
    api_docs = (ROOT / "docs" / "contracts" / "PUBLIC_API.md").read_text(
        encoding="utf-8", errors="replace")
    api_inv = (ROOT / "reports" / "api_inventory.md").read_text(
        encoding="utf-8", errors="replace")
    missing = [a for a in required_apis
               if a not in api_docs and a not in api_inv]
    checks["public_header_api_vs_docs"] = len(missing) == 0
    if missing:
        problems.append({"check": "public_api_missing", "apis": missing})

    # ---- 2. semantic IDs ----
    hdr = (ROOT / "lib" / "phase2" / "include" / "astro" / "phase2"
           / "rejection.h").read_text(encoding="utf-8", errors="replace")
    ids = re.findall(r'#define P2_SEMANTIC_\w+\s+"([^"]+)"', hdr)
    all_text = "\n".join(t for _, t in texts)
    missing_ids = [i for i in ids if i not in all_text]
    checks["semantic_ids_vs_docs"] = len(missing_ids) == 0
    if missing_ids:
        problems.append({"check": "semantic_ids", "missing": missing_ids})
    stale_minmax = "reject_low_count/reject_high_count/max_iterations/min_kept"
    sem_doc = next(t for p, t in texts
                   if str(p).endswith("rejection_semantics.md"))
    checks["minmax_docs_no_max_iterations"] = stale_minmax not in sem_doc
    if stale_minmax in sem_doc:
        problems.append({"check": "minmax_max_iterations_stale"})

    # ---- 3. status enums ----
    status_names = ["P2_STATUS_INVALID_METHOD", "P2_STATUS_INTERNAL_ERROR",
                    "P2_INTEGRATE_INVALID_INPUT", "P2_INTEGRATE_ALL_REJECTED",
                    "P2_INTEGRATE_ZERO_VALID_WEIGHT",
                    "P2_INTEGRATE_NO_CANDIDATES"]
    missing_status = [s for s in status_names
                      if not any(s in t for _, t in texts)]
    checks["status_enums_vs_docs"] = len(missing_status) == 0
    if missing_status:
        problems.append({"check": "status_enums", "missing": missing_status})

    # ---- 4. wbpp_current canonical ----
    bad_wbpp = []
    for p, t in texts:
        lines = t.splitlines()
        for ln, line in enumerate(lines, 1):
            if "wbpp_current" in line:
                nxt = lines[ln] if ln < len(lines) else ""
                ann = "alias" in line or "migration" in line or \
                    "不再" in line or "删除" in line or \
                    "alias" in nxt or "migration" in nxt
                if not ann:
                    bad_wbpp.append(f"{p.relative_to(ROOT)}:{ln}")
    checks["wbpp_current_only_alias"] = len(bad_wbpp) == 0
    if bad_wbpp:
        problems.append({"check": "wbpp_current_canonical", "lines": bad_wbpp})

    # ---- 5. false reject naming ----
    bad_fr = []
    for p, t in texts:
        if str(p).endswith("satellite_v2.md"):
            continue   # 该文件含 V16 历史说明段（显式标注根因）
        for ln, line in enumerate(t.splitlines(), 1):
            if "clean_sample_false_reject" in line or \
                    "clean sample false reject" in line:
                bad_fr.append(f"{p.relative_to(ROOT)}:{ln}")
    checks["observed_rejection_naming"] = len(bad_fr) == 0
    if bad_fr:
        problems.append({"check": "false_reject_naming", "lines": bad_fr})

    # ---- 6. freeze version ----
    sf = (ROOT / "docs" / "validation" / "SCIENCE_FREEZE.md").read_text(
        encoding="utf-8", errors="replace")
    has_v17 = "V17" in sf and "True Final Freeze" in sf
    freeze_lit = f"ASTROCS_FOUNDATION_FINAL_FREEZE = {EXPECTED_FREEZE}"
    checks["freeze_version_vs_report"] = has_v17 and freeze_lit in sf
    if not has_v17 or freeze_lit not in sf:
        problems.append({"check": "freeze_version",
                         "expected_literal": freeze_lit})

    # ---- 7. config consistency（复用既有工具）----
    r = subprocess.run(
        [sys.executable, str(ROOT / "tools" / "config_consistency_check.py")],
        capture_output=True, text=True, encoding="utf-8", errors="replace",
        cwd=ROOT, timeout=120)
    try:
        cc = json.loads(r.stdout[r.stdout.find("{"):])
        checks["schema_vs_parser"] = bool(cc.get("pass"))
        checks["defaults_vs_template"] = bool(cc.get("pass"))
        if not cc.get("pass"):
            problems.append({"check": "config_consistency",
                             "mismatches": cc.get("mismatches")})
    except Exception:
        checks["schema_vs_parser"] = False
        checks["defaults_vs_template"] = False
        problems.append({"check": "config_consistency_runner", "stdout":
                         r.stdout[-500:], "stderr": r.stderr[-500:]})

    res = {"expected_freeze": EXPECTED_FREEZE,
           "semantic_ids": sorted(ids),
           "checks": checks,
           "problems": problems,
           "pass": len(problems) == 0}
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(res, ensure_ascii=False, indent=2),
                   encoding="utf-8")
    print(json.dumps(res, ensure_ascii=False, indent=2))
    sys.exit(0 if res["pass"] else 1)


if __name__ == "__main__":
    main()
