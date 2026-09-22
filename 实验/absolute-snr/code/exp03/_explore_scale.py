#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""EXP-03 探路：误差-尺度关系的形状（先看清楚再定稿）。"""
import sys, math
from pathlib import Path
import numpy as np
HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import exp03_common as E

rng = np.random.default_rng(E.SEED)
SHAPE = (512, 512)
SIG = 20.0
BOXES = (16, 32, 64, 128, 256)

def scan(struct, tag):
    print("### %s   struct_rms=%.3f" % (tag, float(np.std(struct))))
    print("  %5s %10s %10s %10s %10s %10s" % ("box", "R0_bias", "R1_bias", "R2_bias", "R0_sd", "R1_sd"))
    for B in BOXES:
        r0, r1, r2 = [], [], []
        for k in range(4):
            n1 = rng.normal(0.0, SIG, size=SHAPE)
            n2 = rng.normal(0.0, SIG, size=SHAPE)
            f1 = struct + n1
            f2 = struct + n2
            a = E.region_sigma_raw(f1, B)["sigma"]
            b = E.region_sigma_resid(f1, B, n_iter=3)["sigma"]
            c = E.region_sigma_diff(f1, f2, B)["sigma"]
            r0.append(np.median(a)); r1.append(np.median(b)); r2.append(np.median(c))
        r0 = np.array(r0); r1 = np.array(r1); r2 = np.array(r2)
        print("  %5d %9.2f%% %9.2f%% %9.2f%% %9.4f %9.4f" % (
            B, 100*(r0.mean()/SIG-1), 100*(r1.mean()/SIG-1), 100*(r2.mean()/SIG-1),
            r0.std()/SIG, r1.std()/SIG))

scan(np.zeros(SHAPE), "no structure (null)")
for slope in (0.325, 3.25):
    scan(E.struct_ramp(SHAPE, slope), "ramp slope=%.3f" % slope)
for ell in (8.0, 32.0, 128.0):
    scan(E.struct_smooth_field(SHAPE, 3*SIG, ell, np.random.default_rng(7)), "corr ell=%.0f rms=3sig" % ell)
for sb in (8.0, 32.0):
    scan(E.struct_blob(SHAPE, 60*SIG, sb), "blob sigma=%.0f amp=60sig" % sb)
