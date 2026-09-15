#!/usr/bin/env python3
"""PERF-SCALE-001 heavy-path performance harness (independent, process-external).

Runs a workload under a fixed CPU affinity (taskset). The V6 CLI derives its process
budget from CPU affinity (cli/v6_runtime_contract.h + cli_affinity_cpu_count), so the
affinity set is the "worker count" under test. /proc is sampled externally with
runtime/v6_budget.py::sample_proc (independent Python record surface, distinct from the
in-band C++ ResourceRecorder).

--config-template may contain the token __OUT__, replaced by the per-run output dir and
written next to the run log; the command template uses {cfg} for that path.
"""
from __future__ import annotations
import argparse, hashlib, json, os, pathlib, re, shlex, shutil, statistics, subprocess, sys, time

REPO = pathlib.Path("/workspace/Astro CS Database")
sys.path.insert(0, str(REPO / "runtime"))
import v6_budget  # noqa: E402


def sha256_file(p: pathlib.Path) -> str:
    h = hashlib.sha256()
    with open(p, "rb") as f:
        for c in iter(lambda: f.read(1 << 20), b""):
            h.update(c)
    return h.hexdigest()


def sample_loop(proc, interval: float):
    """Sample /proc of a live process; stop when the process has exited AND is reaped
    (p.poll() is not None). A zombie still answers kill(pid,0), so poll() is authoritative."""
    pid = proc.pid
    samples, prev = [], None
    while proc.poll() is None:
        try:
            cur = v6_budget.sample_proc(pid, prev)
        except Exception:
            break
        samples.append(cur)
        prev = cur
        time.sleep(interval)
    return samples


def slope_mb_per_s(ts, rss):
    n = len(ts)
    if n < 3:
        return None
    mt = sum(ts) / n
    mr = sum(rss) / n
    num = sum((t - mt) * (r - mr) for t, r in zip(ts, rss))
    den = sum((t - mt) ** 2 for t in ts)
    return None if den <= 0 else (num / den) / 1048576.0


def active_window_stats(samples, allocated):
    if len(samples) < 2:
        return {"n_intervals": 0}
    t0 = samples[0]["t"]
    eq = []
    for s in samples[1:]:
        dt = max(1e-9, s["dt"])
        eq.append((s["t"] - t0, s["d_process_cpu"] / dt, s["d_active_threads"]))
    wall = eq[-1][0]
    worst_low_run = run = 0.0
    prev_t = 0.0
    for (t, e, _a) in eq:
        dt, prev_t = t - prev_t, t
        if allocated > 0 and (e / allocated) < 0.60:
            run += dt
            worst_low_run = max(worst_low_run, run)
        else:
            run = 0.0
    first10 = [e for (t, e, _a) in eq if t <= 10.0]
    act = [a for (_t, _e, a) in eq]
    return {
        "n_intervals": len(eq), "wall_active_s": wall,
        "max_consecutive_below_60pct_s": worst_low_run,
        "first10s_mean_cpu_frac_of_allocated": (statistics.fmean([e / allocated for e in first10])
                                                if first10 and allocated else None),
        "active_compute_threads_min": min(act) if act else None,
        "active_compute_threads_max": max(act) if act else None,
        "active_compute_threads_mean": statistics.fmean(act) if act else None,
        "intervals_with_single_active_compute_thread": sum(1 for a in act if a <= 1),
        "intervals_total": len(act),
    }


def run_one(cmd, cpu_list, out_dir, interval, cfg_text, env_extra=None):
    work = pathlib.Path(out_dir)
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True, exist_ok=True)
    cfg_path = None
    if cfg_text is not None:
        rel = work.relative_to(REPO) if str(work).startswith(str(REPO)) else work
        resolved = cfg_text.replace("__OUT__", str(rel))
        cfg_path = work / "run_cfg.json"
        cfg_path.write_text(resolved, encoding="utf-8")
    argv = ["taskset", "-c", cpu_list] + shlex.split(cmd)
    wall0 = time.monotonic()
    log = open(work / "run.log", "wb")
    env = dict(os.environ)
    if env_extra:
        env.update(env_extra)
    p = subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT, cwd=str(REPO), env=env)
    samples = sample_loop(p, interval)
    rc = p.wait()
    log.close()
    wall = time.monotonic() - wall0
    allocated = sum((int(a.split("-")[1]) - int(a.split("-")[0]) + 1) if "-" in a else 1
                    for a in cpu_list.split(","))
    m = {"cmd": argv, "cpu_list": cpu_list, "allocated_cores": allocated,
         "rc": rc, "wall_seconds": wall, "interval": interval,
         "config": (str(cfg_path.relative_to(REPO)) if cfg_path else None)}
    if samples:
        last, first = samples[-1], samples[0]
        proc_cpu = last["process_cpu_seconds"] - first["process_cpu_seconds"]
        m["proc_cpu_seconds"] = proc_cpu
        m["avg_equivalent_cores"] = proc_cpu / wall if wall > 0 else None
        m["cpu_mean_percent_of_allocated"] = (100.0 * proc_cpu / wall / allocated
                                              if wall > 0 and allocated else None)
        m["per_thread_cpu_sum_seconds"] = sum(s["d_thread_sum"] for s in samples[1:])
        m["per_thread_cpu_max_seconds"] = max((s["d_thread_max"] for s in samples[1:]), default=0.0)
        m["rss_peak_bytes"] = max(s["rss_bytes"] for s in samples)
        m["pss_peak_bytes"] = max(s["pss_bytes"] for s in samples)
        m["rss_first_bytes"] = first["rss_bytes"]
        m["rss_last_bytes"] = last["rss_bytes"]
        m["threads_peak"] = max(s["threads"] for s in samples)
        m["read_bytes"] = sum(s["d_read_bytes"] for s in samples[1:])
        m["write_bytes"] = sum(s["d_write_bytes"] for s in samples[1:])
        iow = sum(s["d_iowait"] for s in samples[1:])
        m["io_wait_percent"] = 100.0 * iow / wall if wall > 0 else None
        m["io_wait_seconds"] = iow
        m["n_samples"] = len(samples)
        m["rss_growth_mb_per_s"] = slope_mb_per_s([s["t"] - samples[0]["t"] for s in samples],
                                                  [s["rss_bytes"] for s in samples])
        m["active"] = active_window_stats(samples, allocated)
    return m


def product_hashes(root: pathlib.Path):
    return {str(p.relative_to(root)).replace(os.sep, "/"): sha256_file(p)
            for p in sorted(root.rglob("*")) if p.is_file()}


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", required=True)
    ap.add_argument("--cmd", required=True, help="command template; {out} reqd, {cfg} optional")
    ap.add_argument("--config-template", default="")
    ap.add_argument("--budgets", default="1,4,16")
    ap.add_argument("--cpu-map", default="1:0,4:0-3,16:0-15")
    ap.add_argument("--reps", type=int, default=3)
    ap.add_argument("--interval", type=float, default=0.25)
    ap.add_argument("--out", required=True)
    ap.add_argument("--products-subdir", default="")
    ap.add_argument("--env", action="append", default=[], help="KEY=VALUE, repeatable")
    args = ap.parse_args(argv)
    env_extra = {}
    for kv in args.env:
        k, v = kv.split("=", 1)
        env_extra[k] = v
    cmap = {}
    for part in args.cpu_map.split(","):
        k, v = part.split(":")
        cmap[int(k)] = v
    budgets = [int(x) for x in args.budgets.split(",") if x.strip()]
    cfg_text = pathlib.Path(args.config_template).read_text(encoding="utf-8") if args.config_template else None
    out = pathlib.Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    results = []
    for b in budgets:
        cpu = cmap[b]
        for rep in range(args.reps):
            outdir = REPO / "run" / "v6" / "performance" / ("%s_w%d_r%d" % (args.name, b, rep))
            rel = outdir.relative_to(REPO)
            cmd = args.cmd.format(out=str(rel), cfg=str(rel / "run_cfg.json"))
            m = run_one(cmd, cpu, outdir, args.interval, cfg_text, env_extra)
            m.update({"budget": b, "rep": rep, "out_dir": str(rel)})
            prod_root = outdir / args.products_subdir if args.products_subdir else outdir
            m["product_hashes"] = product_hashes(prod_root) if prod_root.exists() else {}
            results.append(m)
            print("[%s b=%d r=%d] rc=%d wall=%.2fs eqcores=%.2f cpumean=%.1f%% rss_peak=%.0fMB wait60=%.1fs single=%d/%d" % (
                args.name, b, rep, m["rc"], m["wall_seconds"],
                m.get("avg_equivalent_cores") or -1, m.get("cpu_mean_percent_of_allocated") or -1,
                (m.get("rss_peak_bytes") or 0) / 1048576.0,
                (m.get("active") or {}).get("max_consecutive_below_60pct_s") or 0,
                (m.get("active") or {}).get("intervals_with_single_active_compute_thread") or 0,
                (m.get("active") or {}).get("intervals_total") or 0), flush=True)
    (out / ("%s_runs.json" % args.name)).write_text(
        json.dumps({"name": args.name, "budgets": budgets, "reps": args.reps, "runs": results},
                   ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
