# -*- coding: utf-8 -*-
"""
P02-003 A/B 对比工具
=====================
对比 P02-001 旧路径基线 (old_path_baseline.json) 与 P02-003 路径B 结果 (path_b_results.json)
- 逐帧对比 WCS、星数、RMS、耗时
- 统计成功率、RMS 分布、退化帧数
- 应用非退化门限 (来自 P02-001 EVIDENCE_INDEX.md)
- 输出 ab_comparison.json (结构化对比 + 门限检查 + 决策结果)

用法:
    pwsh> python engineering/tools/p02_003_ab_compare.py
"""

from __future__ import annotations

import os
import json
import math
import statistics
from datetime import datetime

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", ".."))

OLD_BASELINE = os.path.join(PROJECT_ROOT, "engineering", "evidence", "P02-001", "old_path_baseline.json")
PATH_B_RESULTS = os.path.join(PROJECT_ROOT, "engineering", "evidence", "P02-003", "path_b_results.json")
OUTPUT = os.path.join(PROJECT_ROOT, "engineering", "evidence", "P02-003", "ab_comparison.json")


# 非退化门限 (来自 P02-001 EVIDENCE_INDEX.md, 冻结不得调整)
THRESHOLDS = {
    "total_success_rate": {"baseline": 0.9985915492957746, "threshold": 0.99, "desc": "总成功率 ≥ 99.0%"},
    "rms_arcsec_median": {"baseline": 0.28518205742507347, "threshold": 0.30, "desc": "RMS 中位 ≤ 0.30\""},
    "rms_arcsec_p99": {"baseline": 0.8663135681503781, "threshold": 1.00, "desc": "RMS p99 ≤ 1.00\""},
    "rms_arcsec_max": {"baseline": 1.4906922231714959, "threshold": 1.60, "desc": "RMS max ≤ 1.60\""},
    "n_pairs_median": {"baseline": 34, "threshold": 30, "desc": "n_pairs 中位 ≥ 30"},
    "n_pairs_min": {"baseline": 13, "threshold": 10, "desc": "n_pairs min ≥ 10"},
    "duration_median": {"baseline": 1.3023930000017572, "threshold": 1.50, "desc": "duration 中位 ≤ 1.50s"},
    "duration_p99": {"baseline": 9.682199099999707, "threshold": 12.00, "desc": "duration p99 ≤ 12.00s"},
    "repeat_max_dRA_deg": {"baseline": 0.0, "threshold": 1e-10, "desc": "重复性 max dRA ≤ 1e-10°"},
    "repeat_max_dDec_deg": {"baseline": 8.88e-15, "threshold": 1e-13, "desc": "重复性 max dDec ≤ 1e-13°"},
}


def load_json(path):
    with open(path, "r", encoding="utf-8") as f:
        return json.load(f)


def stats(arr):
    if not arr:
        return {"count": 0, "min": 0, "max": 0, "mean": 0, "median": 0, "p90": 0, "p99": 0}
    s = sorted(arr)
    n = len(s)
    return {
        "count": n,
        "min": s[0],
        "max": s[-1],
        "mean": statistics.mean(s),
        "median": statistics.median(s),
        "p90": s[int(n * 0.9)] if n >= 10 else s[-1],
        "p99": s[int(n * 0.99)] if n >= 100 else s[-1],
    }


def wcs_equal(a, b, eps=1e-12):
    """比较两帧 WCS 是否 bit-wise identical (浮点容差 eps)"""
    if a is None or b is None:
        return False
    keys = ["crval1", "crval2", "crpix1", "crpix2", "cd1_1", "cd1_2", "cd2_1", "cd2_2"]
    for k in keys:
        if k not in a or k not in b:
            return False
        if abs(float(a[k]) - float(b[k])) > eps:
            return False
    if a.get("sip_order", 0) != b.get("sip_order", 0):
        return False
    # SIP 系数比较
    for k in ["sip_a", "sip_b", "sip_ap", "sip_bp"]:
        av = a.get(k, [])
        bv = b.get(k, [])
        if len(av) != len(bv):
            return False
        for x, y in zip(av, bv):
            if abs(float(x) - float(y)) > eps:
                return False
    return True


def main():
    print("=" * 70)
    print("P02-003 A/B 对比工具")
    print("=" * 70)
    print("旧路径基线: %s" % OLD_BASELINE)
    print("路径B 结果: %s" % PATH_B_RESULTS)
    print("输出: %s" % OUTPUT)
    print("=" * 70)

    old = load_json(OLD_BASELINE)
    new = load_json(PATH_B_RESULTS)

    # 构建帧索引: index -> summary
    old_frames = {f["index"]: f for f in old["all_frames_summary"]}
    new_frames = {f["index"]: f for f in new["all_frames_summary"]}

    # 加载每帧详细 WCS (从 results/frame_XXXX.json)
    old_results_dir = os.path.join(PROJECT_ROOT, "engineering", "evidence", "P02-001", "results")
    new_results_dir = os.path.join(PROJECT_ROOT, "engineering", "evidence", "P02-003", "results")

    def load_frame_detail(results_dir, idx):
        path = os.path.join(results_dir, "frame_%04d.json" % idx)
        if not os.path.isfile(path):
            return None
        try:
            with open(path, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return None

    # 逐帧对比
    all_indices = sorted(set(old_frames.keys()) | set(new_frames.keys()))
    per_frame = []
    n_both_success = 0
    n_old_success_new_fail = 0
    n_old_fail_new_success = 0
    n_both_fail = 0
    n_wcs_identical = 0
    n_wcs_diff = 0
    rms_diffs = []
    npairs_diffs = []
    dur_diffs = []
    new_fail_frames = []

    # 抽样比较 WCS (全部 710 帧逐字节比较成本高, 仅比较成功帧)
    wcs_compare_count = 0

    for idx in all_indices:
        o = old_frames.get(idx)
        n = new_frames.get(idx)
        if o is None or n is None:
            per_frame.append({
                "index": idx, "status": "missing",
                "old_present": o is not None, "new_present": n is not None,
            })
            continue

        o_succ = bool(o.get("success", False))
        n_succ = bool(n.get("success", False))

        entry = {
            "index": idx,
            "case_id": o.get("case_id", n.get("case_id", "")),
            "target_name": o.get("target_name", n.get("target_name", "")),
            "filename": o.get("filename", n.get("filename", "")),
            "filter": o.get("filter", n.get("filter", "")),
            "old_success": o_succ,
            "new_success": n_succ,
        }

        if o_succ and n_succ:
            n_both_success += 1
            o_rms = float(o.get("rms_arcsec", 0))
            n_rms = float(n.get("rms_arcsec", 0))
            o_npairs = int(o.get("n_pairs", 0))
            n_npairs = int(n.get("n_pairs", 0))
            o_dur = float(o.get("duration_sec", 0))
            n_dur = float(n.get("duration_sec", 0))
            entry["old_rms"] = o_rms
            entry["new_rms"] = n_rms
            entry["rms_diff"] = n_rms - o_rms
            entry["old_n_pairs"] = o_npairs
            entry["new_n_pairs"] = n_npairs
            entry["n_pairs_diff"] = n_npairs - o_npairs
            entry["old_duration"] = o_dur
            entry["new_duration"] = n_dur
            entry["duration_diff"] = n_dur - o_dur
            rms_diffs.append(n_rms - o_rms)
            npairs_diffs.append(n_npairs - o_npairs)
            dur_diffs.append(n_dur - o_dur)

            # WCS 比较 (抽样: 前 10 帧 + 失败帧附近 + 随机抽样)
            # 全量比较 710 帧 WCS 成本可接受 (每帧只比较几个浮点数)
            wcs_compare_count += 1
            o_detail = load_frame_detail(old_results_dir, idx)
            n_detail = load_frame_detail(new_results_dir, idx)
            if o_detail and n_detail and "wcs" in o_detail and "wcs" in n_detail:
                if wcs_equal(o_detail["wcs"], n_detail["wcs"]):
                    entry["wcs_identical"] = True
                    n_wcs_identical += 1
                else:
                    entry["wcs_identical"] = False
                    n_wcs_diff += 1
            else:
                entry["wcs_identical"] = None

        elif o_succ and not n_succ:
            n_old_success_new_fail += 1
            entry["status"] = "REGRESSION (旧成功->新失败)"
            entry["old_rms"] = float(o.get("rms_arcsec", 0))
            entry["old_n_pairs"] = int(o.get("n_pairs", 0))
            entry["old_duration"] = float(o.get("duration_sec", 0))
            entry["new_error"] = n.get("error", "") or n.get("error_msg", "")
            new_fail_frames.append(entry)
        elif (not o_succ) and n_succ:
            n_old_fail_new_success += 1
            entry["status"] = "IMPROVEMENT (旧失败->新成功)"
            entry["new_rms"] = float(n.get("rms_arcsec", 0))
            entry["new_n_pairs"] = int(n.get("n_pairs", 0))
        else:
            n_both_fail += 1
            entry["status"] = "BOTH_FAIL"

        per_frame.append(entry)

    # 汇总统计
    old_overall = old["overall"]
    new_overall = new["overall"]

    # 退化帧: 旧成功但新失败, 或新 RMS 显著退化 (> 0.1" 或 > 50%)
    regression_frames = [e for e in per_frame if e.get("status") == "REGRESSION (旧成功->新失败)"]
    rms_degraded = [e for e in per_frame if "rms_diff" in e and e["rms_diff"] > 0.1]

    # 门限检查
    threshold_checks = {}
    new_succ_rate = new_overall["success_rate"]
    new_rms_median = new_overall["rms_arcsec_stats"]["median"]
    new_rms_p99 = new_overall["rms_arcsec_stats"]["p99"]
    new_rms_max = new_overall["rms_arcsec_stats"]["max"]
    new_npairs_median = new_overall["n_pairs_stats"]["median"]
    new_npairs_min = new_overall["n_pairs_stats"]["min"]
    new_dur_median = new_overall["duration_stats"]["median"]
    new_dur_p99 = new_overall["duration_stats"]["p99"]

    # 重复性 (前 N 帧)
    new_repeat = new.get("repeatability_first_n", [])
    new_max_dRA = max([r.get("wcs_diff_ra_deg", 0) or 0 for r in new_repeat], default=0)
    new_max_dDec = max([r.get("wcs_diff_dec_deg", 0) or 0 for r in new_repeat], default=0)

    checks = [
        ("total_success_rate", new_succ_rate, ">=", THRESHOLDS["total_success_rate"]["threshold"]),
        ("rms_arcsec_median", new_rms_median, "<=", THRESHOLDS["rms_arcsec_median"]["threshold"]),
        ("rms_arcsec_p99", new_rms_p99, "<=", THRESHOLDS["rms_arcsec_p99"]["threshold"]),
        ("rms_arcsec_max", new_rms_max, "<=", THRESHOLDS["rms_arcsec_max"]["threshold"]),
        ("n_pairs_median", new_npairs_median, ">=", THRESHOLDS["n_pairs_median"]["threshold"]),
        ("n_pairs_min", new_npairs_min, ">=", THRESHOLDS["n_pairs_min"]["threshold"]),
        ("duration_median", new_dur_median, "<=", THRESHOLDS["duration_median"]["threshold"]),
        ("duration_p99", new_dur_p99, "<=", THRESHOLDS["duration_p99"]["threshold"]),
        ("repeat_max_dRA_deg", new_max_dRA, "<=", THRESHOLDS["repeat_max_dRA_deg"]["threshold"]),
        ("repeat_max_dDec_deg", new_max_dDec, "<=", THRESHOLDS["repeat_max_dDec_deg"]["threshold"]),
    ]

    all_pass = True
    for name, value, op, thr in checks:
        if op == ">=":
            passed = value >= thr
        else:
            passed = value <= thr
        threshold_checks[name] = {
            "value": value,
            "operator": op,
            "threshold": thr,
            "baseline": THRESHOLDS[name]["baseline"],
            "passed": passed,
            "desc": THRESHOLDS[name]["desc"],
        }
        if not passed:
            all_pass = False

    # 失败帧集检查: 新失败帧集 ⊆ 旧失败帧集 ∪ {窄带}
    old_fail_indices = {f["index"] for f in old.get("fail_frames", [])}
    new_fail_indices = {f["index"] for f in new.get("fail_frames", [])}
    # 窄带帧: Oiii / H-alpha
    narrowband_indices = {f["index"] for f in per_frame
                          if f.get("filter", "").lower() in ("oiii", "h-alpha", "halpha")}
    allowed_fail_set = old_fail_indices | narrowband_indices
    new_fail_outside_allowed = new_fail_indices - allowed_fail_set
    fail_set_check = {
        "old_fail_indices": sorted(old_fail_indices),
        "new_fail_indices": sorted(new_fail_indices),
        "narrowband_indices": sorted(narrowband_indices),
        "new_fail_outside_allowed": sorted(new_fail_outside_allowed),
        "passed": len(new_fail_outside_allowed) == 0,
        "desc": "新失败帧集 ⊆ 旧失败帧集 ∪ {窄带}",
    }
    if not fail_set_check["passed"]:
        all_pass = False

    # 决策
    if all_pass and n_old_success_new_fail == 0:
        decision = "MERGE_PATH_B"
        decision_reason = "全部门限通过, 无旧成功->新失败退化, 路径B 可合并到 main"
    else:
        decision = "PRESERVE_OLD_PATH"
        reasons = []
        if n_old_success_new_fail > 0:
            reasons.append("存在 %d 帧旧成功->新失败退化" % n_old_success_new_fail)
        if not all_pass:
            failed_checks = [k for k, v in threshold_checks.items() if not v["passed"]]
            reasons.append("门限未通过: %s" % ", ".join(failed_checks))
        if not fail_set_check["passed"]:
            reasons.append("新失败帧集超出允许范围: %s" % fail_set_check["new_fail_outside_allowed"])
        decision_reason = "; ".join(reasons)

    # 输出
    result = {
        "_meta": {
            "task_id": "P02-003",
            "task_name": "PlateSolve 全量 A/B 与路径决策",
            "generated_at": datetime.now().strftime("%Y-%m-%dT%H:%M:%S+08:00"),
            "old_baseline_file": OLD_BASELINE,
            "new_results_file": PATH_B_RESULTS,
            "old_commit_base": old["_meta"].get("commit_base", ""),
            "new_commit_base": new["_meta"].get("commit_base", ""),
            "old_mode": old["_meta"].get("mode", "old"),
            "new_mode": new["_meta"].get("mode", "path-b"),
        },
        "summary": {
            "total_frames_compared": len(all_indices),
            "both_success": n_both_success,
            "old_success_new_fail": n_old_success_new_fail,
            "old_fail_new_success": n_old_fail_new_success,
            "both_fail": n_both_fail,
            "wcs_compared": wcs_compare_count,
            "wcs_identical": n_wcs_identical,
            "wcs_diff": n_wcs_diff,
            "rms_diff_stats": stats(rms_diffs),
            "n_pairs_diff_stats": stats(npairs_diffs),
            "duration_diff_stats": stats(dur_diffs),
        },
        "overall_comparison": {
            "old": {
                "total": old_overall["total"],
                "success": old_overall["success"],
                "fail": old_overall["fail"],
                "success_rate": old_overall["success_rate"],
                "rms_arcsec_stats": old_overall["rms_arcsec_stats"],
                "n_pairs_stats": old_overall["n_pairs_stats"],
                "duration_stats": old_overall["duration_stats"],
            },
            "new": {
                "total": new_overall["total"],
                "success": new_overall["success"],
                "fail": new_overall["fail"],
                "success_rate": new_overall["success_rate"],
                "rms_arcsec_stats": new_overall["rms_arcsec_stats"],
                "n_pairs_stats": new_overall["n_pairs_stats"],
                "duration_stats": new_overall["duration_stats"],
            },
        },
        "threshold_checks": threshold_checks,
        "fail_set_check": fail_set_check,
        "regression_frames": regression_frames,
        "rms_degraded_frames": rms_degraded,
        "decision": decision,
        "decision_reason": decision_reason,
        "all_thresholds_passed": all_pass,
        "per_frame": per_frame,
    }

    with open(OUTPUT, "w", encoding="utf-8") as f:
        json.dump(result, f, ensure_ascii=False, indent=2, default=str)

    # 打印摘要
    print("")
    print("=" * 70)
    print("A/B 对比摘要")
    print("=" * 70)
    print("总对比帧数: %d" % len(all_indices))
    print("双方成功: %d" % n_both_success)
    print("旧成功->新失败 (退化): %d" % n_old_success_new_fail)
    print("旧失败->新成功 (改善): %d" % n_old_fail_new_success)
    print("双方失败: %d" % n_both_fail)
    print("")
    print("WCS bit-wise 一致性: %d/%d 相同, %d 不同" % (n_wcs_identical, wcs_compare_count, n_wcs_diff))
    if rms_diffs:
        rs = result["summary"]["rms_diff_stats"]
        print("RMS 差异 (new - old): min=%.6f, median=%.6f, max=%.6f" % (
            rs["min"], rs["median"], rs["max"]))
    print("")
    print("成功率: 旧 %.2f%% (%d/%d)  vs  新 %.2f%% (%d/%d)" % (
        old_overall["success_rate"] * 100, old_overall["success"], old_overall["total"],
        new_overall["success_rate"] * 100, new_overall["success"], new_overall["total"]))
    print("RMS 中位: 旧 %.4f\"  vs  新 %.4f\"  (门限 ≤ %.2f\")" % (
        old_overall["rms_arcsec_stats"]["median"],
        new_overall["rms_arcsec_stats"]["median"],
        THRESHOLDS["rms_arcsec_median"]["threshold"]))
    print("RMS p99:  旧 %.4f\"  vs  新 %.4f\"  (门限 ≤ %.2f\")" % (
        old_overall["rms_arcsec_stats"]["p99"],
        new_overall["rms_arcsec_stats"]["p99"],
        THRESHOLDS["rms_arcsec_p99"]["threshold"]))
    print("n_pairs 中位: 旧 %d  vs  新 %d  (门限 ≥ %d)" % (
        old_overall["n_pairs_stats"]["median"],
        new_overall["n_pairs_stats"]["median"],
        THRESHOLDS["n_pairs_median"]["threshold"]))
    print("duration 中位: 旧 %.3fs  vs  新 %.3fs  (门限 ≤ %.2fs)" % (
        old_overall["duration_stats"]["median"],
        new_overall["duration_stats"]["median"],
        THRESHOLDS["duration_median"]["threshold"]))
    print("")
    print("=" * 70)
    print("门限检查:")
    for name, chk in threshold_checks.items():
        status = "PASS" if chk["passed"] else "FAIL"
        print("  [%s] %s: value=%.6f %s %.6f (baseline=%.6f)" % (
            status, name, chk["value"], chk["operator"], chk["threshold"], chk["baseline"]))
    print("")
    print("失败帧集检查: %s" % ("PASS" if fail_set_check["passed"] else "FAIL"))
    print("  旧失败帧: %s" % fail_set_check["old_fail_indices"])
    print("  新失败帧: %s" % fail_set_check["new_fail_indices"])
    if fail_set_check["new_fail_outside_allowed"]:
        print("  超出允许范围的新失败帧: %s" % fail_set_check["new_fail_outside_allowed"])
    print("")
    print("=" * 70)
    print("路径决策: %s" % decision)
    print("决策原因: %s" % decision_reason)
    print("=" * 70)
    print("结果已写入: %s" % OUTPUT)
    return 0


if __name__ == "__main__":
    import sys
    sys.exit(main())
