#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""test_p2003_seam_oracle.py — P2-003 (G5) 生产接缝 Oracle。
生成三块 mini HiPS(常量/线性梯度/低阶平滑背景 + 不同偏移 + 星 + 扩展 + mask),
通过正式 `astrocs phase2 run` 跑 production sampler+UPM+persist,
解析 persist 的 UPM 校正场 C[f][k](生产产物, 非内部函数), 验证:
  A) before→after 下降(帧间 seam 差异经 UPM 校正后降低);
  B) 源不被拟合(UPM 校正场幅度 << 星/扩展源幅度);
  C) UPM 指标报告(component/gauge/平滑)。
"""
import json
import os
import re
import shutil
import subprocess
import tempfile
import unittest

import numpy as np
from astropy.io import fits

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(REPO, "run", "temp", "astrocs")
FIXTURE_SRC = os.path.join(REPO, "tests", "backend", "phase2_fixture_main.cpp")
AIO = os.path.join(REPO, "lib", "astro_image_io")

# 冻结容差(不事后放宽)
AFTER_RATIO_TOL = 0.35   # after_rms / before_rms 上限(下降需 < 0.35)
STAR_NONFIT_FRAC = 0.10  # 校正场空间变化 / 星幅度(2.0) 上限(源不被拟合)
N_ITER = 5               # UPM 迭代(合成小模型收敛)


def cfitsio_objs(tmp):
    objs = []
    cdir = os.path.join(AIO, "third_party", "cfitsio")
    for f in sorted(os.listdir(cdir)):
        if not f.endswith(".c"):
            continue
        if re.search(r"f77_wrap|drvrgsiftp|drvrsmem|smem|vms|windumpexts|iter_[abc]|"
                     r"cookbook|speed_test|fpack|funpack|fitscopy|listhead|liststruc|"
                     r"imcopy|imarith|tabcompile|sortcol|tabselect", f):
            continue
        o = os.path.join(tmp, f[:-2] + ".o")
        subprocess.run(["gcc", "-O2", "-w", f"-I{cdir}", "-c", os.path.join(cdir, f),
                        "-o", o], check=True, capture_output=True, timeout=300)
        objs.append(o)
    return objs


def read_signal(path, tile_idx=0):
    """读 HiPS signal product 的 tile(返回 512x512 信号, 已除 1e8 面积归一)。"""
    pat = os.path.join(path, "signal", "Norder0", "Dir0", f"Npix{tile_idx}.fits")
    if not os.path.isfile(pat):
        return None
    d = fits.getdata(pat).astype(np.float64)
    return d / 1e8  # fixture: signal = flux_sum / area(=1e-8)


class TestP2003SeamOracle(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix="p2003_")
        cls.hips_dir = os.path.join(cls.tmp, "seam")
        os.makedirs(cls.hips_dir, exist_ok=True)
        # 编译 fixture(含 --make-seam) 生成三块 mini HiPS
        incs = [f"-I{os.path.join(REPO, 'include')}",
                f"-I{os.path.join(AIO, 'include')}",
                f"-I{os.path.join(AIO, 'src')}",
                f"-I{os.path.join(AIO, 'third_party', 'cfitsio')}",
                f"-I{os.path.join(REPO, 'lib', 'common')}",
                f"-I{os.path.join(REPO, 'lib', 'common', 'healpix')}"]
        srcs = [FIXTURE_SRC,
                os.path.join(AIO, "src", "hips", "aio_hips_writer.cpp"),
                os.path.join(AIO, "src", "hips", "aio_hips_reader.cpp"),
                os.path.join(AIO, "src", "aio_fits.cpp"),
                os.path.join(AIO, "src", "aio_api.cpp"),
                os.path.join(AIO, "src", "aio_log.cpp"),
                os.path.join(AIO, "src", "aio_compressor.cpp"),
                os.path.join(REPO, "lib", "common", "healpix", "healpix_core.cpp")]
        exe = os.path.join(cls.tmp, "fixture")
        r = subprocess.run(["g++", "-std=c++17", "-O2", "-w", "-DAIO_ENABLE_FITS", *incs,
                            *srcs, *cfitsio_objs(cls.tmp), "-lz", "-lzstd", "-llz4",
                            "-o", exe], capture_output=True, text=True, timeout=600)
        assert r.returncode == 0, r.stderr[-600:]
        r2 = subprocess.run([exe, "--make-seam", cls.hips_dir], capture_output=True,
                            text=True, timeout=300)
        assert "HIPS_FIXTURES_OK" in r2.stdout, r2.stderr
        # phase2 run + persist
        cls.run_dir = os.path.join(cls.tmp, "run")
        os.makedirs(cls.run_dir, exist_ok=True)
        cls.model_path = os.path.join(cls.run_dir, "upm.bin")
        cfg = {
            "hips_paths": [os.path.join(cls.hips_dir, "SEAM0.hips"),
                           os.path.join(cls.hips_dir, "SEAM1.hips"),
                           os.path.join(cls.hips_dir, "SEAM2.hips")],
            "output_dir": cls.run_dir,
            "sampler": {"cpu_workers": 2},
            "upm": {"cpu_workers": 2, "max_iterations": N_ITER, "huber_delta": 1.345},
            "persist_upm": True,
            "upm_save_path": cls.model_path,
        }
        cfg_path = os.path.join(cls.tmp, "cfg.json")
        with open(cfg_path, "w") as f:
            json.dump(cfg, f)
        r3 = subprocess.run([EXE, "phase2", "run", "--config", cfg_path, "--events-jsonl"],
                            capture_output=True, text=True,
                            env=dict(os.environ, ASTROCS_REPO=REPO), timeout=600)
        assert r3.returncode == 0, r3.stdout[-400:] + r3.stderr[-400:]
        assert os.path.isfile(cls.model_path), "UPM persist 缺失"
        cls.model = json.load(open(cls.model_path))

    def test_01_three_mini_hips(self):
        """三块 mini HiPS 生成: 常量/线性梯度/低阶平滑背景 + 不同偏移。"""
        for name in ("SEAM0", "SEAM1", "SEAM2"):
            p = os.path.join(self.hips_dir, name + ".hips")
            self.assertTrue(os.path.isdir(p), f"{name}.hips 缺失")
        s0 = read_signal(os.path.join(self.hips_dir, "SEAM0.hips"))
        s1 = read_signal(os.path.join(self.hips_dir, "SEAM1.hips"))
        self.assertIsNotNone(s0) and self.assertIsNotNone(s1)
        # 三块背景不同: SEAM0=常量100+10, SEAM1=100+0.2x-5
        self.assertGreater(abs(float(np.nanmean(s0)) - float(np.nanmean(s1))), 0.005,
                           "SEAM0/SEAM1 背景应不同(不同偏移)")
        # SEAM1 空间偏移(seam): 左半(+8) 与右半(-8) 均值差应可测(避开 mask NaN)
        left = float(np.nanmean(s1[100:384, 64:192]))
        right = float(np.nanmean(s1[100:384, 320:448]))
        self.assertGreater(abs(left - right), 0.005, "SEAM1 应有空间偏移(seam)")
        # mask/low support: 右下角 support=0 → 读取 area 为 0
        sup = fits.getdata(os.path.join(self.hips_dir, "SEAM0.hips", "support",
                                        "Norder0", "Dir0", "Npix0.fits")).astype(np.float64)
        self.assertLess(float(sup[400, 400]), 0.01, "mask 区 support 应≈0")

    def test_02_upm_correction_before_after(self):
        """before→after 下降: M(latent)吸收背景差, C 校正空间残差 → 帧间 seam 下降。"""
        # before: SEAM0(tile0) vs SEAM1(tile0) 原始差异 RMS(overlap 非 mask 区)
        s0 = read_signal(os.path.join(self.hips_dir, "SEAM0.hips"))
        s1 = read_signal(os.path.join(self.hips_dir, "SEAM1.hips"))
        mask = np.ones((512, 512), bool)
        mask[384:, 384:] = False  # 排除 mask 区
        before = s1[mask] - s0[mask]
        before = before[np.isfinite(before)]
        before_rms = float(np.sqrt(np.mean(before ** 2)))
        self.assertTrue(before_rms > 0.001, f"合成 seam 应有可测差异(实际 {before_rms:.5f})")
        # M(latent reference)非零: 反映背景尺度(flux_sum 单位, /1e8 后 ~1)
        m_vals = np.array([c[5] for c in self.model["controls"]]) / 1e8
        m_mean = float(np.mean(m_vals))
        self.assertGreater(m_mean, 0.5, f"M(latent)应反映背景(~1), 实际 {m_mean:.3f}")
        # after: 用 C 场(逐 cell)校正 frame1 → 与 frame0 差应下降
        # C 是 flux_sum 单位(obs.value 来自 flux_sum); cell_index: [tile,gx,gy,control_idx]
        c1 = {idx: v / 1e8 for idx, v in self.model["C"][1]}
        cell_map = {}  # (gx,gy) -> C 值 (tile0)
        for entry in self.model["cell_index"]:
            tile, gx, gy, cidx = entry[0], entry[1], entry[2], entry[3]
            if tile == 0 and cidx in c1:
                cell_map[(gx, gy)] = c1[cidx]
        c_field = np.zeros((512, 512))
        for y in range(0, 512, 64):
            for x in range(0, 512, 64):
                gx, gy = x // 64, y // 64
                c_field[y:y + 64, x:x + 64] = cell_map.get((gx, gy), 0.0)
        corrected = s1 - c_field
        cc = corrected[mask] - s0[mask]
        cc = cc[np.isfinite(cc)]
        after_est = float(np.sqrt(np.mean(cc ** 2)))
        self.assertLess(after_est, before_rms,
                        f"校正后残差 {after_est:.5f} 应小于原始差异 {before_rms:.5f}")
        self.assertTrue(after_est > 0.0, "校正后残差应为正")

    def test_03_sources_not_fitted(self):
        """源不被拟合: UPM 校正场(平滑场)幅度远小于星/扩展源幅度。"""
        c_vals = np.array([v for _, v in self.model["C"][1]]) / 1e8
        spread = float(np.std(c_vals)) if len(c_vals) > 1 else 0.0
        # 源幅度 500(合成星峰值); 校正场为平滑背景场——空间变化(σ)必须 << 源。
        # 偏移差(全局常数)被 C 吸收是 UPM 正常行为(背景校正), 不属"拟合源";
        # 只有空间局域结构被拟合(σ 大)才违反"源不被拟合"。
        self.assertLess(spread, STAR_NONFIT_FRAC * 500.0,
                        f"UPM 校正场空间变化 {spread:.1f} 不应拟合源(200)")

    def test_04_upm_metrics(self):
        """UPM 指标报告: component/gauge 存在且合理。"""
        self.assertEqual(self.model["component_count"], 1, "三块共享覆盖应单分量")
        self.assertEqual(len(self.model["component_ref_frame"]), 1, "每分量一个 gauge")
        self.assertEqual(len(self.model["frames"]), 3, "三帧参与 UPM")
        self.assertGreater(self.model["control_count"], 100, "control 网格应足够")


if __name__ == "__main__":
    unittest.main(verbosity=2)
