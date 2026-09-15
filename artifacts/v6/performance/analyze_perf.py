#!/usr/bin/env python3
"""PERF-SCALE-001 analysis: 1/4/16-worker scaling + §10.5 resource-gate record surface.

Reads the harness runs JSON (external /proc sampler) plus each run's in-band
resource_summary.json (CLI ResourceRecorder, stage-segmented), and emits:
  * perf_summary.json  — per-budget medians, speedup, parallel efficiency, memory/IO facts
  * perf_table.csv     — one row per measured run
  * resource_gate_record.json — §10.5 findings, ALWAYS record_only + PENDING_OWNER_SIGNOFF

SO-05 discipline (CONTROLLER_LOG C-007 + cli/v6_runtime_contract.h): thresholds are the
frozen §18.2 values (85% mean / 90% p50 / 60% 10s window / 32 MB/s growth / >=2 active
compute threads), but no PASS/FAIL is adjudicated here: findings carry
would_fail_if_signed and the verdict is withheld.
"""
from __future__ import annotations
import argparse, csv, json, pathlib, statistics, sys


def load_samples(run_dir: pathlib.Path):
    """Parse the in-band resource_samples.csv (ResourceRecorder) into row dicts."""
    p = run_dir / "resource_samples.csv"
    if not p.exists():
        return []
    try:
        with open(p, newline="", encoding="utf-8") as f:
            return list(csv.DictReader(f))
    except Exception:
        return []

# frozen §18.2 / §10.5 thresholds (cited, not invented here)
TH_MEAN = 85.0
TH_P50 = 90.0
TH_WINDOW_SECONDS = 10.0
TH_QUEUE_LOW = 60.0
TH_GROWTH_MB_S = 32.0
SO05 = {"signoff_id": "SO-05", "signoff_status": "PENDING_OWNER_SIGNOFF",
        "measurement_policy": "record_only",
        "auto_adjudication_policy": "auto_adjudication_withheld_pending_owner_signoff",
        "hard_fail": False}


def median(vals):
    vals = [v for v in vals if v is not None]
    return statistics.median(vals) if vals else None


def load_inband(run_dir: pathlib.Path):
    p = run_dir / "resource_summary.json"
    if not p.exists():
        return None
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except Exception:
        return None


def load_alloc(run_dir: pathlib.Path):
    p = run_dir / "alloc_report.json"
    if not p.exists():
        return None
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except Exception:
        return None


def active_stage(doc):
    if not doc:
        return None
    for s in doc.get("stages", []):
        if s.get("stage") == "active":
            return s
    return None


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs-json", required=True)
    ap.add_argument("--out-prefix", required=True)
    args = ap.parse_args(argv)
    doc = json.loads(pathlib.Path(args.runs_json).read_text(encoding="utf-8"))
    name = doc["name"]
    runs = doc["runs"]
    by_budget = {}
    for r in runs:
        by_budget.setdefault(r["budget"], []).append(r)

    table = []
    summary = {"schema": "astrocs.v6.perf.summary/v1", "name": name,
               "budgets": sorted(by_budget), "reps": doc.get("reps"),
               "metrics_units": {
                   "cpu_pct": "percent_of_one_core (100 = 1 core fully busy)",
                   "cpu_mean_percent_of_allocated": "100 = allocated capacity fully busy",
                   "rss_growth_mb_per_s": "least-squares slope over sampled window"},
               "per_budget": {}, "so05": SO05}
    for b in sorted(by_budget):
        rs = by_budget[b]
        allocated = rs[0]["allocated_cores"]
        inband = [active_stage(load_inband(pathlib.Path(r["out_dir"]))) for r in rs]
        inband = [x for x in inband if x]
        allocs = [load_alloc(pathlib.Path(r["out_dir"])) for r in rs]
        allocs = [x for x in allocs if x]
        alloc_growth = [a.get("rss_growth_mb_per_s") for a in allocs if a.get("rss_growth_mb_per_s") is not None]
        alloc_verdicts = sorted({a.get("growth_verdict") for a in allocs if a.get("growth_verdict")})
        # in-band active-stage sample aggregates (read/write bytes, queue depth, lock wait)
        act_rows = []
        for r in rs:
            for row in load_samples(pathlib.Path(r["out_dir"])):
                if row.get("stage") == "active":
                    act_rows.append(row)

        def sumcol(rows, col):
            tot = 0.0
            for row in rows:
                try:
                    tot += float(row.get(col, 0) or 0)
                except ValueError:
                    pass
            return tot

        def maxcol(rows, col):
            vals = []
            for row in rows:
                try:
                    vals.append(float(row.get(col, 0) or 0))
                except ValueError:
                    pass
            return max(vals) if vals else None
        ext_mean_alloc = median([r.get("cpu_mean_percent_of_allocated") for r in rs])
        entry = {
            "allocated_cores": allocated,
            "n_reps": len(rs),
            "rc_all_zero": all(r["rc"] == 0 for r in rs),
            "wall_seconds_median": median([r.get("wall_seconds") for r in rs]),
            "wall_seconds_all": [r.get("wall_seconds") for r in rs],
            "proc_cpu_seconds_median": median([r.get("proc_cpu_seconds") for r in rs]),
            "avg_equivalent_cores_median": median([r.get("avg_equivalent_cores") for r in rs]),
            "external_cpu_mean_percent_of_allocated_median": ext_mean_alloc,
            "inband_active_n": len(inband),
            "inband_active_cpu_pct_mean": median([s.get("cpu_pct_mean") for s in inband]),
            "inband_active_cpu_mean_percent_of_allocated": (
                median([s.get("cpu_pct_mean") / allocated for s in inband]) if inband else None),
            "inband_active_cpu_p50_percent_of_allocated": (
                median([s.get("cpu_pct_p50") / allocated for s in inband]) if inband else None),
            "inband_active_cpu_p95_percent_of_allocated": (
                median([s.get("cpu_pct_p95") / allocated for s in inband]) if inband else None),
            "inband_active_wall_seconds": median([s.get("wall_seconds") for s in inband]),
            "inband_active_workers_p50": median([s.get("workers_p50") for s in inband]),
            "inband_active_compute_threads_peak": max(
                [s.get("active_compute_threads_peak", 0) for s in inband], default=0),
            "inband_per_thread_cpu_max_pct": median([s.get("per_thread_cpu_max_pct") for s in inband]),
            "inband_io_wait_pct_mean": median([s.get("io_wait_pct_mean") for s in inband]),
            "rss_peak_bytes_median": median([r.get("rss_peak_bytes") for r in rs]),
            "pss_peak_bytes_median": median([r.get("pss_peak_bytes") for r in rs]),
            "read_bytes_median": median([r.get("read_bytes") for r in rs]),
            "write_bytes_median": median([r.get("write_bytes") for r in rs]),
            "io_wait_percent_external_median": median([r.get("io_wait_percent") for r in rs]),
            "threads_peak_median": median([r.get("threads_peak") for r in rs]),
            "rss_growth_mb_per_s_external_median": median([r.get("rss_growth_mb_per_s") for r in rs]),
            "inband_alloc_growth_mb_per_s_median": median(alloc_growth),
            "inband_alloc_growth_mb_per_s_max": max(alloc_growth) if alloc_growth else None,
            "inband_alloc_growth_verdicts": alloc_verdicts,
            "inband_alloc_reclaim_verdicts": sorted({a.get("reclaim_verdict") for a in allocs if a.get("reclaim_verdict")}),
            "inband_active_read_bytes_sum": sumcol(act_rows, "read_bytes") if act_rows else None,
            "inband_active_write_bytes_sum": sumcol(act_rows, "write_bytes") if act_rows else None,
            "inband_active_queue_depth_max": maxcol(act_rows, "queue_depth") if act_rows else None,
            "inband_active_lock_wait_ns_sum": sumcol(act_rows, "lock_wait_ns") if act_rows else None,
            "inband_active_runnable_workers_max": maxcol(act_rows, "runnable_workers") if act_rows else None,
            "inband_active_compute_threads_mean": (
                sumcol(act_rows, "active_compute_threads") / len(act_rows) if act_rows else None),
            "external_active": {
                "max_consecutive_below_60pct_s_median": median(
                    [r.get("active", {}).get("max_consecutive_below_60pct_s") for r in rs]),
                "active_compute_threads_min_of_min": min(
                    [r.get("active", {}).get("active_compute_threads_min", 0) for r in rs], default=0),
                "active_compute_threads_max_of_max": max(
                    [r.get("active", {}).get("active_compute_threads_max", 0) for r in rs], default=0),
                "active_compute_threads_mean_median": median(
                    [r.get("active", {}).get("active_compute_threads_mean") for r in rs]),
                "intervals_with_single_thread_total": sum(
                    r.get("active", {}).get("intervals_with_single_active_compute_thread", 0) for r in rs),
                "intervals_total": sum(r.get("active", {}).get("intervals_total", 0) for r in rs),
            },
        }
        summary["per_budget"][str(b)] = entry
        for r in rs:
            table.append({
                "budget": b, "rep": r["rep"], "rc": r["rc"],
                "allocated_cores": allocated,
                "wall_seconds": r.get("wall_seconds"),
                "proc_cpu_seconds": r.get("proc_cpu_seconds"),
                "avg_equivalent_cores": r.get("avg_equivalent_cores"),
                "cpu_mean_percent_of_allocated_external": r.get("cpu_mean_percent_of_allocated"),
                "rss_peak_bytes": r.get("rss_peak_bytes"),
                "pss_peak_bytes": r.get("pss_peak_bytes"),
                "read_bytes": r.get("read_bytes"),
                "write_bytes": r.get("write_bytes"),
                "io_wait_percent_external": r.get("io_wait_percent"),
                "threads_peak": r.get("threads_peak"),
                "rss_growth_mb_per_s": r.get("rss_growth_mb_per_s"),
                "active_compute_threads_max": r.get("active", {}).get("active_compute_threads_max"),
                "max_consecutive_below_60pct_s": r.get("active", {}).get("max_consecutive_below_60pct_s"),
                "intervals_single_thread": r.get("active", {}).get("intervals_with_single_active_compute_thread"),
                "intervals_total": r.get("active", {}).get("intervals_total"),
            })

    # scaling: baseline = smallest budget
    base_b = min(by_budget)
    base_wall = summary["per_budget"][str(base_b)]["wall_seconds_median"]
    for b in sorted(by_budget):
        e = summary["per_budget"][str(b)]
        w = e["wall_seconds_median"]
        e["speedup_vs_%d" % base_b] = (base_wall / w) if (base_wall and w) else None
        e["parallel_efficiency_vs_%d" % base_b] = (
            (base_wall / w) / (b / base_b) if (base_wall and w) else None)
        e["wall_per_equivalent_core_s"] = (
            (w / e["avg_equivalent_cores_median"]) if (w and e["avg_equivalent_cores_median"]) else None)

    # ---- §10.5 record surface (never adjudicated) ----
    findings = []
    for b in sorted(by_budget):
        e = summary["per_budget"][str(b)]
        allocated = e["allocated_cores"]
        window = e["inband_active_wall_seconds"] or e["external_active"].get("wall_active_s")
        domain = allocated >= 2 and (window or 0) >= TH_WINDOW_SECONDS
        f = {"budget": b, "allocated_cores": allocated,
             "judgement_domain_active": bool(domain),
             "basis": "in-band ResourceRecorder active stage" if e["inband_active_n"] else "external sampler"}
        if domain:
            m = e["inband_active_cpu_mean_percent_of_allocated"]
            p = e["inband_active_cpu_p50_percent_of_allocated"]
            if m is not None and m < TH_MEAN:
                findings.append({"kind": "cpu_mean_low", "budget": b,
                                 "detail": "active-stage CPU mean %.2f%% of allocated < %.0f%%"
                                           % (m, TH_MEAN), "would_fail_if_signed": True})
            if p is not None and p < TH_P50:
                findings.append({"kind": "cpu_p50_low", "budget": b,
                                 "detail": "active-stage CPU p50 %.2f%% of allocated < %.0f%%"
                                           % (p, TH_P50), "would_fail_if_signed": True})
            low = e["external_active"]["max_consecutive_below_60pct_s_median"]
            if low is not None and low >= TH_WINDOW_SECONDS:
                findings.append({"kind": "consecutive_low_utilization", "budget": b,
                                 "detail": "consecutive %.1fs below %.0f%% of allocated (window >= %.0fs)"
                                           % (low, TH_QUEUE_LOW, TH_WINDOW_SECONDS),
                                 "would_fail_if_signed": True})
            if e["external_active"]["active_compute_threads_max_of_max"] <= 1:
                findings.append({"kind": "single_active_compute_thread", "budget": b,
                                 "detail": "max active compute threads = %d"
                                           % e["external_active"]["active_compute_threads_max_of_max"],
                                 "would_fail_if_signed": True})
        # §10.5 memory criterion is the in-band MON-002 alloc report over the active window
        # (external LSQ over the whole wall is reported separately as corroboration).
        if "unbounded" in (e.get("inband_alloc_growth_verdicts") or []) and (window or 0) >= TH_WINDOW_SECONDS:
            findings.append({"kind": "memory_growth_unbounded", "budget": b,
                             "detail": "in-band alloc report growth_verdict=unbounded in >=1 run "
                                       "(max rss_growth %.2f MB/s, median %.2f MB/s; failure line %.0f MB/s; "
                                       "active window %.1fs >= %.0fs)"
                                       % (e.get("inband_alloc_growth_mb_per_s_max") or -1,
                                          e.get("inband_alloc_growth_mb_per_s_median") or -1,
                                          TH_GROWTH_MB_S, window or -1, TH_WINDOW_SECONDS),
                             "would_fail_if_signed": True})
        if "unexplained_residual" in (e.get("inband_alloc_reclaim_verdicts") or []):
            findings.append({"kind": "alloc_reclaim_missing", "budget": b,
                             "detail": "in-band alloc report reclaim_verdict=unexplained_residual",
                             "would_fail_if_signed": True})
        g = e["rss_growth_mb_per_s_external_median"]
        if g is not None and g >= TH_GROWTH_MB_S and (window or 0) >= TH_WINDOW_SECONDS:
            findings.append({"kind": "memory_growth_unbounded", "budget": b,
                             "detail": "external RSS LSQ slope %.2f MB/s >= %.0f MB/s over >=%.0fs"
                                       % (g, TH_GROWTH_MB_S, TH_WINDOW_SECONDS),
                             "would_fail_if_signed": True})
    gate = {"schema": "astrocs.v6.perf.resource-gate-record/v1", "name": name,
            "thresholds_source": "ASTROCS_PROJECT_CONSTITUTION.md §10.5 / §18.2 (frozen)",
            "thresholds": {"mean_percent_of_allocated": TH_MEAN, "p50_percent_of_allocated": TH_P50,
                           "consecutive_window_seconds": TH_WINDOW_SECONDS,
                           "consecutive_util_percent": TH_QUEUE_LOW,
                           "memory_growth_mb_per_s": TH_GROWTH_MB_S,
                           "min_active_compute_threads": 2},
            "so05": SO05, "status": "record_only_pending_owner_signoff",
            "hard_fail": False, "findings": findings,
            "facts_for_release_gates": {
                "only_single_thread_in_recompute_observed": any(
                    f["kind"] == "single_active_compute_thread" for f in findings),
                "unbounded_memory_growth_observed": any(
                    f["kind"] == "memory_growth_unbounded" for f in findings),
            },
            "note": "PENDING_OWNER_SIGNOFF: findings are recorded facts, not a PASS/FAIL verdict."}

    prefix = pathlib.Path(args.out_prefix)
    prefix.parent.mkdir(parents=True, exist_ok=True)
    prefix.with_name(prefix.name + "_perf_summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    prefix.with_name(prefix.name + "_resource_gate_record.json").write_text(
        json.dumps(gate, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    with open(str(prefix) + "_perf_table.csv", "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(table[0].keys()))
        w.writeheader()
        w.writerows(table)
    for b in sorted(by_budget):
        e = summary["per_budget"][str(b)]
        print("b=%-2d wall=%.2fs eqcores=%.2f speedup=%.3f eff=%.3f cpumean_alloc=%.1f%%"
              % (b, e["wall_seconds_median"], e["avg_equivalent_cores_median"] or -1,
                 e["speedup_vs_%d" % base_b] or -1, e["parallel_efficiency_vs_%d" % base_b] or -1,
                 e["inband_active_cpu_mean_percent_of_allocated"] or -1))
    print("findings=%d %s" % (len(findings), [f["kind"] + "@" + str(f["budget"]) for f in findings]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
