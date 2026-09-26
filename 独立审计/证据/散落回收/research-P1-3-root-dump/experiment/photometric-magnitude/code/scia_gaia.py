# -*- coding: utf-8 -*-
"""SCI-A · Gaia DR3SP XP 谱读取（本地星表，零网络）

星表读取走仓内 gaia_xpsd_client（编译 code/gaia_xp_dump.c），本模块只解析其 CSV。
星表来源：仓库根 gaia/GaiaDR3SP/gdr3sp-1.0.0-*.xpsd（PixInsight XPSD 格式，Gaia DR3 BP/RP
平均谱，343 点 336–1020 nm @2 nm，uint8 + flux_min/flux_mul 线性标定）。
"""
from __future__ import annotations

import os
import subprocess

import numpy as np

from scia_common import CACHE, REPO, RUN, sha256_file

DUMP_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "gaia_xp_dump.c")
DUMP_BIN = os.path.join(RUN, "bin", "gaia_xp_dump")
XPSD_DIR = "GaiaDR3SP"


def build_dumper(force=False):
    os.makedirs(os.path.dirname(DUMP_BIN), exist_ok=True)
    src = os.path.join(REPO, "lib", "infrastructure", "gaia_xpsd_client", "src", "gaia_client.c")
    if force or not os.path.exists(DUMP_BIN) or os.path.getmtime(DUMP_BIN) < os.path.getmtime(DUMP_SRC):
        cmd = ["gcc", "-O2", "-std=gnu11", "-w",
               "-I", os.path.join(REPO, "lib", "infrastructure", "gaia_xpsd_client", "src"),
               DUMP_SRC, src, "-lz", "-lm", "-lpthread", "-o", DUMP_BIN]
        print("[gaia] " + " ".join(cmd))
        subprocess.run(cmd, check=True, cwd=REPO)
    return DUMP_BIN


def dump_cone(ra, dec, radius_deg, mag_high, tag, force=False):
    """导出锥内 XP 谱到 CACHE/<tag>.csv（缓存命中则复用）。"""
    build_dumper()
    out = os.path.join(CACHE, f"gaia_xp_{tag}.csv")
    if force or not os.path.exists(out):
        cmd = [DUMP_BIN, XPSD_DIR, f"{ra:.6f}", f"{dec:.6f}", f"{radius_deg:.6f}",
               f"{mag_high:.3f}", out]
        print("[gaia] " + " ".join(cmd))
        subprocess.run(cmd, check=True, cwd=REPO)
    return out


class XpCatalog:
    """锥内 XP 星表：ra/dec/magG + 采样谱（W m^-2 nm^-1）。"""

    def __init__(self, csv_path):
        with open(csv_path, encoding="utf-8") as f:
            head = f.readline().strip()
        meta = dict(kv.split("=") for kv in head.lstrip("#").split())
        self.start_nm = float(meta["start_nm"]); self.step_nm = float(meta["step_nm"])
        self.n_spec = int(meta["count"]); self.n_src = int(meta["n"])
        self.wl_nm = self.start_nm + self.step_nm * np.arange(self.n_spec)
        d = np.loadtxt(csv_path, delimiter=",", skiprows=2)  # 行1=#meta, 行2=列名
        if d.ndim == 1:
            d = d[None, :]
        self.ra = d[:, 0]; self.dec = d[:, 1]; self.magG = d[:, 2]
        self.flux_min = d[:, 3]; self.flux_mul = d[:, 4]
        self.bytes = d[:, 5:5 + self.n_spec].astype(float)
        self.csv = csv_path
        self.sha256 = sha256_file(csv_path)

    def spectrum(self, i):
        """F(λ) = byte*flux_mul + flux_min  [W m^-2 nm^-1]；负值截到 0（线性解码在零附近的下溢）。"""
        s = self.bytes[i] * self.flux_mul[i] + self.flux_min[i]
        return np.clip(s, 0.0, None)

    def mag_to_flux_scale(self, i, mag_g):
        """把第 i 颗星的谱归一到给定 G 星等：10^(-0.4(mag_g - magG_i))。"""
        return 10.0 ** (-0.4 * (mag_g - self.magG[i]))

    def summary(self):
        return dict(csv=os.path.relpath(self.csv, REPO), sha256=self.sha256,
                    n_src=self.n_src, grid=f"{self.start_nm:.0f}..{self.wl_nm[-1]:.0f} nm "
                                           f"@{self.step_nm:.0f} nm x{self.n_spec}",
                    magG_min=float(self.magG.min()), magG_max=float(self.magG.max()))
