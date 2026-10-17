#!/usr/bin/env python3
"""Issue #92 A1-FIX —— 修复前/后真实六帧 E2E 对比。

输入: 两次真实 CLI 的 stderr + 产物目录 (pre = 首批冻结 run, post = 本批修复 run)。
输出: JSON + 可读表格, 逐帧对比
  · star_matcher: gaia_projected_in_frame / psf_total / psf_valid / 正反向命中 /
    unique_matches / rejected_* / match_distance / fit_used / scale / sigma / r_inliers
  · p1_phot.json: n_matched / photscal(k_photo) / error_status / photometry_applied /
    sigma_residual_dex / n_frames_applied
  · CLI: exit code
"""
from __future__ import annotations

import argparse
import glob
import json
import os
import re

BLOCK_RE = re.compile(r"\[star_matcher\] 初始化")
P2 = re.compile(r"阶段2: gaia_projected_in_frame=(\d+) / (\d+)")
P3 = re.compile(r"阶段3: psf_total=(\d+), psf_valid=(\d+)")
VALID = re.compile(r"PSF有效星: (\d+) / (\d+), Gaia星: (\d+), 匹配半径: ([\d.]+) px")
FWD = re.compile(r"正向匹配 \(PSF→Gaia\): (\d+) / (\d+)")
BWD = re.compile(r"反向匹配 \(Gaia→PSF\): (\d+) / (\d+)")
P456 = re.compile(r"阶段4/5/6: spatial_candidates=(\d+) \(正向命中\), unique_matches=(\d+) "
                  r"\(双向唯一\), rejected_ambiguous=(\d+).*?, rejected_distance=(\d+)")
KD = re.compile(r"KD-tree 双向匹配完成: (\d+) 对")
MD = re.compile(r"阶段8: match_distance median=([\d.eE+-]+) p90=([\d.eE+-]+) max=([\d.eE+-]+) px")
Q = re.compile(r"阶段6/7/8: rejected_quality=(\d+) .*?fit_used=(\d+), robust_iterations=(\d+), "
               r"scale=([\d.eE+-]+), sigma=([\d.eE+-]+)")
QALL = re.compile(r"阶段6/7: rejected_quality=(\d+) \(全部无效\)")
RI = re.compile(r"阶段8: r_inliers median=([\d.eE+-]+) p90=([\d.eE+-]+) max=([\d.eE+-]+) dex")
PCA = re.compile(r"\[pc_api\] 匹配\+清洗完成: (\d+) 颗, scale=([\d.eE+-]+), sigma_residual=([\d.eE+-]+)")
CONE = re.compile(r"锥形搜索: ra=([-\d.]+) dec=([-\d.]+) r=([-\d.]+) mag_min=([-\d.]+)")
EXIT = re.compile(r'"exit_code":(\d+)')


def parse_stderr(path):
    blocks = []
    cur = None
    cones = []
    with open(path, errors="ignore") as f:
        for line in f:
            if "锥形搜索: ra=" in line:
                m = CONE.search(line)
                if m:
                    cones.append([float(m.group(1)), float(m.group(2)), float(m.group(3)),
                                  float(m.group(4))])
            if BLOCK_RE.search(line):
                if cur is not None:
                    blocks.append(cur)
                cur = {"raw_lines": []}
                continue
            if cur is None:
                continue
            cur["raw_lines"].append(line)
            for key, rx, cast in (
                ("gaia_projected_in_frame", P2, lambda m: [int(m.group(1)), int(m.group(2))]),
                ("psf", P3, lambda m: [int(m.group(1)), int(m.group(2))]),
                ("valid", VALID, lambda m: [int(m.group(1)), int(m.group(2)), int(m.group(3)),
                                            float(m.group(4))]),
                ("fwd", FWD, lambda m: [int(m.group(1)), int(m.group(2))]),
                ("bwd", BWD, lambda m: [int(m.group(1)), int(m.group(2))]),
                ("p456", P456, lambda m: [int(m.group(i)) for i in range(1, 5)]),
                ("kd", KD, lambda m: int(m.group(1))),
                ("match_distance", MD, lambda m: [float(m.group(i)) for i in range(1, 4)]),
                ("quality", Q, lambda m: [int(m.group(1)), int(m.group(2)), int(m.group(3)),
                                          float(m.group(4)), float(m.group(5))]),
                ("quality_all_invalid", QALL, lambda m: int(m.group(1))),
                ("r_inliers", RI, lambda m: [float(m.group(i)) for i in range(1, 4)]),
                ("pc_api", PCA, lambda m: [int(m.group(1)), float(m.group(2)), float(m.group(3))]),
            ):
                m = rx.search(line)
                if m:
                    cur[key] = cast(m)
    if cur is not None:
        blocks.append(cur)
    return blocks, cones


def read_phot(path_glob):
    hits = glob.glob(path_glob, recursive=True)
    if not hits:
        return None
    d = json.load(open(hits[0]))
    out = {
        "path": hits[0],
        "n_frames": d.get("n_frames"), "n_frames_ok": d.get("n_frames_ok"),
        "n_frames_applied": d.get("n_frames_applied"),
        "n_frames_failed": d.get("n_frames_failed"),
        "photscal": d.get("photscal"), "photscale_source": d.get("photscale_source"),
        "photometry_applied": d.get("photometry_applied"),
        "pixel_scaling": d.get("pixel_scaling"),
        "spatial_gain_mode": d.get("spatial_gain_mode"),
        "photscale_detail": d.get("photscale_detail") or {},
        "frames": [],
    }
    detail = d.get("photscale_detail") or {}
    for fr in d.get("frames", []):
        det = detail.get(fr.get("frame_key"), {})
        out["frames"].append({
            "frame_key": fr.get("frame_key"), "status": fr.get("status"),
            "n_matched": det.get("n_matched", fr.get("n_matched")),
            "error_status": fr.get("error_status"),
            "photometry_applied": fr.get("photometry_applied"),
            "sigma_residual_dex": det.get("sigma_residual_dex", fr.get("sigma_residual_dex")),
            "k_photo": det.get("k_photo", fr.get("k_photo")),
            "fitted": det.get("fitted"),
        })
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pre-stderr", required=True)
    ap.add_argument("--post-stderr", required=True)
    ap.add_argument("--pre-out", required=True, help="pre 产物目录 (含 p1_phot.json)")
    ap.add_argument("--post-out", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    pre_blocks, pre_cones = parse_stderr(a.pre_stderr)
    post_blocks, post_cones = parse_stderr(a.post_stderr)

    def exit_of(stderr):
        # exit 在 stdout 里; 这里由调用方另传, 简化: 从 stderr 的 final 行找
        txt = open(stderr, errors="ignore").read()
        m = re.search(r"（exit_code=(\d+)）", txt)
        return int(m.group(1)) if m else None

    res = {
        "pre": {"stderr": a.pre_stderr, "n_star_matcher_blocks": len(pre_blocks),
                "cones": pre_cones, "exit_code": exit_of(a.pre_stderr),
                "p1_phot": read_phot(os.path.join(a.pre_out, "**", "p1_phot.json"))},
        "post": {"stderr": a.post_stderr, "n_star_matcher_blocks": len(post_blocks),
                 "cones": post_cones, "exit_code": exit_of(a.post_stderr),
                 "p1_phot": read_phot(os.path.join(a.post_out, "**", "p1_phot.json"))},
        "blocks_pre": pre_blocks,
        "blocks_post": post_blocks,
    }
    json.dump(res, open(a.out, "w"), indent=2, ensure_ascii=False)

    print(f"{'#':>2} | {'frame_key':<46} | {'cone ra/dec/r':>24} | "
          f"{'pre in':>9} | {'post in':>10} | {'post fwd/uniq':>13} | {'post k_photo':>13}")
    n = max(len(pre_blocks), len(post_blocks))
    pf = (res["post"]["p1_phot"] or {}).get("photscale_detail") or {}
    qf = (res["pre"]["p1_phot"] or {})
    for i in range(n):
        pb = pre_blocks[i] if i < len(pre_blocks) else {}
        qb = post_blocks[i] if i < len(post_blocks) else {}
        cone = post_cones[i] if i < len(post_cones) else (pre_cones[i] if i < len(pre_cones) else None)

        def g(b, k, j=None):
            v = b.get(k)
            if v is None:
                return "-"
            if j is not None:
                return v[j] if isinstance(v, list) and len(v) > j else "-"
            return v
        keys = list(pf.keys())
        fk = keys[i] if i < len(keys) else "-"
        kp = pf.get(fk, {}).get("k_photo") if fk != "-" else None
        print(f"{i:>2} | {fk:<46} | "
              f"{(f'{cone[0]:.4f}/{cone[1]:.4f}/{cone[2]:.3f}' if cone else '-'):>24} | "
              f"{str(g(pb,'gaia_projected_in_frame')):>9} | "
              f"{str(g(qb,'gaia_projected_in_frame')):>10} | "
              f"{str(g(qb,'fwd',0))+'/'+str(g(qb,'p456',1)):>13} | "
              f"{(f'{kp:.6e}' if kp else '-'):>13}")
    print("pre  exit:", res["pre"]["exit_code"], "| post exit:", res["post"]["exit_code"])
    for tag in ("pre", "post"):
        p = res[tag]["p1_phot"]
        if p:
            print(f"{tag}  p1_phot: frames={p['n_frames']} ok={p['n_frames_ok']} "
                  f"applied={p['n_frames_applied']} failed={p['n_frames_failed']} "
                  f"photscal={p['photscal']} applied_flag={p['photometry_applied']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
