# -*- coding: utf-8 -*-
"""AUD-402 / BD1 数值复算件（本文所有"本机复算"由此产生；输出存 numerics.txt）。
只读仓库、不 import 仓库内任何模块。"""
import math, statistics, sys
sys.stdout.reconfigure(encoding="utf-8")

print("Gauss  2*sqrt(2*ln2)     =", repr(2 * math.sqrt(2 * math.log(2))),
      "| 字面量 2.3548200450309493 二进制相等:",
      2 * math.sqrt(2 * math.log(2)) == 2.3548200450309493)

moff_exact = 2 * math.sqrt(2) * math.sqrt(2 ** 0.25 - 1)          # STAR_PSF_ALGORITHMS.md:62
print("Moffat4 2*sqrt(2)*sqrt(2**(1/4)-1) =", repr(moff_exact),
      "| 字面量 1.230310 相对差:", (1.230310 - moff_exact) / moff_exact)

deg = lambda n: 180 / math.pi * math.sqrt(math.pi / 3) / n          # IVOA REC-HIPS-1.0 §4.4.1
print("nside=512:", repr(deg(512)), "deg |", repr(deg(512) * 3600), "arcsec (旧缺陷值 412.258369)")
print("nside=1  :", repr(deg(1)), "deg (gen_geometry_truth.py:177 写的 58.6)")
print("nside=512(leaf_order=0) 应为:", repr(deg(1 << 9)), "deg；实际写 58.6 ⇒ 差", deg(1) / deg(512), "倍")

print("1/3600 =", repr(1 / 3600), "kPxScaleDeg 相等:", 1 / 3600 == 0.0002777777777777778)
print("sqrt(pi/2) =", repr(math.sqrt(math.pi / 2)),
      "| 代码常数 1.253 相对偏差:", (1.253 - math.sqrt(math.pi / 2)) / math.sqrt(math.pi / 2))
print("gain_variance 期望 1000/2 + 25/4 =", 1000 / 2 + 25 / 4)

snr = [10.01518830728662, 16.95958883413694, 16.04022427426992, 37.36271474205877, 15.728210630590322]
flux = [2984.19140625, 11900.705322265625, 11581.213134765625, 30877.013671875, 11209.939697265625]
print("median(kReal[].snr_optimal) =", statistics.median(snr),
      "== kOracleMedian:", statistics.median(snr) == 16.04022427426992)
print("median(kReal[].flux)        =", statistics.median(flux),
      "== kGroupRefFlux:", statistics.median(flux) == 11581.213134765625)
