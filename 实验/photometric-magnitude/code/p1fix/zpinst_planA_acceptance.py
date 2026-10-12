#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""方案 A 仪器零点的数据分析验收（不重跑；基线取自既有产物）。

基线来源（既有产物，原始代码产出）：
  run/PHOT-FIX/p1_rerun/t2/p1_phot.json  -> 逐帧 k_photo
  run/PHOT-FIX/p1_rerun/t2/p1_snr.json   -> 旧 zp_inst (frames[].snr_reference.frame_zero_point_mag)
                                            旧 m5      (frames[].frame_depth_m5_mag)
新值 = 计算（同一帧同一 k_photo）：
  zp_inst_new = C_abs - 2.5*log10(k_photo),  C_abs = -14.276811281840642 (方案 A 冻结常数)
  m5_new      = zp_inst_new - (zp_inst_old - m5_old)     # 2.5*log10(F5) 逐帧不变
权重：w ∝ 1/F_ref², F_ref ∝ 10^(0.4*zp_inst) ⇒ w ∝ 10^(-0.8*zp_inst)
"""
import json, math, os, re, numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))))
T2 = os.path.join(ROOT, "run/PHOT-FIX/p1_rerun/t2")
C_ABS = -14.276811281840642

phot = json.load(open(os.path.join(T2, "p1_phot.json")))
snr = json.load(open(os.path.join(T2, "p1_snr.json")))

# 旧 zp_inst / m5：按 input_light 基名匹配
def base(p):
    b = os.path.basename(p).replace("cleaned_", "").replace(".fts", "").replace("@", "_")
    return re.sub(r"-300S.*$", "", b)

old = {}
for fr in snr["frames"]:
    k = base(fr.get("input_light", ""))
    old[k] = (fr.get("snr_reference", {}).get("frame_zero_point_mag"), fr.get("frame_depth_m5_mag"))

rows = []
for fk, v in sorted(phot["photscale_detail"].items()):
    kp = v.get("k_photo")
    if not kp or kp <= 0: continue
    key = re.sub(r"-300S.*$", "", fk)
    zod, mod = old.get(key, (None, None))
    if zod is None: continue
    znew = C_ABS - 2.5 * math.log10(kp)
    mnew = znew - (zod - mod)
    m = re.search(r"M(\d+)_T\d_.*?-(\d{8})_", fk)
    rows.append(dict(frame=fk, grp="M" + m.group(1), night=m.group(2),
                     k=kp, zold=zod, znew=znew, mold=mod, mnew=mnew))

print(f"{'帧':<26} {'组':<4} {'夜':<9} {'k_photo':>13} {'旧zp_inst':>11} {'新zp_inst':>11}")
for r in rows:
    print(f"{r['frame'][:26]:<26} {r['grp']:<4} {r['night']:<9} {r['k']:>13.6g} {r['zold']:>11.5f} {r['znew']:>11.5f}")

zo = np.array([r["zold"] for r in rows]); zn = np.array([r["znew"] for r in rows])
mo = np.array([r["mold"] for r in rows]); mn = np.array([r["mnew"] for r in rows])
print(f"\n帧数 = {len(rows)}")
print(f"旧 zp_inst: 中位 {np.median(zo):.5f}  极差 {zo.max()-zo.min():.5f}  SD {zo.std(ddof=1):.5f}")
print(f"新 zp_inst: 中位 {np.median(zn):.5f}  极差 {zn.max()-zn.min():.5f}  SD {zn.std(ddof=1):.5f}")

def bygrp(vals, key):
    g = {}
    for r, v in zip(rows, vals): g.setdefault(r[key], []).append(v)
    return {k: np.array(v) for k, v in g.items()}

print("\n=== 跨指向组离散（组内中位）===")
go, gn = bygrp(zo, "grp"), bygrp(zn, "grp")
gm_o = np.array([np.median(v) for v in go.values()]); gm_n = np.array([np.median(v) for v in gn.values()])
for k in sorted(go):
    print(f"  {k}: 旧中位 {np.median(go[k]):.5f}   新中位 {np.median(gn[k]):.5f}")
print(f"  组中位的极差: 旧 {gm_o.max()-gm_o.min():.5f} mag  ->  新 {gm_n.max()-gm_n.min():.5f} mag")
print(f"  组中位 SD  : 旧 {gm_o.std(ddof=1):.5f}  ->  新 {gm_n.std(ddof=1):.5f}")

print("\n=== 跨天区权重配比 w ∝ 10^(-0.8·zp_inst)（以全体几何中位为 1）===")
wo = 10 ** (-0.8 * (zo - np.median(zo))); wn = 10 ** (-0.8 * (zn - np.median(zn)))
print(f"  权重相对极差: 旧 {wo.max()/wo.min():.5f}  ->  新 {wn.max()/wn.min():.5f}")
print(f"  ⇒ 跨天区权重偏差: 旧 {(wo.max()/wo.min()-1)*100:.2f}%  ->  新 {(wn.max()/wn.min()-1)*100:.2f}%")

print("\n=== m5 物理合理性硬门 ===")
print(f"  旧 m5: 中位 {np.median(mo):.5f}  范围 [{mo.min():.5f}, {mo.max():.5f}]")
print(f"  新 m5: 中位 {np.median(mn):.5f}  范围 [{mn.min():.5f}, {mn.max():.5f}]")
bad = np.abs(mn - np.median(mo)) > 1.0
print(f"  与基线中位偏离 >1 mag 的帧数: {int(bad.sum())} ⇒ {'通过' if bad.sum()==0 else '不通过'}")
print(f"  新 m5 − 旧 m5 的极差: {(mn-mo).max()-(mn-mo).min():.5f} mag")
json.dump(rows, open(os.path.join(ROOT, "实验/photometric-magnitude/results/p1fix/zpinst_planA.json"), "w"), indent=1, ensure_ascii=False)
