#!/usr/bin/env python3
# 实验/SCI-C/code/c6_sparse_dense.py
"""C6 稀疏现场求值 vs 稠密表示：数值等价 + 内存随节点/分块而非像素增长。

判据（证据分级见 README §5）：
  E1  生产 dense cache 与 sparse calibrate_block 数值等价（max|Δ| ≤ 1e-12 冻结门）
  E2  稀疏模型持久化体积 < 5% 稠密栅格体积
  E3  按需求值（只探 64x64 块）峰值 RSS < 稠密物化（全 512x512）峰值 RSS
  E4  天光面稀疏系数（n_nodes）≪ 像素数；子集求值与全网格求值逐位一致
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import sci_c_common as S
from c1_additive import CFG_FULL, SKY_CFG, scenario
from c3_public_plane import make_samples, pix_to_sky


def peak_rss_kb(argv):
    """在独立子进程里跑 probe 并取其峰值 RSS [kB]（RUSAGE_CHILDREN 新鲜进程）。

    argv 必须传列表：仓库路径含空格，字符串 split 会把路径切碎。
    """
    code = (
        "import resource,subprocess,sys\n"
        "subprocess.run(sys.argv[1:], check=True)\n"
        "print(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)\n"
    )
    r = subprocess.run([sys.executable, "-c", code] + list(argv),
                       capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(r.stderr[-2000:])
    return int(r.stdout.strip().splitlines()[-1])


def run_probe(sc, tag):
    sp = S.RUN / ("c6_%s.json" % tag)
    op = S.RUN / ("c6_%s.out.json" % tag)
    sp.write_text(json.dumps(sc), encoding="utf-8")
    rss = peak_rss_kb([str(S.RUN / "upm_probe"), str(sp), str(op)])
    return json.loads(op.read_text(encoding="utf-8")), rss


def main():
    g = S.Gates()
    res = {}
    w = S.build_world(seed_tag="c6")

    # ---- E1/E2/E3：UPM 稀疏 vs 稠密
    full = dict(cfg=CFG_FULL, obs=w["obs"], frames=[S.FRAME_IDS[n] for n in w["names"]],
                probe=dict(tile=0, x0=0, y0=0, nx=S.TILE_PX, ny=S.TILE_PX),
                dense_cache=str(S.RUN / "c6_dense.bin"),
                sparse_model_path=str(S.RUN / "c6_sparse.json"),
                out_bin=str(S.RUN / "c6_full.bin"))
    o_full, rss_full = run_probe(full, "full")
    block = dict(full, probe=dict(tile=0, x0=0, y0=0, nx=64, ny=64),
                 out_bin=str(S.RUN / "c6_blk.bin"))
    block.pop("dense_cache")
    block.pop("sparse_model_path")
    o_blk, rss_blk = run_probe(block, "blk")
    res["upm_memory"] = dict(
        sparse_bytes=o_full.get("sparse_bytes"), dense_bytes=o_full.get("dense_bytes"),
        dense_vs_sparse_max_abs=o_full.get("dense_vs_sparse_max_abs"),
        dense_vs_sparse_n=o_full.get("dense_vs_sparse_n"),
        rss_full_grid_kb=rss_full, rss_64block_kb=rss_blk,
        rc_materialize=o_full.get("rc_materialize_dense"))
    g.add("E1_dense_equals_sparse",
          "生产 dense cache 与 sparse calibrate_block 数值等价（max|Δ| ≤ 1e-12）",
          o_full.get("dense_vs_sparse_max_abs"),
          float(o_full.get("dense_vs_sparse_max_abs", 9)) <= 1e-12 and
          int(o_full.get("dense_vs_sparse_n", 0)) == S.TILE_PX * S.TILE_PX * 4)
    sb, db = o_full.get("sparse_bytes", 0), o_full.get("dense_bytes", 1)
    res["size_ratio"] = float(sb) / max(float(db), 1.0)
    g.add("E2_sparse_much_smaller", "稀疏模型持久化体积 < 5% 稠密栅格",
          dict(sparse=sb, dense=db, ratio=res["size_ratio"]), res["size_ratio"] < 0.05)
    g.add("E3_rss_scales_with_block", "按需求值 64x64 块峰值 RSS < 稠密物化全网格峰值 RSS",
          dict(blk=rss_blk, full=rss_full), rss_blk < rss_full)

    # ---- E4：天光面稀疏系数与子集求值一致性
    samples, ra, dec = make_samples(w)
    yy, xx = np.mgrid[0:S.TILE_PX, 0:S.TILE_PX]
    ra_f, dec_f = pix_to_sky(xx.ravel().astype(float), yy.ravel().astype(float))
    o_sky, D_full, B_full = S.run_sky_probe(
        dict(cfg=SKY_CFG, samples=samples, frames=[S.FRAME_IDS[n] for n in w["names"]],
             probe=dict(ra_deg=ra_f.tolist(), dec_deg=dec_f.tolist())), "c6_sky_full")
    # 子集：只取 (x,y) 落在 [0,64)x[0,64) 的探针点
    sel = (xx.ravel() < 64) & (yy.ravel() < 64)
    o_sub, D_sub, B_sub = S.run_sky_probe(
        dict(cfg=SKY_CFG, samples=samples, frames=[S.FRAME_IDS[n] for n in w["names"]],
             probe=dict(ra_deg=ra_f[sel].tolist(), dec_deg=dec_f[sel].tolist())), "c6_sky_sub")
    dmax = float(np.max(np.abs(D_sub - D_full[:, sel])))
    bmax = float(np.max(np.abs(B_sub - B_full[:, sel])))
    res["sky_plane"] = dict(n_nodes=o_sky.get("n_nodes"), n_params=o_sky.get("n_params"),
                            n_pixels=S.TILE_PX * S.TILE_PX,
                            node_ratio=float(o_sky.get("n_nodes", 0)) / (S.TILE_PX * S.TILE_PX),
                            delta_subset_max_abs=dmax, b_subset_max_abs=bmax,
                            model_hash_full=o_sky.get("model_hash"),
                            model_hash_sub=o_sub.get("model_hash"))
    g.add("E4_sky_on_demand", "天光面 n_nodes ≪ 像素数 且子集现场求值与全网格逐位一致（≤1e-12）",
          dict(node_ratio=res["sky_plane"]["node_ratio"], dmax=dmax, bmax=bmax),
          res["sky_plane"]["node_ratio"] < 0.01 and max(dmax, bmax) <= 1e-12)

    res["gates"] = g.summary()
    p = S.json_dump(res, "c6_sparse_dense.json")
    print("== C6 稀疏现场求值 vs 稠密 ==")
    for r in res["gates"]["rows"]:
        print("  [%s] %-28s %s" % ("PASS" if r["ok"] else "FAIL", r["id"], r["value"]))
    print("  -> %s" % p)
    return 0 if res["gates"]["n_fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
