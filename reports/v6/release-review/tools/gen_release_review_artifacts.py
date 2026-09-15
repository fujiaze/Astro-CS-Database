#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""DOC-CONVERGE-001 机器可读发布复核判定生成器（V6 并行包 Wave 12）。

读取 W4 冻结合同与一致性检查器输出，生成：
  artifacts/v6/release-review/release_review_verdict.json
  artifacts/v6/release-review/pending_items.json
不改变任何冻结值，只做结构化汇总。
"""
from __future__ import annotations
import json, os, subprocess, sys, datetime

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", ".."))
FREEZE = os.path.join(ROOT, "docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json")
CONV = os.path.join(ROOT, "artifacts/v6/release-review/doc_convergence_report.json")
OUT = os.path.join(ROOT, "artifacts/v6/release-review")


def head():
    return subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT, capture_output=True, text=True).stdout.strip()


def main():
    fr = json.load(open(FREEZE, encoding="utf-8"))
    conv = json.load(open(CONV, encoding="utf-8")) if os.path.isfile(CONV) else {}
    clauses = fr["clauses"]
    pending = [c["id"] for c in clauses if c["status"] == "PENDING_OWNER_SIGNOFF"]
    open_clauses = [c["id"] for c in clauses if c["status"] == "OPEN"]
    frozen = [c["id"] for c in clauses if c["status"] == "FROZEN"]
    now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")

    steps = [
        {"n": 1, "step": "任务提交全部完成", "verdict": "NOT_MET",
         "basis": "TASK_LEDGER.csv: WIN-VERIFY-001=WAITING_WINDOWS, DOC-CONVERGE-001=READY, FINAL-AUDIT-001=BLOCKED, OWNER-PACK-001=BLOCKED"},
        {"n": 2, "step": "GitHub CI 通过", "verdict": "NOT_MET",
         "basis": "Linux CI 持续红: THREAD-BUDGET rc=1 (lib/core/src/module_adapters.cpp:245,252 P36 回退态), CTEST-REGISTRATION rc=1 (2 非 V6 IPV); Windows CI run 35012779853 failure 无候选"},
        {"n": 3, "step": "Linux 最终 SHA 真实数据流终验", "verdict": "NOT_MET",
         "basis": "REAL-SCIENCE-001 为库级五口径(equal/exposure/ivar=DOCUMENTED_BASELINE); Phase2 CLI obs=0 fail-closed; M42/银心马赛克终验与 write_phase1_product->run_point_information/run_psfsw_robust 未跑; G-RD-01/02 OPEN"},
        {"n": 4, "step": "Agent 图像初审", "verdict": "NOT_MET",
         "basis": "reports/v6 检索 图像/预览/preview/初审 零命中; 无固定显示参数预览与结构化初审结论"},
        {"n": 5, "step": "Windows/Fatduck 复验", "verdict": "AWAITING",
         "basis": "WIN-VERIFY-001=AWAITING_WINDOWS_VALIDATION; Fatduck 不可达(ssh255/tcp124/ping-loss/tailscale-offline); 32/32 UNAVAILABLE; FD-F-003 OPEN"},
        {"n": 6, "step": "汇总和打包", "verdict": "NOT_MET",
         "basis": "OWNER-PACK-001=BLOCKED(depends_on FINAL-AUDIT-001)"},
    ]

    verdict = {
        "tool": "reports/v6/release-review/tools/gen_release_review_artifacts.py",
        "task": "DOC-CONVERGE-001", "wave": 12,
        "package": "AstroCS_PARALLEL_SCIENCE_IMPLEMENTATION_V6_20260915",
        "base_head": head(), "generated_utc": now,
        "semantic_authority": "docs/contracts/v6/frozen/astrocs.v6.contract-freeze.v1.json",
        "clause_counts": {"total": len(clauses), "frozen": len(frozen),
                          "pending_owner_signoff": len(pending), "open": len(open_clauses)},
        "section_14_5": steps,
        "section_14_5_summary": {"NOT_MET": 5, "AWAITING": 1, "PASS": 0},
        "section_17_12_gates": {
            "gate_7_same_sha_linux_windows_ci": "NOT_MET",
            "gate_8_linux_real_data_and_fatduck": "NOT_MET",
            "gate_6_resource_utilization": "NOT_MET_PENDING_OWNER_SIGNOFF",
            "gate_10_p0_p1_zero": "NOT_PROVEN(FD-F-003 OPEN)"},
        "ci_redlines": [
            {"check": "THREAD-BUDGET", "rc": 1, "fact": "lib/core/src/module_adapters.cpp:245,252 omp_set_num_threads 未登记 (P36 回退态)", "v6": False},
            {"check": "CTEST-REGISTRATION", "rc": 1, "fact": "ipv_dead_params_lock / ipv_dead_params_lock_selfcheck (非 V6 IPV 残项)", "v6": False},
            {"check": "AGENTS-GOV", "rc": 0, "fact": "GOV_CHECK_PASS 10/10", "v6": True},
            {"check": "VERSION-CONSISTENCY", "rc": 0, "fact": "VERSION_CHECK_PASS", "v6": True},
            {"check": "VERSION-NAMESPACES", "rc": 0, "fact": "VERSION_NAMESPACES_PASS", "v6": True}],
        "windows": {"status": "AWAITING_WINDOWS_VALIDATION", "cases_unavailable": "32/32",
                    "candidate_present": False, "fatduck_reachable": False, "fd_f_003": "OPEN"},
        "resource_gate_so05": {"worker_16_cpu_mean_pct": 65.09, "frozen_min_pct": 85,
                               "worker_16_p50_pct": 87.63, "frozen_p50_min_pct": 90,
                               "memory_growth_unbounded_16w": True, "status": "record_only_pending_owner_signoff"},
        "doc_convergence": {"verdict": conv.get("verdict"), "fail_count": conv.get("fail_count"),
                            "selftest_pass": (conv.get("selftest") or {}).get("selftest_pass"),
                            "checks": conv.get("checks")},
        "concentration_fix": {"verdict": "FIXED", "from": "ADU/px", "to": "ADU/px^2",
                              "anchor": ["FZ-FIELD-PSFSW-4COMP", "ALG-P2-PSFSW-001", "astrocs.v6.psfsw.v1.schema.json concentration.units pattern"],
                              "oracle_rc": 0, "oracle_result": "38/38"},
        "oi04": {"status": "CLOSED", "mechanism": "61 docs/**/v6/** registered in docs/DOCUMENT_INDEX.yaml",
                 "checker_rc": 0, "covered_files": 282},
        "machine_convergence_report": "artifacts/v6/release-review/doc_convergence_report.json",
        "machine_corrections": "artifacts/v6/release-review/convergence_corrections.json",
        "release_review_reports": "reports/v6/release-review/",
        "package_status_recommendation": "NOT_READY",
        "release_status_literal": "NOT_RELEASED",
        "announce_release": False,
        "version_bumped": False,
        "deliverable_task_status_recommendation": "REVIEW_REQUIRED",
        "unresolved_owner_items": [
            "SO-05 resource gate (record-only vs auto-adjudication; 16w 65.09% pass/fail; memory growth)",
            "CI redline ownership (P36 rollback module_adapters.cpp; 2 IPV CTEST-REGISTRATION; Windows MSVC root cause)",
            "F-CAR / F-AIT legacy projection errors (CAR dec sign, AIT sqrt2) - require owner authorization",
            "REAL-SCIENCE-001 equal/exposure/ivar = DOCUMENTED_BASELINE, not full-chain production",
            "Phase2 CLI real-data unreachable (obs=0)",
            "non-v6 SCI supersession (AR-032/SO-06) - owner signature; C-004.5 write protection",
            "schema/units cross-tension (provenance allOf vs signal; quantity.units free string)",
            "AIO FITS real format lowercase e",
            "AR-034-GAP 7 historical CI reds; AR-035-GAP 785 ledger",
            "memory.md still at DOC-CONV-001 SHA (out of write_scope)",
            "concentration fix signoff formality (schema registered_text_error)"],
    }

    pending_items = {
        "task": "DOC-CONVERGE-001", "generated_utc": now, "base_head": head(),
        "clause_status": {"FROZEN": frozen, "PENDING_OWNER_SIGNOFF": pending, "OPEN": open_clauses},
        "signoff_items": fr.get("signoff_items"),
        "open_items": fr.get("open_items"),
        "so_items": [
            {"id": "SO-01", "topic": "F-OBS-01 DRIZZLE §3 术语/单位修正", "status": "PENDING_OWNER_SIGNOFF"},
            {"id": "SO-02", "topic": "F-OBS-02/S2 面亮度归一", "status": "PENDING_OWNER_SIGNOFF"},
            {"id": "SO-03", "topic": "S3 常量场 Oracle 判据", "status": "PENDING_OWNER_SIGNOFF"},
            {"id": "SO-04", "topic": "AR-030/031 协方差产品非目标取代", "status": "PENDING_OWNER_SIGNOFF"},
            {"id": "SO-05", "topic": "AR-036/019/026 §10.5/§17.6 记录/裁决分离", "status": "PENDING_OWNER_SIGNOFF"},
            {"id": "SO-06", "topic": "AR-032 非 v6 SCI 迁移/取代清单", "status": "PENDING_OWNER_SIGNOFF"},
            {"id": "SO-07", "topic": "F-OBS-03/04/05 数值阈值/数据面/标定", "status": "PENDING_OWNER_SIGNOFF"}],
        "psf_snr_power": "DEFERRED_NOT_PRODUCTION",
        "production_modes": ["point_information", "surface_gls", "psfsw_robust"],
        "documented_baseline_modes": ["equal", "pixel_ivar"],
    }

    for name, obj in (("release_review_verdict.json", verdict), ("pending_items.json", pending_items)):
        p = os.path.join(OUT, name)
        with open(p, "w", encoding="utf-8") as f:
            f.write(json.dumps(obj, ensure_ascii=False, indent=2, sort_keys=True) + "\n")
        print("WROTE", os.path.relpath(p, ROOT))
    print("STATUS", verdict["package_status_recommendation"], verdict["release_status_literal"],
          "steps", verdict["section_14_5_summary"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
