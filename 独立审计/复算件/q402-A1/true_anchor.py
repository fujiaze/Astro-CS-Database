#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""AUD-402 A1: 为 defaults.json 每个 field 在**被引文档**内找「真正陈述该值的行」。

规则：在被引文件全文里找同时含 (a) 值 token 或值的一个数字形态、(b) 键名片段/关键词
的行；输出候选行号与原文（截断）。用于把「锚点漂移到空行/标题/无关行」量化。
"""
import json
import re
import sys
from pathlib import Path

REPO = Path(r"F:/Astro dev/Astro CS Normalization Database")
OUT = Path(r"独立审计/复算件/q402-A1/true_anchor.tsv")

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

doc = json.loads((REPO / "eng/packaging/config/defaults.json").read_text(encoding="utf-8"))


def vtoks(v):
    if isinstance(v, bool):
        return ["true", "false", "1", "0"]
    if isinstance(v, (int, float)):
        out = {str(v)}
        if isinstance(v, float):
            out.add(("%r" % v))
            out.add(("%g" % v))
        return sorted(out)
    if isinstance(v, list):
        out = []
        for x in v:
            out += vtoks(x)
        return out
    if isinstance(v, str):
        return [v, v.lower(), v.upper()]
    return []


rows = []
for f in doc["fields"]:
    key = f["key"]
    leaf = key.split(".")[-1]
    val = f.get("value")
    sref = f.get("source_ref") or {}
    path = sref.get("path")
    if not path:
        m = re.search(r"(docs/[A-Za-z0-9_\-/.]+\.md)", f.get("source") or "")
        path = m.group(1) if m else None
    if not path or not (REPO / path).exists():
        rows.append((key, json.dumps(val, ensure_ascii=False), path or "-", "-", "<<no cited file>>"))
        continue
    lines = (REPO / path).read_text(encoding="utf-8", errors="replace").splitlines()
    toks = [t for t in vtoks(val) if len(t) > 1]
    hints = [leaf]
    hints += {
        "noise.patch_grid": ["patch grid"],
        "noise.clip_sigma": ["5σ", "clip"],
        "noise.max_clip_rounds": ["≤2 轮", "轮"],
        "noise.mask_budget_min_sky": ["N_sky", "9216"],
        "noise.mask_budget_min_patches": ["n_qualified", "8"],
        "noise.mask_r_min_px": ["r_min"],
        "noise.mask_fwhm_floor_scale": ["r_min", "FWHM"],
        "noise.source_mask_radius_px": ["rmax"],
        "noise.mask_radius_scale": ["rmax"],
        "rejection.sigma.lower_sigma": ["4.0/3.0/8", "sigma/winsorized"],
        "rejection.esd.alpha": ["alpha", "ESD"],
        "precision.default": ["FP64", "fp64"],
        "hips.tile_width": ["512"],
        "cosmetic.bad_column_sigma": ["bad_column_sigma", "列状"],
        "cosmetic.bad_column_variance_kappa": ["κ", "kappa"],
        "photometry.min_reference_stars": [">=3", "≥3"],
        "photometry.min_inlier_stars": [">=2", "≥2"],
        "photometry.mag_tolerance": ["3.0 mag", "mag_tolerance"],
        "photometry.tukey_c": ["4.685"],
        "detection.threshold_sigma": ["5.0·bgnoise", "5σ", "bgnoise"],
        "psf.moffat_beta": ["β=4", "beta", "Moffat"],
        "calibration.master_flat_median_range": ["0.5", "median"],
        "snr.path": ["snr_path"],
        "sparse_snr.spacing_px": ["spacing", "64"],
    }.get(key, [])
    best = []
    for i, ln in enumerate(lines, 1):
        score = 0
        if any(t in ln for t in toks):
            score += 2
        if any(h and h in ln for h in hints):
            score += 2
        if score >= 4:
            best.append((i, ln.strip()[:160]))
    rows.append((key, json.dumps(val, ensure_ascii=False), path,
                 len(best), " ;; ".join("%d:%s" % (i, t) for i, t in best[:3])))

with OUT.open("w", encoding="utf-8") as fh:
    fh.write("key\tvalue\tdoc\tcandidates\ttrue_anchor\n")
    for r in rows:
        fh.write("\t".join(str(x) for x in r) + "\n")
print(OUT)
