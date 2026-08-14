#!/usr/bin/env python3
# V17 G8：Phase1 真实 16 帧 E2E 性能基准运行器（NON_PRODUCTION_TOOL_ONLY）
#
# 用途：
#   - 逐帧运行 orchestrator.exe <stage1.json>（真实 NGC1727 H-alpha 队列）；
#   - cold：每帧初始指向取 FITS header（null hint，与 V16 相同）；
#   - warm：从上一帧 stdout 的 CRVAL 解析已解中心，写入下一帧
#     platesolve.initial_ra_deg/initial_dec_deg（hint/refine 政策，
#     不复制 WCS；solver 仍逐帧独立求解+验证）；
#   - 收集每帧 wall 与每阶段 [Xs] STAGE 完成 → JSON（median/p95）。
#
# 用法：
#   py -3.12 tools/phase1_e2e_bench.py --configs run/temp/satgate/e2e/stage1_1727_NN.json \
#       --name before_cold --warm 0 --subset 16
import argparse
import json
import re
import statistics
import subprocess
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ORCH = ROOT / "lib" / "orchestrator" / "cpp" / "orchestrator.exe"
OUTDIR = ROOT / "run" / "temp" / "perf_v17"


def parse_crval(text):
    m = re.search(r"CRVAL=\(([-\d.]+),\s*([-\d.]+)\)", text)
    if not m:
        return None
    return float(m.group(1)), float(m.group(2))


def parse_timings(text):
    stages = {}
    for m in re.finditer(r"\[([\d.]+)s\] ([A-Z_]+) 完成", text):
        stages.setdefault(m.group(2), []).append(float(m.group(1)))
    walls = {}
    for m in re.finditer(r"frame(\d+) rc=(\d+) wall=([\d.]+)s", text):
        walls[int(m.group(1))] = (int(m.group(2)), float(m.group(3)))
    return stages, walls


def summarize(stages, walls):
    out = {"frame_wall_s": {}}
    for k, (rc, w) in sorted(walls.items()):
        out["frame_wall_s"][k] = {"rc": rc, "wall_s": w}
    out["stage"] = {}
    for stage, vals in sorted(stages.items()):
        sv = sorted(vals)
        out["stage"][stage] = {
            "n": len(vals),
            "median_s": round(statistics.median(vals), 3),
            "p95_s": round(sv[min(len(sv) - 1, int(len(sv) * 0.95))], 3),
            "first_s": round(vals[0], 3),
            "last_s": round(vals[-1], 3),
        }
    ok = [w for _, (rc, w) in walls.items()]
    out["wall_median_s"] = round(statistics.median(ok), 3) if ok else None
    out["wall_p95_s"] = round(sorted(ok)[int(len(ok) * 0.95) - 1], 3) \
        if ok else None
    out["all_rc_zero"] = all(rc == 0 for rc, _ in walls.values()) \
        and len(walls) > 0
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--configs", nargs="+", required=True)
    ap.add_argument("--name", required=True)
    ap.add_argument("--warm", type=int, default=0)
    ap.add_argument("--subset", type=int, default=0)
    args = ap.parse_args()

    configs = sorted(Path(c) for c in args.configs)
    if args.subset > 0:
        configs = configs[: args.subset]
    OUTDIR.mkdir(parents=True, exist_ok=True)
    log_path = OUTDIR / f"{args.name}.log"
    frames_text = []
    env = dict(subprocess.os.environ)
    env["PATH"] = r"C:\msys64\mingw64\bin;" + env.get("PATH", "")
    hint = None
    for idx, cfg_path in enumerate(configs):
        cfg = json.loads(cfg_path.read_text(encoding="utf-8"))
        if args.warm and hint is not None:
            cfg.setdefault("platesolve", {})["initial_ra_deg"] = hint[0]
            cfg.setdefault("platesolve", {})["initial_dec_deg"] = hint[1]
        tmp = cfg_path.with_name(f"{cfg_path.stem}_bench.json")
        tmp.write_text(json.dumps(cfg, ensure_ascii=False, indent=2),
                       encoding="utf-8")
        t0 = time.monotonic()
        r = subprocess.run([str(ORCH), str(tmp)], capture_output=True,
                           text=True, encoding="utf-8", errors="replace",
                           env=env, timeout=1800)
        wall = time.monotonic() - t0
        out = r.stdout + r.stderr
        tag = f"frame{idx:02d}"
        frames_text.append(
            f"===== {tag} (config {cfg_path.name}) =====\n")
        frames_text.append(out)
        frames_text.append(f"frame{idx:02d} rc={r.returncode} wall={wall:.1f}s\n")
        sol = parse_crval(out)
        if sol is not None:
            hint = sol
        print(f"[{args.name}] {tag} rc={r.returncode} "
              f"hint={'next' if (args.warm and sol) else '-'}")
    (OUTDIR / f"{args.name}.log").write_text("\n".join(frames_text),
                                             encoding="utf-8")
    text = "\n".join(frames_text)
    stages, walls = parse_timings(text)
    summary = summarize(stages, walls)
    summary["mode"] = "warm" if args.warm else "cold"
    summary["n_frames"] = len(configs)
    summary["log"] = f"{args.name}.log"
    (OUTDIR / f"{args.name}.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8")
    print(json.dumps(summary, indent=2))
    print("written:", OUTDIR / f"{args.name}.json")


if __name__ == "__main__":
    main()
