#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
D-001 三片 HISS 球面重合与光度一致性验证

验证内容:
  1. 三片 nside/nested 一致 (球面网格统一)
  2. ipix 球面坐标覆盖范围 (三片相邻/重叠)
  3. ipix 交集 (真实球面信号重合, 非元数据比较)
  4. 重叠区 signal 比较 + 光度尺度比值统计
  5. WCS CD 矩阵镜像检查 (det 符号 = 手性; CD00/CD11 符号 = 旋转角度)
  6. 输出 overlap_analysis.png 可视化 + overlap_stats.json
"""
from __future__ import annotations
import os
import sys
import json

PROJECT = r'f:\Astro dev\Astro CS Normalization Database'
os.chdir(PROJECT)
sys.path.insert(0, os.path.join(PROJECT, 'lib', 'astro_image_io', 'python'))

from hiss_v2 import v1_read_snr_model
import numpy as np
from astropy_healpix import HEALPix
from astropy import units as u
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

PANELS = ['panel1', 'panel2', 'panel3']
EVID = os.path.join(PROJECT, 'engineering_authoritative', 'evidence', 'D-001')
OUT_PNG = os.path.join(EVID, 'overlap_analysis.png')
OUT_JSON = os.path.join(EVID, 'overlap_stats.json')

print('=' * 70)
print('D-001 三片 HISS 球面重合与光度一致性验证')
print('=' * 70)

# ---------------------------------------------------------------------------
# 1. 读取三片 HISS (V1 格式), 提取 WCS + ipix + signal
# ---------------------------------------------------------------------------
data = {}
for p in PANELS:
    path = os.path.join(PROJECT, 'output', 'D-001', f'T4_RED_GalaxyCenter_{p}.hiss')
    nside, nested, ipix, pixel, meta, snr = v1_read_snr_model(path)
    wcs = meta['wcs']
    cd = np.array(wcs['cd'], dtype=np.float64).reshape(2, 2)
    crval = np.array(wcs['crval'], dtype=np.float64)
    crpix = np.array(wcs['crpix'], dtype=np.float64)
    hp = HEALPix(nside=nside, order='nested' if nested else 'ring')
    lon, lat = hp.healpix_to_lonlat(ipix.astype(np.int64))
    ra = np.asarray(lon.to(u.deg).value, dtype=np.float64)
    dec = np.asarray(lat.to(u.deg).value, dtype=np.float64)
    det_cd = float(np.linalg.det(cd))
    pixscale = float(np.sqrt(np.abs(det_cd)) * 3600.0)  # arcsec/px
    data[p] = dict(
        path=path, nside=int(nside), nested=bool(nested),
        ipix=ipix, signal=pixel.astype(np.float64), meta=meta, wcs=wcs,
        cd=cd, crval=crval, crpix=crpix, ra=ra, dec=dec,
        det_cd=det_cd, pixscale=pixscale, n_pix=int(len(ipix)),
    )
    print(f'\n[{p}] {os.path.basename(path)}')
    print(f'  nside={nside} nested={nested} n_pix={len(ipix)} '
          f'filter={meta.get("filter")} exp={meta.get("exposure_s")}s '
          f'obs={meta.get("obs_time")}')
    print(f'  CRVAL=({crval[0]:.6f}, {crval[1]:.6f})  CRPIX=({crpix[0]:.1f}, {crpix[1]:.1f})')
    print(f'  CD=[[{cd[0,0]:.6e}, {cd[0,1]:.6e}], [{cd[1,0]:.6e}, {cd[1,1]:.6e}]]')
    print(f'  det(CD)={det_cd:.6e}  pixscale={pixscale:.4f}"  sip_order={wcs.get("sip_order")}')
    print(f'  RA  range = [{ra.min():.4f}, {ra.max():.4f}] deg  (span {ra.max()-ra.min():.4f})')
    print(f'  Dec range = [{dec.min():.4f}, {dec.max():.4f}] deg  (span {dec.max()-dec.min():.4f})')
    print(f'  signal: min={pixel.min():.4f} max={pixel.max():.4f} '
          f'median={np.median(pixel):.4f} mean={np.mean(pixel):.4f}')

# ---------------------------------------------------------------------------
# 2. 球面网格一致性 (nside / nested 必须一致才能在相同 ipix 空间比较)
# ---------------------------------------------------------------------------
print('\n' + '=' * 70)
print('2. 球面网格一致性检查')
print('=' * 70)
nsides = set(data[p]['nside'] for p in PANELS)
nested_set = set(data[p]['nested'] for p in PANELS)
grid_ok = (len(nsides) == 1) and (len(nested_set) == 1)
print(f'  nside  set = {nsides}  -> consistent: {len(nsides) == 1}')
print(f'  nested set = {nested_set} -> consistent: {len(nested_set) == 1}')
print(f'  >>> grid unified: {"PASS" if grid_ok else "FAIL"}')

# ---------------------------------------------------------------------------
# 3. ipix 球面交集 (真实信号重合, 不是元数据)
# ---------------------------------------------------------------------------
print('\n' + '=' * 70)
print('3. 球面 ipix 交集 (真实覆盖重合)')
print('=' * 70)
ipix_sets = {p: set(data[p]['ipix'].tolist()) for p in PANELS}
for p in PANELS:
    print(f'  {p}: {len(ipix_sets[p])} HEALPix pixels (nside={data[p]["nside"]})')

pair_names = [('panel1', 'panel2'), ('panel1', 'panel3'), ('panel2', 'panel3')]
overlaps = {}
for a, b in pair_names:
    inter = ipix_sets[a] & ipix_sets[b]
    overlaps[f'{a}_vs_{b}'] = inter
    pct_a = 100.0 * len(inter) / len(ipix_sets[a]) if ipix_sets[a] else 0
    pct_b = 100.0 * len(inter) / len(ipix_sets[b]) if ipix_sets[b] else 0
    print(f'  {a} n {b}: {len(inter)} ipix  ({pct_a:.2f}% of {a}, {pct_b:.2f}% of {b})')
triple = ipix_sets['panel1'] & ipix_sets['panel2'] & ipix_sets['panel3']
print(f'  triple intersection: {len(triple)} ipix')

# ---------------------------------------------------------------------------
# 4. 重叠区 signal 比较 + 光度尺度比值
# ---------------------------------------------------------------------------
print('\n' + '=' * 70)
print('4. 重叠区 signal 比较 + 光度尺度比值统计')
print('=' * 70)

sig_map = {p: dict(zip(data[p]['ipix'].tolist(), data[p]['signal'].tolist())) for p in PANELS}

pair_stats = {}
for a, b in pair_names:
    key = f'{a}_vs_{b}'
    inter = overlaps[key]
    if len(inter) == 0:
        pair_stats[key] = None
        print(f'  {key}: no overlap, skip')
        continue
    sa = np.array([sig_map[a][i] for i in inter], dtype=np.float64)
    sb = np.array([sig_map[b][i] for i in inter], dtype=np.float64)
    mask = (sa > 0) & (sb > 0)
    sa_v, sb_v = sa[mask], sb[mask]
    ratio = sa_v / sb_v
    med_r = float(np.median(ratio))
    mad_r = float(np.median(np.abs(ratio - med_r)))
    mean_r = float(np.mean(ratio))
    std_r = float(np.std(ratio))
    p16, p84 = float(np.percentile(ratio, 16)), float(np.percentile(ratio, 84))
    corr = float(np.corrcoef(sa_v, sb_v)[0, 1])
    pair_stats[key] = dict(
        n_overlap=int(len(inter)), n_valid=int(mask.sum()),
        ratio_median=med_r, ratio_mad=mad_r, ratio_mean=mean_r, ratio_std=std_r,
        ratio_p16=p16, ratio_p84=p84,
        signal_a_median=float(np.median(sa_v)), signal_b_median=float(np.median(sb_v)),
        correlation=corr,
        sa=sa_v.tolist(), sb=sb_v.tolist(), ratio=ratio.tolist(),
    )
    print(f'  {key}: n_overlap={len(inter)} n_valid={mask.sum()}')
    print(f'    ratio median={med_r:.4f}  MAD={mad_r:.4f}  mean={mean_r:.4f}  std={std_r:.4f}')
    print(f'    ratio p16={p16:.4f}  p84={p84:.4f}  (scatter {p84-p16:.4f})')
    print(f'    Pearson corr={corr:.4f}')
    print(f'    signal median: {a}={np.median(sa_v):.4f}  {b}={np.median(sb_v):.4f}')

print('\n  光度尺度稳定性判定 (已 photometric 校准, 比值应接近 1.0):')
photo_ok = True
for key, st in pair_stats.items():
    if st is None:
        continue
    med = st['ratio_median']
    scatter = st['ratio_p84'] - st['ratio_p16']
    ok = (abs(med - 1.0) < 0.15) and (scatter < 0.5)
    photo_ok = photo_ok and ok
    print(f'    {key}: median={med:.4f} scatter={scatter:.4f} -> {"PASS" if ok else "CHECK"}')
print(f'  >>> photometric scale stable: {"PASS" if photo_ok else "CHECK"}')

# ---------------------------------------------------------------------------
# 5. WCS 镜像检查 (CD 矩阵)
#    核心判据: det(CD) 符号一致 = 同手性 (无镜像).
#    CD00/CD11 符号差异 = 旋转角度差异 (刚体旋转, 非镜像).
# ---------------------------------------------------------------------------
print('\n' + '=' * 70)
print('5. WCS 镜像检查 (CD 矩阵)')
print('=' * 70)
det_signs = {p: (1 if data[p]['det_cd'] > 0 else -1) for p in PANELS}
det_consistent = len(set(det_signs.values())) == 1
print(f'  det(CD) sign: {det_signs}  -> consistent: {det_consistent}')
for p in PANELS:
    print(f'    {p}: det(CD)={data[p]["det_cd"]:.6e}  sign={det_signs[p]}  '
          f'pixscale={data[p]["pixscale"]:.4f}"  '
          f'CD00={data[p]["cd"][0,0]:.6e}(ra)  CD11={data[p]["cd"][1,1]:.6e}(dec)')
pixscales = [data[p]['pixscale'] for p in PANELS]
ps_min, ps_max = min(pixscales), max(pixscales)
ps_rel = (ps_max - ps_min) / np.mean(pixscales)
print(f'  pixscale: min={ps_min:.4f}" max={ps_max:.4f}" relative_diff={ps_rel:.4f} '
      f'({"PASS" if ps_rel < 0.02 else "CHECK"})')
cd00_signs = {p: (1 if data[p]['cd'][0, 0] > 0 else -1) for p in PANELS}
cd11_signs = {p: (1 if data[p]['cd'][1, 1] > 0 else -1) for p in PANELS}
cd00_consistent = len(set(cd00_signs.values())) == 1
cd11_consistent = len(set(cd11_signs.values())) == 1
print(f'  CD[0,0] (ra dir) sign: {cd00_signs} -> consistent: {cd00_consistent}')
print(f'  CD[1,1] (dec dir) sign: {cd11_signs} -> consistent: {cd11_consistent}')
# 旋转分组: (CD00,CD11) 符号组合相同的为同旋转组
rotation_groups = {}
for p in PANELS:
    sig = (cd00_signs[p], cd11_signs[p])
    rotation_groups.setdefault(sig, []).append(p)
print(f'  rotation groups (CD00,CD11 sign): {rotation_groups}')
rotation_diff = len(rotation_groups) > 1
if rotation_diff:
    print(f'  NOTE: panels differ by camera rotation (CD00/CD11 sign flip).')
    print(f'        This is a 180 deg rotation (rigid body), NOT a mirror.')
    print(f'        det(CD) consistent confirms identical chirality (no mirror).')
    print(f'        Drizzle reprojects to sphere regardless of camera angle,')
    print(f'        so spherical signal overlap remains valid (see sec 4).')
# 镜像检查核心: det(CD) 符号一致 = 无镜像. 旋转差异是正常拍摄方向.
mirror_ok = det_consistent and (ps_rel < 0.02)
print(f'  >>> no mirror (det chirality consistent): {"PASS" if mirror_ok else "FAIL"}')
print(f'  >>> rotation difference present: {rotation_diff} (acceptable, not a mirror)')

# ---------------------------------------------------------------------------
# 6. 可视化 overlap_analysis.png (英文标签避免字体问题)
# ---------------------------------------------------------------------------
print('\n' + '=' * 70)
print('6. 生成可视化 overlap_analysis.png')
print('=' * 70)
fig, axes = plt.subplots(2, 2, figsize=(15, 12))
colors = {'panel1': '#e41a1c', 'panel2': '#377eb8', 'panel3': '#4daf4a'}

# 子图1: 球面位置 (RA-Dec 散点, 三片 + 重叠区高亮)
ax = axes[0, 0]
for p in PANELS:
    ax.scatter(data[p]['ra'], data[p]['dec'], s=2, alpha=0.25,
               c=colors[p], label=f'{p} ({data[p]["n_pix"]} px)')
if len(triple) > 0:
    hp_ref = HEALPix(nside=data['panel1']['nside'], order='nested')
    lon_t, lat_t = hp_ref.healpix_to_lonlat(np.array(sorted(triple), dtype=np.int64))
    ax.scatter(np.asarray(lon_t.to(u.deg).value), np.asarray(lat_t.to(u.deg).value),
               s=18, c='black', marker='x', label=f'triple overlap ({len(triple)} px)', zorder=5)
# 标注 pair overlap
for key, st in pair_stats.items():
    if st is None:
        continue
    a, b = key.split('_vs_')
    inter = overlaps[key]
    hp_ref = HEALPix(nside=data[a]['nside'], order='nested')
    lon_o, lat_o = hp_ref.healpix_to_lonlat(np.array(sorted(inter), dtype=np.int64))
    ax.scatter(np.asarray(lon_o.to(u.deg).value), np.asarray(lat_o.to(u.deg).value),
               s=6, c='gold', marker='s', alpha=0.6,
               label=f'{a} n {b} ({len(inter)} px)', zorder=4)
ax.set_xlabel('RA (deg)')
ax.set_ylabel('Dec (deg)')
ax.set_title('Spherical position (HEALPix ipix -> RA/Dec, nside=512 nested)')
ax.legend(loc='best', fontsize=7)
ax.grid(True, alpha=0.3)

# 子图2: 重叠区 signal 散点对比
ax = axes[0, 1]
for key, st in pair_stats.items():
    if st is None:
        continue
    a, b = key.split('_vs_')
    ax.scatter(st['sb'], st['sa'], s=4, alpha=0.4, label=f'{a} vs {b} (n={st["n_valid"]}, r={st["ratio_median"]:.3f})')
all_vals = []
for st in pair_stats.values():
    if st is None:
        continue
    all_vals.extend(st['sa'])
    all_vals.extend(st['sb'])
if all_vals:
    lo, hi = float(np.min(all_vals)), float(np.max(all_vals))
    ax.plot([lo, hi], [lo, hi], 'k--', alpha=0.5, label='1:1')
ax.set_xlabel('signal B')
ax.set_ylabel('signal A')
ax.set_title('Overlap region signal scatter')
ax.legend(loc='best', fontsize=8)
ax.grid(True, alpha=0.3)

# 子图3: 光度比值直方图
ax = axes[1, 0]
for key, st in pair_stats.items():
    if st is None:
        continue
    rclip = np.clip(st['ratio'], 0, 3)  # clip for visibility
    ax.hist(rclip, bins=40, alpha=0.5, label=f'{key} (med={st["ratio_median"]:.3f})')
ax.axvline(1.0, color='k', linestyle='--', alpha=0.7, label='ratio=1.0')
ax.set_xlabel('signal ratio (A/B) [clipped to 3]')
ax.set_ylabel('count')
ax.set_title('Photometric ratio distribution (overlap)')
ax.legend(loc='best', fontsize=8)
ax.grid(True, alpha=0.3)

# 子图4: 三片球面覆盖矩形
ax = axes[1, 1]
for p in PANELS:
    ra_min, ra_max = data[p]['ra'].min(), data[p]['ra'].max()
    dec_min, dec_max = data[p]['dec'].min(), data[p]['dec'].max()
    rect = plt.Rectangle((ra_min, dec_min), ra_max - ra_min, dec_max - dec_min,
                          linewidth=2, edgecolor=colors[p], facecolor=colors[p],
                          alpha=0.2, label=f'{p} coverage')
    ax.add_patch(rect)
    ax.scatter(data[p]['crval'][0], data[p]['crval'][1], marker='+',
               c=colors[p], s=100, zorder=5)
ax.set_xlabel('RA (deg)')
ax.set_ylabel('Dec (deg)')
ax.set_title('Spherical coverage (+ = CRVAL center)')
ax.legend(loc='best', fontsize=8)
ax.grid(True, alpha=0.3)
ax.set_aspect('equal', adjustable='datalim')

fig.suptitle('D-001: GalaxyCenter 3-panel Red HISS spherical overlap & photometric check (nside=512 nested)',
             fontsize=13, fontweight='bold')
fig.tight_layout(rect=[0, 0, 1, 0.97])
fig.savefig(OUT_PNG, dpi=150)
print(f'  saved: {OUT_PNG}')
plt.close(fig)

# ---------------------------------------------------------------------------
# 7. 汇总 JSON
# ---------------------------------------------------------------------------
total_overlap = sum(len(v) for v in overlaps.values())
summary = dict(
    task='D-001',
    panels={p: dict(
        nside=data[p]['nside'], nested=data[p]['nested'], n_pix=data[p]['n_pix'],
        crval=data[p]['crval'].tolist(), crpix=data[p]['crpix'].tolist(),
        cd=data[p]['cd'].tolist(), det_cd=data[p]['det_cd'], pixscale_arcsec=data[p]['pixscale'],
        ra_range=[float(data[p]['ra'].min()), float(data[p]['ra'].max())],
        dec_range=[float(data[p]['dec'].min()), float(data[p]['dec'].max())],
        signal_median=float(np.median(data[p]['signal'])),
        filter=data[p]['meta'].get('filter'), exposure_s=data[p]['meta'].get('exposure_s'),
        obs_time=data[p]['meta'].get('obs_time'),
        cd00_sign=cd00_signs[p], cd11_sign=cd11_signs[p],
    ) for p in PANELS},
    grid_consistent=grid_ok,
    overlaps={k: len(v) for k, v in overlaps.items()},
    triple_overlap=len(triple),
    pair_stats={k: ({kk: vv for kk, vv in v.items() if kk not in ('sa', 'sb', 'ratio')}
                    if v else None) for k, v in pair_stats.items()},
    photometric_stable=photo_ok,
    mirror_check=dict(
        det_signs=det_signs, det_consistent=det_consistent,
        cd00_signs=cd00_signs, cd00_consistent=cd00_consistent,
        cd11_signs=cd11_signs, cd11_consistent=cd11_consistent,
        rotation_groups={str(k): v for k, v in rotation_groups.items()},
        rotation_difference=rotation_diff,
        pixscale_relative_diff=float(ps_rel),
        mirror_ok=mirror_ok,
        note='det(CD) consistent = same chirality (no mirror). CD00/CD11 sign '
             'difference = 180 deg camera rotation (rigid body, not mirror).',
    ),
    overall_pass=grid_ok and photo_ok and mirror_ok and (total_overlap > 0),
)
def _json_default(o):
    if isinstance(o, np.bool_):
        return bool(o)
    if isinstance(o, np.integer):
        return int(o)
    if isinstance(o, np.floating):
        return float(o)
    return str(o)

with open(OUT_JSON, 'w', encoding='utf-8') as f:
    json.dump(summary, f, indent=2, ensure_ascii=False, default=_json_default)
print(f'  saved: {OUT_JSON}')

print('\n' + '=' * 70)
print(f'总结: grid={grid_ok}  photo={photo_ok}  mirror(no)={mirror_ok}  '
      f'rotation_diff={rotation_diff}  overlap>0={total_overlap>0}')
print(f'OVERALL: {"PASS" if summary["overall_pass"] else "CHECK"}')
print('=' * 70)
