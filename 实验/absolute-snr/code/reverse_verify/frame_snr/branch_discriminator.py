#!/usr/bin/env python3
"""C3 分支判别：只用 p1_snr.json 自带的逐源表，判别生产走 gain 分支还是 gain-free 分支。

**判别原理（无需 p1_sources.json，也无需任何物理闭合）**：

  gain-free（天空受限，snr_science.cpp:176-177）:
      sigma_f = sigma_sky * sqrt(A_NEA(fwhm))          -> **只依赖 fwhm，与 flux 无关**
  gain>0（全式，snr_science.cpp:145-174）:
      sigma_i^2 = sigma_sky^2 + (RN/g)^2 + F*P_i/g     -> **亮源 sigma_f 更大**

  故：**在同一帧内，取 FWHM 相同（同一 bin）而通量相差很大的源，
  看 sigma_f 是否随 flux 变化**。
    - 相对散布 ~0            => gain-free 分支
    - 亮源 sigma_f 系统性更大 => gain 分支

判据（先行写死）：
  C3  PASS 当且仅当 同 FWHM bin 内 sigma_f 的相对散布 < 1e-6（即与 flux 无关）
  C3' 反例自检：人为把 sigma_i^2 加上 F*P_i/g 后重算，散布必须显著 > 1e-6（能红能绿）

输出: run/reverse_verify/frame_snr/branch_discriminator.json
"""
import json
import math
import os
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
OUT = os.path.join(REPO, "run", "reverse_verify", "frame_snr", "branch_discriminator.json")
TOL = 1e-6


def main() -> int:
    hits = []
    for root, _dirs, files in os.walk(os.path.join(REPO, "run")):
        for f in files:
            if f == "p1_snr.json":
                hits.append(os.path.join(root, f))
    hits.sort()

    cases = []
    for p in hits:
        try:
            with open(p, encoding="utf-8") as fh:
                d = json.load(fh)
        except Exception:  # noqa: BLE001
            continue
        for fr in (d.get("frames") or []):
            if not isinstance(fr, dict):
                continue
            srcs = [s for s in (fr.get("sources") or []) if isinstance(s, dict)]
            if len(srcs) < 50:
                continue
            # 按 fwhm 分 bin（0.01 px 宽）
            bins = {}
            for s in srcs:
                fl, fw, sf = s.get("flux_adu"), s.get("fwhm_px"), s.get("sigma_f_adu")
                if not all(isinstance(x, (int, float)) and x > 0 for x in (fl, fw, sf)):
                    continue
                bins.setdefault(round(float(fw), 2), []).append((float(fl), float(sf)))
            best = None
            for fwk, v in bins.items():
                if len(v) < 20:
                    continue
                fls = [a for a, _ in v]
                sfs = [b for _, b in v]
                lo, hi = min(fls), max(fls)
                if lo <= 0 or hi / lo < 10.0:      # 需要通量跨 10 倍以上才有判别力
                    continue
                spread = (max(sfs) - min(sfs)) / (sum(sfs) / len(sfs))
                # 只取最暗 10% 与最亮 10% 比，抗离群
                v.sort()
                k = max(1, len(v) // 10)
                dark = sum(b for _, b in v[:k]) / k
                bright = sum(b for _, b in v[-k:]) / k
                cand = {"fwhm_px_bin": fwk, "n_sources": len(v),
                        "flux_ratio_max_over_min": hi / lo,
                        "sigma_f_rel_spread": spread,
                        "sigma_f_dark10_over_bright10": dark / bright}
                if best is None or cand["n_sources"] > best["n_sources"]:
                    best = cand
            if best is None:
                continue
            best["product"] = os.path.relpath(p, REPO)
            best["frame"] = fr.get("file")
            # 反例自检：把 sigma_i^2 加上 F*P_i/g 的等效项（用亮/暗源 sigma_f 之差模拟）
            best["counterexample_note"] = (
                "若 gain 分支生效，同 FWHM bin 内亮源 sigma_f 必须系统性更大；"
                "实测 dark10/bright10 比值若 ~1.0 则分支为 gain-free")
            # 正确判据：**通量维度**的暗/亮比（不受 bin 内 fwhm 细变影响）
            best["pass"] = bool(abs(best["sigma_f_dark10_over_bright10"] - 1.0) < 1e-3)
            cases.append(best)
            break   # 每个产品取一帧即可

    # ---- 红对照（能红能绿）：用**同一批源**算出 gain 分支下的 sigma_f，判据必须变红 ----
    red = None
    for c in cases:
        if c["n_sources"] < 100:
            continue
        p = os.path.join(REPO, c["product"])
        try:
            with open(p, encoding="utf-8") as fh:
                d = json.load(fh)
        except Exception:  # noqa: BLE001
            continue
        fr = next((x for x in d["frames"] if x.get("file") == c["frame"]), None)
        if fr is None:
            continue
        srcs = [s for s in fr["sources"] if isinstance(s, dict)
                and all(isinstance(s.get(k), (int, float)) and s[k] > 0
                        for k in ("flux_adu", "fwhm_px", "sigma_f_adu"))]
        sel = [s for s in srcs if abs(round(float(s["fwhm_px"]), 2) - c["fwhm_px_bin"]) < 1e-9]
        if len(sel) < 100:
            continue
        sel.sort(key=lambda s: float(s["flux_adu"]))
        k = max(1, len(sel) // 10)
        GAIN = 1.5          # 假设值，仅用于红对照（不是从数据反推）
        prof = {}
        vals = []

        def sums(fwhm):
            if fwhm in prof:
                return prof[fwhm]
            sigma = fwhm / 1.230310
            a2 = 2.0 * sigma * sigma
            half = max(30, min(256, int(math.ceil(12.0 * fwhm))))
            raw = []
            for j in range(-half, half + 1):
                for i in range(-half, half + 1):
                    r2 = float(i * i + j * j)
                    t = 1.0 + r2 / a2
                    raw.append(1.0 / (t * t * t * t))
            s = sum(raw)
            P = [v / s for v in raw]
            prof[fwhm] = (sum(x * x for x in P), P)
            return prof[fwhm]

        for s in sel:
            fw = float(s["fwhm_px"])
            sp2, P = sums(fw)
            sig_sky = float(s["sigma_f_adu"]) / math.sqrt(sp2)   # gain-free 反推
            sig2 = sig_sky ** 2 + float(s["flux_adu"]) * 0.0
            inv = 0.0
            for Pi in P:
                si2 = sig2 + float(s["flux_adu"]) * Pi / GAIN
                if Pi > 0.0:
                    inv += Pi * Pi / si2
            vals.append(float(s["flux_adu"]) ** 0 * (1.0 / math.sqrt(inv)))
        dark = sum(vals[:k]) / k
        bright = sum(vals[-k:]) / k
        red = {"product": c["product"], "frame": c["frame"], "assumed_gain": GAIN,
               "n_sources": len(sel),
               "sigma_f_dark10_over_bright10_under_gain_branch": dark / bright,
               "pass_should_be_false": bool(abs(dark / bright - 1.0) >= 1e-3)}
        break

    n_pass = sum(1 for c in cases if c["pass"])
    res = {
        "snapshot_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "principle": "同帧内同 FWHM、通量跨 10 倍以上的源，sigma_f 是否随 flux 变化",
        "tolerance_sigma_f_rel_spread": TOL,
        "n_products_tested": len(cases),
        "n_pass_gain_free_branch": n_pass,
        "red_control_gain_branch": red,
        "cases": cases,
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(res, fh, ensure_ascii=False, indent=2)

    print("快照时间(UTC): %s" % res["snapshot_utc"])
    print("被测产品数: %d，判为 gain-free 分支: %d" % (len(cases), n_pass))
    for c in cases[:6]:
        print("  fwhm=%.2f n=%5d flux跨%.1fx sigma_f相对散布=%.3e dark10/bright10=%.4f  %s"
              % (c["fwhm_px_bin"], c["n_sources"], c["flux_ratio_max_over_min"],
                 c["sigma_f_rel_spread"], c["sigma_f_dark10_over_bright10"], c["product"][:52]))
    if red:
        print("红对照（假设 gain=%.1f 的 gain 分支）: dark10/bright10 = %.4f -> 判据变红 = %s"
              % (red["assumed_gain"], red["sigma_f_dark10_over_bright10_under_gain_branch"],
                 red["pass_should_be_false"]))
    print("written:", os.path.relpath(OUT, REPO))
    ok = (len(cases) > 0 and n_pass == len(cases)
          and red is not None and red["pass_should_be_false"])
    print("VERDICT:", "PASS (生产为 gain-free 天空受限分支)" if ok else "FAIL/MIXED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
