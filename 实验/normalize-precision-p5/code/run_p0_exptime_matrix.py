#!/usr/bin/env python3
"""P0（Issue #7 A.3）EXPTIME 变体矩阵: 真实生产入口复现/回归。

对每个 EXPTIME 变体（valid/missing/zero/negative/nan）跑一次
`<acsd> normalize -y --json run/P0-EXPTIME/cfg-<v>.json`（偏置+平场，无暗流 ⇒
不要求亮帧 EXPTIME 参与 K=t_light/t_dark；WCS 用 explicit_config 给出真实解，
绕开 IPV —— IPV 的极限星等派生本身需要 EXPTIME，否则无法到达 drizzle 之后的
精度分支），记录:
  * 进程退出码 / 是否异常终止（ASan 报告时为非零 + stderr 含 ERROR 行）;
  * stderr 是否出现 ASan/UBSan/SEGV 报告（use-after-free 的判据）;
  * 产品侧车 frame_science.json 的 exposure_seconds / exposure_source
    （证明缺失/非法 EXPTIME 走的是**具名 unavailable** 语义, 不是读已释放内存）;
  * 同帧其它科学量（precision 控制点数 / 帧摘要 SNR）是否与 valid 变体一致。

用法: python3 run_p0_exptime_matrix.py <acsd_binary> <tag>
日志落 evidence/p0_exptime_<tag>_<variant>.log。
"""
from __future__ import annotations

import json
import os
import subprocess
import sys

VARIANTS = ["valid", "missing", "zero", "negative", "nan"]
ROOT = "/workspace/Astro CS Database"
EVID = os.path.join(ROOT, "实验/normalize-precision-p5/evidence")


def sidecar_summary(out_dir: str):
    """从产品侧车取 exposure 与 precision 摘要（找不到返回 None）。"""
    if not os.path.isdir(out_dir):
        return None
    for d in sorted(os.listdir(out_dir)):
        p = os.path.join(out_dir, d, "frame_science.json")
        if not os.path.exists(p):
            continue
        try:
            doc = json.load(open(p, "r", encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as e:
            return {"error": "sidecar unreadable: %s" % e}
        controls = doc.get("precision", {}).get("controls", [])
        n_avail = sum(1 for c in controls
                      if c.get("status", {}).get("availability") == "available")
        return {
            "frame": d,
            "exposure_seconds": doc.get("exposure_seconds"),
            "exposure_source": doc.get("exposure_source"),
            "n_controls": len(controls),
            "n_available": n_avail,
            "frame_snr": doc.get("precision", {}).get("frame_summary", {})
                            .get("reference_snr"),
        }
    return None


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    acsd, tag = sys.argv[1], sys.argv[2]
    rows = []
    for v in VARIANTS:
        cfg = os.path.join(ROOT, "run/P0-EXPTIME/wcscfg-%s.json" % v)
        out_dir = os.path.join(ROOT, "run/P0-EXPTIME/wcsout-%s" % v)
        log = os.path.join(EVID, "p0_exptime_%s_%s.log" % (tag, v))
        with open(log, "w", encoding="utf-8") as lf:
            # -force: 本复现刻意不提供 master_dark（否则亮帧 EXPTIME 会被
            # K=t_light/t_dark 路径先行拒绝，到不了 drizzle 后的精度分支）；
            # 预检对"缺母版"报错，-force 是正本给的显式越过开关。
            proc = subprocess.run([acsd, "normalize", "-y", "-force", "--json", cfg],
                                  stdout=lf, stderr=subprocess.STDOUT,
                                  cwd=ROOT)
        text = open(log, "r", encoding="utf-8", errors="replace").read()
        san = [k for k in ("AddressSanitizer", "runtime error:", "LeakSanitizer",
                           "SEGV", "heap-use-after-free", "SUMMARY:")
               if k in text]
        rows.append({
            "variant": v, "exit": proc.returncode,
            "sanitizer_hits": san,
            "sidecar": sidecar_summary(out_dir),
            "log": log,
        })
    print("== P0 EXPTIME matrix (tag=%s, binary=%s) ==" % (tag, acsd))
    for r in rows:
        s = r["sidecar"] or {}
        print("  %-8s exit=%-3s san=%-14s exposure=%s source=%s controls=%s/%s snr=%s"
              % (r["variant"], r["exit"], ",".join(r["sanitizer_hits"]) or "-",
                 s.get("exposure_seconds"), s.get("exposure_source"),
                 s.get("n_available"), s.get("n_controls"), s.get("frame_snr")))
    out = os.path.join(EVID, "p0_exptime_%s_matrix.json" % tag)
    json.dump(rows, open(out, "w", encoding="utf-8"), indent=1)
    print("matrix -> %s" % out)
    # 判据: 无 sanitizer 命中; 所有变体 exit=0; 非法 EXPTIME 走 unavailable 语义
    bad = [r for r in rows if r["sanitizer_hits"] or r["exit"] != 0]
    for r in rows:
        s = r["sidecar"] or {}
        if r["variant"] != "valid" and s:
            if s.get("exposure_seconds") != 0.0 or "unavailable" not in \
                    str(s.get("exposure_source", "")):
                bad.append({"variant": r["variant"],
                            "why": "invalid EXPTIME must map to named unavailable"})
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
